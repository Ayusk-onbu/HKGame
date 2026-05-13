#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "D3D12System.h"

enum class ROOTTYPE {
	Normal,
	Structured,
	Skinning,
	CopyImage,
	SkyBox,
};

class RootSignature
{
public:
	/// <summary>
	/// 送りたい情報の設定、紐づけ
	/// </summary>
	/// <param name="d3d12"></param>
	/// <param name="type"></param>
	void CreateRootSignature(D3D12System& d3d12, ROOTTYPE type);

	Microsoft::WRL::ComPtr <ID3D12RootSignature>& GetRS() { return rootSignature_; }

private:
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature_ = nullptr;
};

