#pragma once

#ifndef D3D12WRAPPER_H
#define D3D12WRAPPER_H

#include <d3dx12.h>
#include <dxgi1_4.h>

namespace D3D12WRAPPER {
	struct ManagedHandle {
	private:
		HANDLE h;
	public:
		ManagedHandle& operator=(HANDLE handle)
		{
			h = handle;
			return *this;
		}
		bool operator ==(HANDLE handle)
		{
			return (h == handle);
		}
		bool operator !=(HANDLE handle) {
			return (h != handle);
		}
		HANDLE operator->()
		{
			return h;
		}
		~ManagedHandle() {
			CloseHandle(h);
		}
		HANDLE Get() {
			return h;
		}
	};

	class DEVICE {
	public:
		DEVICE(HRESULT &hr, HWND handle, SIZE res, int max_frame, bool windowed, DXGI_SWAP_CHAIN_FLAG flags = (DXGI_SWAP_CHAIN_FLAG)NULL);
		
		Microsoft::WRL::ComPtr<ID3D12Device4> operator->() {
			return device;
		}
		bool WaitForCommandQueue() {
			bool success = true;
			HRESULT hr;

			// swap the current rtv buffer index to draw on the correct buffer
			current_frame = swapChain->GetCurrentBackBufferIndex();

			//if fence value has not been set by gpu, create event and wait
			if (fences[current_frame]->GetCompletedValue() < fenceValue[current_frame]) {
				hr = fences[current_frame]->SetEventOnCompletion(fenceValue[current_frame], fenceEvent.Get()); //create an event for when the fence value is set by gpu (when it gets to the commandQueue->Signal(fence, fenceValue) command)
				if (FAILED(hr)) success = false;

				if (success) WaitForSingleObject(fenceEvent.Get(), INFINITE);
			}
			return success;
		}

		//the basic components to render to the screen
		Microsoft::WRL::ComPtr<ID3D12Device4> device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> renderQueue;
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv;
		std::unique_ptr<Microsoft::WRL::ComPtr<ID3D12Resource>[]> renderTargets;

		std::unique_ptr<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>[]> cmdAllocators;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;

		std::unique_ptr<Microsoft::WRL::ComPtr<ID3D12Fence>[]> fences;
		std::unique_ptr<UINT64[]> fenceValue;
		ManagedHandle fenceEvent;

		int current_frame = 0;
	private:
	};

	void StartDebug();
	void ReportLive();
}

#endif