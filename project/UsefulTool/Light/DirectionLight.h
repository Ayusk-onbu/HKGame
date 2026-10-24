#pragma once
#include <wrl.h>
#include "ResourceFunc.h"
#include "D3D12System.h"
//#include "Structured.h"

struct DirectionalLight {
	Vector4 color;// ライトの色
	Vector3 direction;// ライトの向き
	float intensity;// 輝度
	int shadowType;// シャドウの種類
};

class DirectionLight
{
public:
	void Initialize(D3D12System d3d12);
	void SetType(const int& type);
	DirectionalLight* GetDirectionalLightData() { return directionalLightData_; }
private:
	void GetResourceAddress();
#pragma region GetSeries
public:
	Microsoft::WRL::ComPtr <ID3D12Resource>GetResource() { return resource_; }
#pragma endregion
public:
	DirectionalLight* directionalLightData_ = nullptr;
private:
	Microsoft::WRL::ComPtr <ID3D12Resource> resource_;
};

// LightはLightManagerに持たせてそいつをSingletonで扱う
class DirectionalLightManager
{
public:
	//DirectionalLightManager(/*Fngine* engine*/)/* : p_engine_(engine)*/{}
	DirectionalLightManager() = default;
	~DirectionalLightManager() = default;

public:
	// 初期化処理
	void Initialize(uint32_t maxCounts);

	// Resourceとして使用する
	void BindResource(uint32_t slot);

	// 使用するLightの追加
	uint32_t AddLight(const DirectionalLight& light);

	uint32_t AddLight();

	//DirectionalLight& GetLight(uint32_t index);

	uint32_t GetActiveCount()const { return activeCount_; }
private:
	// エンジン
	//Fngine* p_engine_;

	// データ
	//std::unique_ptr<Structured<DirectionalLight>>dataList_;

	// カウント
	uint32_t activeCount_ = 0;
}; 