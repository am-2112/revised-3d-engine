#pragma once
#define WIN32_LEAN_AND_MEAN

//if adding support for multiple graphics apis, may want to include a general wrapper that will then convert to the needed implementation later (so only the general wrapper will be included here)
#include "D3D12Wrapper.h"
#include <windows.h>

//for debugging (getting mem leak detection)
#ifdef _DEBUG
#include <stdlib.h> //enabling MSVC++
#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC
#endif

#include <comdef.h>

#include <D3Dcompiler.h>
#define OutputReturnHRESULT(h) {OutputDebugString(_com_error(hr).ErrorMessage()); OutputDebugString(L"\n"); return false;}

#include "ParserOBJ.h"
using namespace Parser;

using namespace D3D12WRAPPER;
using namespace std;
using namespace Microsoft::WRL;

//entry point
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
);

//event handler
LRESULT CALLBACK WndProc(
	_In_ HWND   hWnd,
	_In_ UINT   message,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
);

//message loop
int mainloop(HWND& hwnd, DEVICE& device, vector<ComPtr<ID3D12PipelineState>>& PSOs, vector<ComPtr<ID3DBlob>>& shaders, vector<ComPtr<ID3D12RootSignature>>& roots);
SIZE GetResolution(HWND& hwnd);
bool CreatePSOs(DEVICE& device, std::vector<ComPtr<ID3D12PipelineState>> &PSOs, std::vector<ComPtr<ID3DBlob>> &shaders, std::vector<ComPtr<ID3D12RootSignature>> &roots);
bool CompileShader(LPCWSTR filePath, LPCSTR entry, LPCSTR target, std::vector<ComPtr<ID3DBlob>>& shaders);
void ShowBlobErrorMSG(ComPtr<ID3DBlob> error);
DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID& wicFormatGUID);

int WINAPI WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	//basic information about window
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"Engine";
	wcex.hIconSm = NULL;

	//register window class (for subsequent use in calls to CreateWindowEx)
	if (!RegisterClassExW(&wcex)) {
		MessageBox(NULL, L"Call to RegisterClassEx Failed", L"ClassRegistration", NULL);
		return 1;
	}
	
	//create window
	HWND hWnd = CreateWindowExW(
		WS_EX_OVERLAPPEDWINDOW, //window style
		wcex.lpszClassName, //class name
		L"Main", //app name
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, //initial x,y position
		800, 450, //size
		NULL, NULL, //parent; menu bar
		hInstance,
		NULL
	);
	if (!hWnd) {
		MessageBox(NULL, L"Call to CreateWindowExW Failed", L"CreatingWindow", NULL);
		return 1;
	}
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	
#ifdef _DEBUG
	D3D12WRAPPER::StartDebug();
	std::atexit(D3D12WRAPPER::ReportLive); //if com objects are added to vectors at global scale, their deconstructor will not be called before ReportLive() - so they need to be made in this function (whether directly, or from a different class / namespace)
#endif

	//once i know the amount of states i need to make, these can be turned into fixed size arrays - and enums can be used to index these for their purpose
	std::vector<ComPtr<ID3D12PipelineState>> PSOs;
	std::vector<ComPtr<ID3DBlob>> shaders;
	std::vector<ComPtr<ID3D12RootSignature>> roots;

	HRESULT hr;
	DEVICE device(hr, hWnd, GetResolution(hWnd), 3, true); //create device and set up rendering
	if (FAILED(hr)) OutputReturnHRESULT(hr);
	if (!CreatePSOs(device, PSOs, shaders, roots)) return false; //set up possible needed PSOs ahead of time

	int result = mainloop(hWnd, device, PSOs, shaders, roots);
	Sleep(100);
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return result;
}

#include <DirectXMath.h>
using namespace DirectX;

int mainloop(HWND& hwnd, DEVICE& device, vector<ComPtr<ID3D12PipelineState>> &PSOs, vector<ComPtr<ID3DBlob>> &shaders, vector<ComPtr<ID3D12RootSignature>> &roots) {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	//testing
	vector<OBJ::IND> ind; vector<OBJ::VERTEX> v; vector<OBJ::POINT> vn; vector<OBJ::TEX> vt; vector<OBJ::MATERIAL> mats;
	wstring file = L"..//sample-models//sofa//Contitnental_Sofa.obj";
	OBJ::Load(file, ind, v, vn, vt, mats); //with textures, currently ~3100 ms (5100ms on first load?) (mostly from loading in the textures)

	//also need to be able to handle materials with no maps (atm, it tries to upload an empty texture of infinite size) (this would require a different pixel shader - therefore a different pso; or creating a default texture & default information)

	//load index data to vertex buffer (vertex buffer since using structured buffers to index (with draw() rather than drawindex())
	int indSize = sizeof(OBJ::IND) * ind.size();
	ComPtr<ID3D12Resource> iBuffer; //default heap which the index information will be uploaded to
	ComPtr<ID3D12Resource> iUploadHeap;

	auto h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto b = CD3DX12_RESOURCE_DESC::Buffer(indSize);
	 HRESULT hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b, //resource description for a buffer
		D3D12_RESOURCE_STATE_COMMON,
		nullptr, //optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&iBuffer));
	if (FAILED(hr)) return false;

	//can give resource heaps a name so when debugging with the graphics debugger, can see what resource is being looked at
	iBuffer->SetName(L"Index Buffer Resource Heap");

	// create upload heap (to copy data to gpu; read only for gpu)
	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	b = CD3DX12_RESOURCE_DESC::Buffer(indSize);
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b, // resource description for a buffer
		D3D12_RESOURCE_STATE_COMMON, //GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&iUploadHeap));
	iUploadHeap->SetName(L"Index Buffer Upload Resource Heap");
	if (FAILED(hr)) return false;

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = ind.data(); //pointer to vertex array
	indexData.RowPitch = indSize; 
	indexData.SlicePitch = indSize; 

	UpdateSubresources(device.cmdList.Get(), iBuffer.Get(), iUploadHeap.Get(), 0, 0, 1, &indexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	auto t = CD3DX12_RESOURCE_BARRIER::Transition(iBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	device.cmdList->ResourceBarrier(1, &t);


	//load vertex, vertex normal, texture coordinate data to the structured buffers
	//order: v, vn, vt
	vector<ComPtr<ID3D12Resource>> sVBuffer(3);
	int vSize = sizeof(OBJ::VERTEX) * v.size();
	int vnSize = sizeof(OBJ::POINT) * vn.size();
	int vtSize = sizeof(OBJ::TEX) * vt.size();

	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	b = CD3DX12_RESOURCE_DESC::Buffer((UINT64)vSize);
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&sVBuffer[0]));
	if (FAILED(hr)) return false;
	sVBuffer[0]->SetName(L"V Resource Heap");

	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	b = CD3DX12_RESOURCE_DESC::Buffer((UINT64)vnSize);
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&sVBuffer[1]));
	if (FAILED(hr)) return false;
	sVBuffer[1]->SetName(L"Vn Resource Heap");

	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	b = CD3DX12_RESOURCE_DESC::Buffer((UINT64)vtSize);
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&sVBuffer[2]));
	if (FAILED(hr)) return false;
	sVBuffer[2]->SetName(L"Vt Resource Heap");

	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = v.data();
	vertexData.RowPitch = vSize;
	vertexData.SlicePitch = vSize;

	D3D12_SUBRESOURCE_DATA vertexNData = {};
	vertexNData.pData = vn.data();
	vertexNData.RowPitch = vnSize;
	vertexNData.SlicePitch = vnSize;

	D3D12_SUBRESOURCE_DATA vertexTData = {};
	vertexTData.pData = vt.data();
	vertexTData.RowPitch = vtSize;
	vertexTData.SlicePitch = vtSize;

	//upload vertex data (would this require creating a completely new upload heap or does reusing work?)
	vector<ComPtr<ID3D12Resource>> vUploadHeap(3);
	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	b = CD3DX12_RESOURCE_DESC::Buffer((UINT64)vSize); //large enough to upload all the data
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b, // resource description for a buffer
		D3D12_RESOURCE_STATE_COMMON, //GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&vUploadHeap[0]));
	vUploadHeap[0]->SetName(L"Vertex Data Upload Resource Heap");
	if (FAILED(hr)) return false;

	UpdateSubresources(device.cmdList.Get(), sVBuffer[0].Get(), vUploadHeap[0].Get(), 0, 0, 1, &vertexData);

	auto t2 = CD3DX12_RESOURCE_BARRIER::Transition(sVBuffer[0].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); //structured buffers not for pixel shader use
	device.cmdList.Get()->ResourceBarrier(1, &t2);


	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	b = CD3DX12_RESOURCE_DESC::Buffer((UINT64)vnSize); //large enough to upload all the data
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b, // resource description for a buffer
		D3D12_RESOURCE_STATE_COMMON, //GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&vUploadHeap[1]));
	vUploadHeap[1]->SetName(L"Vertex Data Upload Resource Heap");
	if (FAILED(hr)) return false;

	UpdateSubresources(device.cmdList.Get(), sVBuffer[1].Get(), vUploadHeap[1].Get(), 0, 0, 1, &vertexNData);

	 t2 = CD3DX12_RESOURCE_BARRIER::Transition(sVBuffer[1].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); //structured buffers not for pixel shader use
	device.cmdList.Get()->ResourceBarrier(1, &t2);


	h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	b = CD3DX12_RESOURCE_DESC::Buffer((UINT64)vtSize); //large enough to upload all the data
	hr = device->CreateCommittedResource(
		&h,
		D3D12_HEAP_FLAG_NONE,
		&b, // resource description for a buffer
		D3D12_RESOURCE_STATE_COMMON, //GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&vUploadHeap[2]));
	vUploadHeap[2]->SetName(L"Vertex Data Upload Resource Heap");
	if (FAILED(hr)) return false;

	UpdateSubresources(device.cmdList.Get(), sVBuffer[2].Get(), vUploadHeap[2].Get(), 0, 0, 1, &vertexTData);

	t2 = CD3DX12_RESOURCE_BARRIER::Transition(sVBuffer[2].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); //structured buffers not for pixel shader use
	device.cmdList.Get()->ResourceBarrier(1, &t2);


	//for now, only going to load main diffuse textures (ie. not any of the normal and others at the same time yet - would need different root parameters) into memory
	//can create descriptor heaps for each texture, and then set the correct one when rendering (with cmdList->SetDescriptorHeaps; since I will have one render pass per material)
	vector<ComPtr<ID3D12Resource>> textures(mats.size());
	vector<ComPtr<ID3D12Resource>> texUploadHeaps(mats.size());

	vector<ComPtr<ID3D12DescriptorHeap>> descriptorHeaps(mats.size());
	vector<D3D12_DESCRIPTOR_HEAP_DESC> heapDesc(mats.size());
	vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDesc(mats.size());

	for (int t = 0; t < mats.size(); t++) {
		//create resource description first
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Alignment = 0; // may be 0, 4KB, 64KB, or 4MB. 0 will let runtime decide between 64KB and 4MB (4MB for multi-sampled textures)
		textureDesc.Width = mats[t].maps[OBJ::INDEX_Map_Kd].size.cx; // width of the texture
		textureDesc.Height = mats[t].maps[OBJ::INDEX_Map_Kd].size.cy; // height of the texture
		textureDesc.DepthOrArraySize = 1; // if 3d image, depth of 3d image. Otherwise an array of 1D or 2D textures (we only have one image, so we set 1)
		textureDesc.MipLevels = 1; // Number of mipmaps. We are not generating mipmaps for this texture, so we have only one level
		textureDesc.Format = GetDXGIFormatFromWICFormat(mats[t].maps[OBJ::INDEX_Map_Kd].format); // This is the dxgi format of the image (format of the pixels)
		textureDesc.SampleDesc.Count = 1; // This is the number of samples per pixel, we just want 1 sample
		textureDesc.SampleDesc.Quality = 0; // The quality level of the samples. Higher is better quality, but worse performance
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // The arrangement of the pixels. Setting to unknown lets the driver choose the most efficient one
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // no flags


		auto h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		hr = device->CreateCommittedResource(
			&h, // a default heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&textureDesc, // the description of our texture
			D3D12_RESOURCE_STATE_COPY_DEST, // We will copy the texture from the upload heap to here, so we start it out in a copy dest state
			nullptr, // used for render targets and depth/stencil buffers
			IID_PPV_ARGS(&textures[t]));
		if (FAILED(hr)) return false;
		textures[t]->SetName(L"Texture Buffer Resource Heap");

		UINT64 textureUploadBufferSize;
		//this function gets the size an upload buffer needs to be to upload a texture to the gpu (accounting for padding)
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

		// now we create an upload heap to upload our texture to the GPU
		h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto b = CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize);
		hr = device->CreateCommittedResource(
			&h, // upload heap
			D3D12_HEAP_FLAG_NONE, // no flags
			&b, // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
			D3D12_RESOURCE_STATE_COMMON, // We will copy the contents from this heap to the default heap above
			nullptr,
			IID_PPV_ARGS(&texUploadHeaps[t]));
		if (FAILED(hr)) return false;
		texUploadHeaps[t]->SetName(L"Texture Buffer Upload Resource Heap");

		// store vertex buffer in upload heap
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = mats[t].maps[OBJ::INDEX_Map_Kd].tx.data(); // pointer to image data
		textureData.RowPitch = mats[t].maps[OBJ::INDEX_Map_Kd].stride; //stride?
		textureData.SlicePitch = mats[t].maps[OBJ::INDEX_Map_Kd].stride * textureDesc.Height;

		// Now we copy the upload buffer contents to the default heap
		UpdateSubresources(device.cmdList.Get(), textures[t].Get(), texUploadHeaps[t].Get(), 0, 0, 1, &textureData);

		// transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
		auto t5 = CD3DX12_RESOURCE_BARRIER::Transition(textures[t].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		device.cmdList->ResourceBarrier(1, &t5);

		// create the descriptor heap that will store our srv
		heapDesc[t] = {};
		heapDesc[t].NumDescriptors = 1;
		heapDesc[t].Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc[t].Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		hr = device->CreateDescriptorHeap(&heapDesc[t], IID_PPV_ARGS(&descriptorHeaps[t]));
		if (FAILED(hr)) return false;

		// now we create a shader resource view (descriptor that points to the texture and describes it)
		srvDesc[t] = {};
		srvDesc[t].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc[t].Format = textureDesc.Format;
		srvDesc[t].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc[t].Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(textures[t].Get(), &srvDesc[t], descriptorHeaps[t]->GetCPUDescriptorHandleForHeapStart());
	}


	//execute the commands that have been recorded, and then wait for the command queue to finish (implement wait function within DEVICE if it is not already there)
	device.cmdList->Close();
	ID3D12CommandList* ppCommandLists[] = { device.cmdList.Get() };
	device.renderQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	device.fenceValue[device.current_frame]++;
	hr = device.renderQueue->Signal(device.fences[device.current_frame].Get(), device.fenceValue[device.current_frame]);
	if (FAILED(hr)) return false;

	//wait for buffer upload
	if (!device.WaitForCommandQueue()) return false;

	//remember to reclaim memory (call deconstructors for all the textures in each material)
	for (int m = 0; m < mats.size(); m++) {
		for (int t = 0; t < 9; t++) {
			if (mats[m].maps[t].tx.size() > 0) mats[m].maps[t].tx.~vector();
		}
	}
	ind.~vector();
	v.~vector();
	vt.~vector();
	vn.~vector();

	//for some reason, there is still 1Gb of memory being used (and huge growth when uploading textures; is it copied to the upload heap? but when i destruct the heap it should go?)
	//might be able to get rid of all the heaps once they are uploaded and stuff? (ie. once I have views for them all)

	//creating buffer view for vertex buffer
	D3D12_VERTEX_BUFFER_VIEW vBufferView = {};
	vBufferView.BufferLocation = iBuffer->GetGPUVirtualAddress();
	vBufferView.StrideInBytes = sizeof(OBJ::IND);
	vBufferView.SizeInBytes = indSize;

	//create depthstencil view
	ComPtr<ID3D12DescriptorHeap> dsDescriptorHeap;
	ComPtr<ID3D12Resource> depthStencilBuffer;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
	if (FAILED(hr)) return false;

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	SIZE resolution = GetResolution(hwnd);

	auto h2 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto tex = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, resolution.cx, resolution.cy, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	device->CreateCommittedResource(
		&h2,
		D3D12_HEAP_FLAG_NONE,
		&tex,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
	);
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
	if (FAILED(hr)) return false;
	dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

	device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	//creating viewport
	D3D12_VIEWPORT viewport = {};
	 resolution = GetResolution(hwnd);

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = resolution.cx;
	viewport.Height = resolution.cy;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//creating scissor rect
	D3D12_RECT scissorRect = {};
	resolution = GetResolution(hwnd);

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = resolution.cx;
	scissorRect.bottom = resolution.cy;

	//build projection and view matrix
	 resolution = GetResolution(hwnd);
	XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(45.0f * (3.14f / 180.0f), (float)resolution.cx / (float)resolution.cy, 0.1f, 1000.0f);
	XMFLOAT4X4 cameraProjMat;
	XMFLOAT4X4 cameraViewMat;
	XMStoreFloat4x4(&cameraProjMat, tmpMat);

	// set starting camera state
	XMVECTOR defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
	XMVECTOR defaultUp = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

	XMFLOAT4 cameraPosition = XMFLOAT4(0.0f, 0.0f, -2.0f, 0.0f); //default cam position (may need to change in order to see object)
	XMFLOAT4 cameraForward;
	XMFLOAT4 cameraUp;

	XMStoreFloat4(&cameraForward, defaultForward);
	XMStoreFloat4(&cameraUp, defaultUp);

	// build view matrix
	XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
	XMVECTOR cTarg = XMLoadFloat4(&cameraForward);
	XMVECTOR cUp = XMLoadFloat4(&cameraUp);
	tmpMat = XMMatrixLookToLH(cPos, cTarg, cUp);
	XMStoreFloat4x4(&cameraViewMat, tmpMat);

	//create object matrix
	XMMATRIX temp = XMMatrixIdentity(); //shouldn't have any effect on position
	XMFLOAT4X4 wvpMat;
	XMStoreFloat4x4(&wvpMat, temp);

	//copy object matrix (make constant buffer heap too)
	unique_ptr<UINT8* []> cbvGPUAddress = make_unique<UINT8 * []>(3); //3 for triple buffering
	int ConstantBufferPerObjectAlignedSize = (sizeof(wvpMat) + 255) & ~255;

	//for each frame (3 for triple buffering)
	ComPtr<ID3D12Resource> constantBufferUploadHeaps[3];
	for (int i = 0; i < 3; ++i)
	{
		// create resource for cube 1
		auto h = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto b = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
		hr = device->CreateCommittedResource(
			&h, // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&b, // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_COMMON, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&constantBufferUploadHeaps[i]));
		constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");
		if (FAILED(hr)) return false;

		ZeroMemory(&wvpMat, sizeof(wvpMat));

		CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)

		// map the resource heap to get a gpu virtual address to the beginning of the heap
		hr = constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbvGPUAddress[i]));
		if (FAILED(hr)) return false;

		// Because of the constant read alignment requirements, constant buffer views must be 256 bit aligned. Our buffers are smaller than 256 bits,
		// so we need to add spacing between the two buffers, so that the second buffer starts at 256 bits from the beginning of the resource heap.
		XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat);
		XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat);
		temp = XMMatrixIdentity() * viewMat * projMat; //create wvp matrix
		XMMATRIX transposed = XMMatrixTranspose(temp); //must transpose for gpu
		XMStoreFloat4x4(&wvpMat, transposed);

		memcpy(cbvGPUAddress[i], &wvpMat, sizeof(wvpMat)); // cube1's constant buffer data; would use ConstantBufferPerObjectAlignedSize if there was more than one object
	}



	while (msg.message != WM_QUIT) {

		//update pipeline
		bool success = true;
		HRESULT hr;
		if (!device.WaitForCommandQueue()) return false;


		hr = device.cmdAllocators[device.current_frame]->Reset(); //resetting frees memory that the list was stored in
		if (FAILED(hr)) success = false;

		//reset; put it in recording state (all other lists from the given allocator must not be recording)
		hr = device.cmdList->Reset(device.cmdAllocators[device.current_frame].Get(), PSOs[0].Get());
		if (FAILED(hr)) success = false;

		CD3DX12_RESOURCE_BARRIER r = CD3DX12_RESOURCE_BARRIER::Transition(device.renderTargets[device.current_frame].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		device.cmdList->ResourceBarrier(1, &r);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(device.rtv->GetCPUDescriptorHandleForHeapStart(), device.current_frame, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		device.cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle); //set render target for output merger stage (output of pipeline)

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		device.cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr); //last two are optional, for clearing specified rects on the screen
		device.cmdList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// set root signature
		device.cmdList->SetGraphicsRootSignature(roots[0].Get()); // set the root signature

		//set other constant values
		device.cmdList->RSSetViewports(1, &viewport);
		device.cmdList->RSSetScissorRects(1, &scissorRect);
		device.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		device.cmdList->IASetVertexBuffers(0, 1, &vBufferView);
		device.cmdList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[device.current_frame]->GetGPUVirtualAddress());

		//setting root parameters
		device.cmdList->SetGraphicsRootShaderResourceView(2, sVBuffer[0]->GetGPUVirtualAddress());
		device.cmdList->SetGraphicsRootShaderResourceView(3, sVBuffer[1]->GetGPUVirtualAddress());
		device.cmdList->SetGraphicsRootShaderResourceView(4, sVBuffer[2]->GetGPUVirtualAddress());

		//loop through each material, start by setting the correct descriptor heap for the texture
		int total = 0;
		for (int t = 0; t < mats.size(); t++) {
			ID3D12DescriptorHeap* descriptorHeap[] = { descriptorHeaps[t].Get()};//, vDescHeap.Get(), vnDescHeap.Get(), vtDescHeap.Get() }; //get right texture (and remember to set structured buffers); issues with setting the structured buffers
			device.cmdList->SetDescriptorHeaps(_countof(descriptorHeap), descriptorHeap);
			device.cmdList->SetGraphicsRootDescriptorTable(1, descriptorHeaps[t]->GetGPUDescriptorHandleForHeapStart()); //1 since cb is first (setting texture descriptor table here); need descriptorTable for textures (srv doesn't work)
			

			//loop for each set of triangles that use this material
			for (int v = 0; v < mats[t].length.size(); v++) {
				device.cmdList->DrawInstanced(mats[t].length[v], 1, mats[t].start[v], 0);
				total += mats[t].length[v]; //debugging
			}
		}

		//transition from 'render target' to 'present' so that it can be presented to the screen
		r = CD3DX12_RESOURCE_BARRIER::Transition(device.renderTargets[device.current_frame].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		device.cmdList->ResourceBarrier(1, &r);

		hr = device.cmdList->Close();
		if (FAILED(hr)) success = false;

		//now present
		ID3D12CommandList* ppCommandLists[] = { device.cmdList.Get() }; //list of command lists (only one atm)
		device.renderQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		hr = device.renderQueue->Signal(device.fences[device.current_frame].Get(), ++device.fenceValue[device.current_frame]); //know when command queue has finished, because fence value will be set to 'fenceValue' from the gpu	
		if (FAILED(hr)) success = false;

		hr = device.swapChain->Present(0, NULL); //present current backbuffer
		if (FAILED(hr)) success = false;

		if (!success) return false;


		//handle all queued messages (last, in case user exits -> don't want to render after window has been destroyed)
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}
//still need to add deconstructor code to ensure that I wait till rendering has stopped (once I move this code out of the main .cpp file)

//should put somewhere else maybe
DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID& wicFormatGUID) {
	if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) return DXGI_FORMAT_R32G32B32A32_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) return DXGI_FORMAT_R16G16B16A16_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) return DXGI_FORMAT_R16G16B16A16_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) return DXGI_FORMAT_R8G8B8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) return DXGI_FORMAT_B8G8R8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) return DXGI_FORMAT_B8G8R8X8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) return DXGI_FORMAT_R10G10B10A2_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) return DXGI_FORMAT_B5G5R5A1_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) return DXGI_FORMAT_B5G6R5_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) return DXGI_FORMAT_R32_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) return DXGI_FORMAT_R16_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) return DXGI_FORMAT_R16_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) return DXGI_FORMAT_R8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) return DXGI_FORMAT_A8_UNORM;

	else return DXGI_FORMAT_UNKNOWN;
}

SIZE GetResolution(HWND& hwnd) {
	RECT cRect;
	GetClientRect(hwnd, &cRect);
	SIZE res;
	res.cx = cRect.right - cRect.left;
	res.cy = cRect.bottom - cRect.top;
	return res;
}

LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0); //send WM_QUIT to stop message loop
		break;
	default: //default processing of messages
		return DefWindowProcW(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

//both this and the HRESULT definition do not seem to work
void ShowBlobErrorMSG(ComPtr<ID3DBlob> error) {
	if (error == nullptr) return;
	const char* errorMessage = static_cast<const char*>(error->GetBufferPointer());
	OutputDebugStringA(errorMessage);
}

bool CompileShader(LPCWSTR filePath, LPCSTR entry, LPCSTR target, std::vector<ComPtr<ID3DBlob>>& shaders) {
	HRESULT hr;

	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> error;
	hr = D3DCompileFromFile(filePath,
		nullptr,
		nullptr,
		entry, //entrypoint
		target,
#ifdef _DEBUG
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
#else
		D3DCOMPILE_OPTIMIZATION_LEVEL3,
#endif
		0, //flags for effect files
		&shader,
		&error);
	if (FAILED(hr)) { ShowBlobErrorMSG(error); OutputReturnHRESULT(hr) };
	shaders.push_back(shader);

	return true;
}

bool CreatePSOs(DEVICE& device, std::vector<ComPtr<ID3D12PipelineState>>& PSOs, std::vector<ComPtr<ID3DBlob>>& shaders, std::vector<ComPtr<ID3D12RootSignature>>& roots) {
	//start by creating al the input layouts that may be needed
	HRESULT hr;
	D3D12_INPUT_ELEMENT_DESC OBJ_INPUT[] =
	{
		{ "INDEX", 0,DXGI_FORMAT_R32G32B32_SINT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } //rgb 32-bit uint for indexing structured buffers of v, vt, vn; intended for rendering passes per-material
	};
	D3D12_INPUT_LAYOUT_DESC OBJ_LAYOUT_DESC = {};
	OBJ_LAYOUT_DESC.NumElements = sizeof(OBJ_INPUT) / sizeof(D3D12_INPUT_ELEMENT_DESC); //gets number of elements
	OBJ_LAYOUT_DESC.pInputElementDescs = OBJ_INPUT;

	//sample desc from swapchain
	DXGI_SWAP_CHAIN_DESC scDesc;
	device.swapChain->GetDesc(&scDesc);

	//compile shaders and add them to the vector list
	if (!CompileShader(L"../Engine/Core/Main/OBJ_Vertex.hlsl", "main", "vs_5_1", shaders)) return false;
	if (!CompileShader(L"../Engine/Core/Main/Default_Pixel.hlsl", "main", "ps_5_1", shaders)) return false;

	//create root signatures
	D3D12_ROOT_DESCRIPTOR rootWPVMatDescriptor{ //the following two are likely to be common (wpv matrix and a single texture for material)
		.ShaderRegister = 0,
		.RegisterSpace = 0
	};
	D3D12_DESCRIPTOR_RANGE matTexture{
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = 1, //only one texture
		.BaseShaderRegister = 0, 
		.RegisterSpace = 0,
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND //appended from root signature tables
	};

	D3D12_ROOT_DESCRIPTOR_TABLE matTableRanges{ //separate ranges since the visibility in the shaders is different
		.NumDescriptorRanges = 1,
		.pDescriptorRanges = &matTexture
	};

	D3D12_ROOT_DESCRIPTOR vRange{
		.ShaderRegister = 1,
		.RegisterSpace = 0
	};
	D3D12_ROOT_DESCRIPTOR vnRange{
		.ShaderRegister = 2,
		.RegisterSpace = 0
	};
	D3D12_ROOT_DESCRIPTOR vtRange{
		.ShaderRegister = 3,
		.RegisterSpace = 0
	};

	//finish creating root parameters for first / default .obj pso
	D3D12_ROOT_PARAMETER rootParameters[5]{
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV, .Descriptor = rootWPVMatDescriptor, .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX},
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, .DescriptorTable = matTableRanges, .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL},
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV, .Descriptor = vRange, .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX},
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV, .Descriptor = vnRange, .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX},
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV, .Descriptor = vtRange, .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX},
	};

	//default sampler
	D3D12_STATIC_SAMPLER_DESC sampler{
		.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		.MipLODBias = 0,
		.MaxAnisotropy = 0,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
		.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		.MinLOD = 0.0f,
		.MaxLOD = D3D12_FLOAT32_MAX,
		.ShaderRegister = 0,
		.RegisterSpace = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};

	//create root signature desc
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(5, 
		rootParameters,
		1, //one static sampler
		&sampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | //can deny shader stages here for better performance
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

	//create and serialize signature
	ComPtr<ID3DBlob> error; // a buffer holding the error data if any
	ComPtr<ID3DBlob> signature;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
	if (FAILED(hr)) { ShowBlobErrorMSG(error); OutputReturnHRESULT(hr); }

	ComPtr<ID3D12RootSignature> root;
	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root));
	if (FAILED(hr)) OutputReturnHRESULT(hr);
	roots.push_back(root);

	//may need to set rasterizer state to ensure that the triangles aren't being culled
	D3D12_RASTERIZER_DESC rDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rDesc.CullMode = D3D12_CULL_MODE_BACK; //testing

	//default pipeline state for .obj models
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { // a structure to define a pso
		.pRootSignature = root.Get(),
		.VS = {.pShaderBytecode = shaders[0]->GetBufferPointer(), .BytecodeLength = shaders[0]->GetBufferSize()},
		.PS = {.pShaderBytecode = shaders[1]->GetBufferPointer(), .BytecodeLength = shaders[1]->GetBufferSize()},
		.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		.SampleMask = 0xffffffff, //point sampling is done
		.RasterizerState = rDesc,
		.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		.InputLayout = OBJ_LAYOUT_DESC,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM},
		.DSVFormat = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc = scDesc.SampleDesc, // must be the same sample description as the swapchain and depth/stencil buffer
	};

	//create default pso for .obj models
	ComPtr<ID3D12PipelineState> OBJ_STATE;
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&OBJ_STATE));
	if (FAILED(hr)) OutputReturnHRESULT(hr);
	PSOs.push_back(OBJ_STATE);

	return true;
}