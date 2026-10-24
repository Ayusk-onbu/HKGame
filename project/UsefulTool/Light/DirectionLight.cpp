#include "DirectionLight.h"
#include "Fngine.h"

void DirectionLight::Initialize(D3D12System d3d12) {
	resource_ = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(DirectionalLight));
	GetResourceAddress();
	//デフォルト値
	directionalLightData_->color = { 0.0f,0.650f,0.650f,1.0f };
	directionalLightData_->direction = { 0.0f,0.60f,0.0f };
	directionalLightData_->intensity = 1.0f;
	directionalLightData_->shadowType = 2; // デフォルトのシャドウタイプ
}

void DirectionLight::SetType(const int& type) {
	directionalLightData_->shadowType = type;
}

void DirectionLight::GetResourceAddress() {
	//書き込むためのアドレスを取得
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
}


void DirectionalLightManager::Initialize(uint32_t maxLights) {
    /*dataList_ = std::make_unique<Structured<DirectionalLight>>(p_engine_);
    dataList_->Initialize(maxLights);
    activeCount_ = 0;*/
}

// ライトの追加
uint32_t DirectionalLightManager::AddLight(const DirectionalLight& light) {
    //if (activeCount_ >= dataList_->GetNumElements()) {
    //    return UINT32_MAX; // キャパシティオーバー
    //}

    //uint32_t index = activeCount_;
    //// 戻り値の T* ポインタ経由で配列のように直接設定
    //dataList_->GetMappedData()[index] = light;
    //activeCount_++;

    //return index;
    return 0;
}

uint32_t DirectionalLightManager::AddLight() {
    //if (activeCount_ >= dataList_->GetNumElements()) {
    //    return UINT32_MAX; // キャパシティオーバー
    //}

    //DirectionalLight data;
    //data.direction = {};
    //data.color = { 1.0f,1.0f,1.0f,1.0f };
    //data.intensity = 1.0f;
    //data.shadowType = 2;

    //uint32_t index = activeCount_;
    //// 戻り値の T* ポインタ経由で配列のように直接設定
    //dataList_->GetMappedData()[index] = data;
    //activeCount_++;

    //return index;
    return 0;
}

// 既存ライトの更新（参照を取得して直接編集）
//DirectionalLight& DirectionalLightManager::GetLight(uint32_t index) {
//    return dataList_->GetMappedData()[index];
//}

// CommandList へ描画設定（SRVのバインド）
void DirectionalLightManager::BindResource(uint32_t rootParameterIndex) {
    /*auto commandList = p_engine_->GetCommand().GetList().GetList().Get();

    commandList->SetGraphicsRootDescriptorTable(
        rootParameterIndex,
        dataList_->GetSRVHandleGPU()
    );

    commandList->SetGraphicsRoot32BitConstant(rootParameterIndex + 1, activeCount_, 0);*/
}