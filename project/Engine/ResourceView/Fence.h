#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <stdint.h>

class Fence
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="device"></param>
	void Initialize(Microsoft::WRL::ComPtr <ID3D12Device>& device);
	/// <summary>
	/// 更新処理　SignalとWait
	/// </summary>
	/// <param name="commandQueue"></param>
	void Update(Microsoft::WRL::ComPtr <ID3D12CommandQueue>& commandQueue);
	/// <summary>
	/// Deleteと同処理
	/// </summary>
	void UnLoad();

	void Flush(Microsoft::WRL::ComPtr <ID3D12CommandQueue>& commandQueue);
private:
	/// <summary>
	/// ゴールと図るための道具を渡す処理
	/// </summary>
	/// <param name="commandQueue"></param>
	void Signal(Microsoft::WRL::ComPtr <ID3D12CommandQueue>& commandQueue);
	/// <summary>
	/// Fenceの値とValue比べて、超えるまで待つ処理
	/// </summary>
	void Wait();
private:
	Microsoft::WRL::ComPtr<ID3D12Fence>fence_;
	uint64_t value_;
	HANDLE event_;
};

