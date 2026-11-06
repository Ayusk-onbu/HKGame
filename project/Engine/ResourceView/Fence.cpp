#include "Fence.h"
#include <cassert>

void Fence::Initialize(Microsoft::WRL::ComPtr <ID3D12Device>& device) {
	HRESULT hr;
	fence_ = nullptr;
	value_ = 0;
	hr = device->CreateFence(value_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));
	//FenceのSignalを持つためのイベントを作成する
	event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(event_ != nullptr);
}

void Fence::Update(Microsoft::WRL::ComPtr <ID3D12CommandQueue>& commandQueue) {
	Signal(commandQueue);
	Wait();
}

void Fence::Signal(Microsoft::WRL::ComPtr <ID3D12CommandQueue>& commandQueue) {
	// 上限の更新
	value_++;
	// ゴールの位置の設定と、どの位進んだかを記録するための物を渡す
	commandQueue->Signal(fence_.Get(), value_);
}

void Fence::Wait() {
	//Fenceの値が指定したSignal値にたどり着いているか確認する
	//GetCompletedValueの初期値はFence作成時に渡した初期値
	if (fence_->GetCompletedValue() < value_) {
		//指定したSingnalにたどりついていないので、たどりつくまで待つようにイベントを設定する
		fence_->SetEventOnCompletion(value_, event_);
		//イベントを待つ
		WaitForSingleObject(event_, INFINITE);
	}
}

void Fence::UnLoad() {
	CloseHandle(event_);
}

void Fence::Flush(Microsoft::WRL::ComPtr <ID3D12CommandQueue>& commandQueue) {
	Signal(commandQueue);
	Wait();
}