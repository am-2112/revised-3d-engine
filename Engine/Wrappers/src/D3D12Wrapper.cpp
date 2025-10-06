#pragma once

#include "D3D12Wrapper.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgidebug.h>

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;

//creates device and sets it up with swapchain, command lists/queues/allocators, fences; ready to begin rendering to screen
D3D12WRAPPER::DEVICE::DEVICE(HRESULT& hr, HWND handle, SIZE res, int max_frame, bool windowed, DXGI_SWAP_CHAIN_FLAG flags) {
	renderTargets = make_unique<ComPtr<ID3D12Resource>[]>(max_frame);
	cmdAllocators = make_unique<ComPtr<ID3D12CommandAllocator>[]>(max_frame);

	fences = make_unique<ComPtr<ID3D12Fence>[]>(max_frame);
	fenceValue = make_unique<UINT64[]>(max_frame);

	fenceEvent = NULL;
	fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr) return;

	ComPtr<IDXGIFactory4> dxgiFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hr)) return;

	ComPtr<IDXGIAdapter1> adapter; //graphics card (including integrated graphics)
	UINT index = 0; //find first dx12 compatible card
	bool notSuitable = true;

	//putting index++ in the function causes a memory leak
	while (notSuitable && (dxgiFactory->EnumAdapters1(index++, &adapter) != DXGI_ERROR_NOT_FOUND)) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		//ignore software devices
		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr); //check if it is dx12
			if (SUCCEEDED(hr)) notSuitable = false;
		}
	}
	if (notSuitable) { hr = E_FAIL; return; }

	//memory leak here (IID_PPV_ARGS)
	hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&device));

	//create command queue
	D3D12_COMMAND_QUEUE_DESC cqDesc = {}; //default values (direct queue for swapchain)
	hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&renderQueue));
	if (FAILED(hr)) return;

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; //only one sample (no multisampling)

	//describe and create swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = res.cx;
	swapChainDesc.Height = res.cy;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferCount = max_frame;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.Flags = flags;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen;
	fullscreen.RefreshRate.Numerator = 0; //forcing native refresh rate (unsure how this is affected by ALLOW_TEARING //in fact, it doesn't work with ALLOW_TEARING)
	fullscreen.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	fullscreen.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	fullscreen.Windowed = windowed;
	
	ComPtr<IDXGISwapChain1> temp;
	hr = dxgiFactory->CreateSwapChainForHwnd(renderQueue.Get(), handle, &swapChainDesc, &fullscreen, NULL, &temp);
	if (FAILED(hr)) return;
	swapChain = static_cast<IDXGISwapChain3*>(temp.Get());

	//create rtv heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = max_frame; // number of descriptors for this heap (one for each buffer)
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //rtv heap not visible to shaders since it will store output from pipeline

	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtv));
	if (FAILED(hr)) return;

	UINT rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv->GetCPUDescriptorHandleForHeapStart()); //handle to first descriptor (basically a pointer)

	//create rtv for each buffer
	for (int i = 0; i < max_frame; i++) {
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
		if (FAILED(hr)) return;

		device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, rtv_descriptor_size); //increment handle by size gotten above
	}

	//creating command allocators
	for (int i = 0; i < max_frame; i++)
	{
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocators[i]));
		if (FAILED(hr)) return;
	}

	//create command list (only one needed, since it can record to one allocator and then go to the next frame's allocator)
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocators[current_frame].Get(), NULL, IID_PPV_ARGS(&cmdList));
	if (FAILED(hr)) return;

	//create fence event for synchronising
	for (int i = 0; i < max_frame; i++) {
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])); //first is initial fence value; flag is for if shared or not
		if (FAILED(hr)) return;
		fenceValue[i] = 0;
	}
}

void D3D12WRAPPER::StartDebug() {
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		debugController->EnableDebugLayer();;
	OutputDebugString(L"Entered Debugging======================= \n");
}

void D3D12WRAPPER::ReportLive() {
	OutputDebugString(L"Start Of Report===================== \n");
	ComPtr<IDXGIDebug1> dxgi_debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug))))
		dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	OutputDebugString(L"End Of Report======================= \n");
}