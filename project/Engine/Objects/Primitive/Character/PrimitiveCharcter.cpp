#include "PrimitiveCharcter.h"
#include "ModelManager.h"
#include "SRVManager.h"

void PrimitiveCharacter::Initialize(Fngine* engine, uint32_t numInstance) {
    PrimitiveBaseModel::Initialize(engine, numInstance);
    useModelName_ = "Naira";

    SetCommandPrePass = [this]() {
        if (instanceDataList_.empty()) return;

        auto commandList = p_engine_->GetCommand().GetList().GetList();

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // SRVごとにデータサイズが違うので別のものを作らないといけない
        auto& prePassPSO = PSOManager::GetInstance()->GetPSO("DepthPrePassCharacter");
        commandList->SetGraphicsRootSignature(prePassPSO.GetRootSignature().GetRS().Get());
        commandList->SetPipelineState(prePassPSO.GetGPS().Get());

        // インスタンスバッファ(WVP等)のセット
        commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());

        // 各インスタンスを描画 (スキニング動的VBVの切り替えに対応)
        for (UINT i = 0; i < static_cast<UINT>(instanceDataList_.size()); ++i) {
            const auto& instanceData = instanceDataList_[i];
            auto& objData = ModelManager::GetInstance()->LoadObjectData(instanceData.modelName);

            if (instanceData.useCustomVB) {
                commandList->IASetVertexBuffers(0, 1, &instanceData.vertexBufferView);
            }
            else {
                commandList->IASetVertexBuffers(0, 1, &objData.GetVertexBufferView());
            }

            commandList->IASetIndexBuffer(&objData.GetIndexBufferView());

            commandList->DrawIndexedInstanced(objData.GetIndexCount(), 1, 0, 0, i);
        }
    };

    SetCommand = [this]() {
        if (instanceDataList_.empty()) return;

        auto commandList = p_engine_->GetCommand().GetList().GetList();
        
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("Character").GetRootSignature().GetRS().Get());
        commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("Character").GetGPS().Get());

        // 記述子ヒープ等のバインド
        commandList->SetGraphicsRootDescriptorTable(0, instancingBuffer_->GetSRVHandleGPU());
        commandList->SetGraphicsRootDescriptorTable(1, SRVManager::GetInstance()->GetHeap()->GetGPUDescriptorHandleForHeapStart());

        // 各インスタンスを描画
        for (UINT i = 0; i < static_cast<UINT>(instanceDataList_.size()); ++i) {
            const auto& instanceData = instanceDataList_[i];

            // 受け取った ModelID を使って ModelManager から静的データを取得
            auto& objData = ModelManager::GetInstance()->LoadObjectData(instanceData.modelName);

            // 頂点バッファ(VBV)のセット (スキニング動的VBV or 静的VBV)
            if (instanceData.useCustomVB) {
                // Dispachしてないからデータが存在しない
                commandList->IASetVertexBuffers(0, 1, &instanceData.vertexBufferView);
                //commandList->IASetVertexBuffers(0, 1, &objData.GetVertexBufferView());
            }
            else {
                commandList->IASetVertexBuffers(0, 1, &objData.GetVertexBufferView());
            }

            // ModelManager から取得した IBV と IndexCount をセットして描画！
            commandList->IASetIndexBuffer(&objData.GetIndexBufferView());
            commandList->DrawIndexedInstanced(objData.GetIndexCount(), 1, 0, 0, i);
        }
    };
}

CharacterForGPU PrimitiveCharacter::ConvertToGPUData(const CharacterObjectData& data) {
    CharacterForGPU returnData{};
    returnData.WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(data.worldTransform.mat_);
    returnData.World = data.worldTransform.mat_;
    returnData.uvTransform = data.uvTransform.mat_;
    returnData.color = data.color;

    // テクスチャインデックスを取得
    returnData.colorTextureIndex = TextureManager::GetInstance()->GetTexture(data.colorTextureName).GetSrvIndex();
    returnData.normalTextureIndex = TextureManager::GetInstance()->GetTexture(data.normalTextureName).GetSrvIndex();
    returnData.skinMask1TextureIndex = TextureManager::GetInstance()->GetTexture(data.skinMask1TextureName).GetSrvIndex();
    returnData.skinMask2TextureIndex = TextureManager::GetInstance()->GetTexture(data.skinMask2TextureName).GetSrvIndex();
    returnData.melaninScale = data.melaninScale;
    returnData.bloodVolumeScale = data.bloodVolumeScale;
    returnData.roughnessScale = data.roughnessScale;
    returnData.sssScale = data.sssScale;

    return returnData;
}