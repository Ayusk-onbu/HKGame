#include "GPUParticle.h"
#include "ModelManager.h"
#include "SDFManager.h"
#include "StringPool.h"
#include <d3dx12.h>

void GPUParticleSystem::Initialize(Fngine* engine, uint32_t numParticles) {
    freeListIndexBuffer_ = std::make_unique<RWStructured<int>>(engine);
    freeListIndexBuffer_->Initialize(1);

    freeListBuffer_ = std::make_unique<RWStructured<uint32_t>>(engine);
    freeListBuffer_->Initialize(numParticles);
    
    // 通常のインスタンス描画モードで初期化
    GPUComputeBaseModel::Initialize(engine, numParticles, GPURenderMode::Instanced);

    // 定数バッファの生成など
    perViewBuffer_ = std::make_unique<ConstantBuffer<PerView>>(p_engine_);
    perViewBuffer_->Initialize();

    emitBuffer_ = std::make_unique<ConstantBuffer<GPUEmitter>>(p_engine_);
    emitBuffer_->Initialize();
    emitBuffer_->GetMappedData()->count = 10;
    emitBuffer_->GetMappedData()->emit = 0;

    timeIndex_ = TimeKeeper::GetInstance()->RegisterTimer(0.5f, 0.0f, true);

    perFrameBuffer_ = std::make_unique<ConstantBuffer<PerFrame>>(p_engine_);
    perFrameBuffer_->Initialize();

    SetCommand = [this]() {
        auto commandList = p_engine_->GetCommand().GetList().GetList();

        auto preUAVBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            dataBuffer_->GetResource(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &preUAVBarrier);

        auto object = ModelManager::GetInstance()->LoadObjectData("plane");

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("GPUParticle").GetRootSignature().GetRS().Get());
        commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("GPUParticle").GetGPS().Get());
        commandList->IASetVertexBuffers(0, 1, &object.GetVertexBufferView());
        commandList->IASetIndexBuffer(&object.GetIndexBufferView());

        commandList->SetGraphicsRootConstantBufferView(0, perViewBuffer_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootDescriptorTable(1, dataBuffer_->GetSRVHandleGPU()); 
        commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture("GridLine").GetHandleGPU());

        commandList->DrawIndexedInstanced(object.GetIndexCount(), dataBuffer_->GetNumElements(), 0, 0, 0);

        auto postUAVBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            dataBuffer_->GetResource(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &postUAVBarrier);
    };
}

void GPUParticleSystem::Update(float deltaTime) {
    // 定数バッファの更新 (経過時間やエミッター位置をGPUに送る)
    auto camera = CameraSystem::GetInstance()->GetActiveCamera();
    perViewBuffer_->GetMappedData()->viewProjection = camera->GetViewProjectionMatrix();
    Matrix4x4 view = camera->GetViewMatrix();
    view.m[3][0] = 0.0f;
    view.m[3][1] = 0.0f;
    view.m[3][2] = 0.0f;
    view.m[3][3] = 1.0f; // ここはW成分なので 1.0f
    perViewBuffer_->GetMappedData()->billboardMatrix = Matrix4x4::Transpose(view);
    
    if (TimeKeeper::GetInstance()->IsEnd(timeIndex_)) {
        emitBuffer_->GetMappedData()->emit = 1;
    }
    else {
        emitBuffer_->GetMappedData()->emit = 0;
    }

    perFrameBuffer_->GetMappedData()->time = Chronos::GetInstance()->GetGameTime();
    perFrameBuffer_->GetMappedData()->deltaTime = 1.0f / 60.0f;

    // 基底クラスのUpdate（Compute Shader起動）をキック
    GPUComputeBaseModel::Update();
}

void GPUParticleSystem::DispatchInitializeCS(){
    auto commandList = p_engine_->GetCommand().GetList().GetList();
    ID3D12DescriptorHeap* ppHeaps[] = { SRVManager::GetInstance()->GetHeap()};
    // コマンドリストにヒープをセット（テーブルをセットするより【前】に呼ぶ！）
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    commandList->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("InitializeParticle.CS").GetRootSignature().GetRS().Get());
    commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("InitializeParticle.CS").GetCPS().Get());

    commandList->SetComputeRootDescriptorTable(0, dataBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootDescriptorTable(1, freeListIndexBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootDescriptorTable(2, freeListBuffer_->GetUAVHandleGPU());
    // 今回はちょうど1024個なので
    commandList->Dispatch(1,1,1);
}

void GPUParticleSystem::DispatchUpdateCS(){
    auto commandList = p_engine_->GetCommand().GetList().GetList();

    commandList->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("EmitParticleSDF.CS").GetRootSignature().GetRS().Get());
    commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("EmitParticleSDF.CS").GetCPS().Get());

    commandList->SetComputeRootDescriptorTable(0, dataBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootDescriptorTable(1, freeListIndexBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootDescriptorTable(2, freeListBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootConstantBufferView(3, emitBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(4, perFrameBuffer_->GetGPUVirtualAddress());
    uint32_t id = StringPool::GetInstance()->GetStringID("Naira_ExportTest");
    commandList->SetComputeRootConstantBufferView(5, SDFManager::GetInstance()->tempBakeResources_[id].configBuffer->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(6, SDFManager::GetInstance()->GetSDF("Naira_ExportTest")->texture.GetSRVHandleGPU());

    // 今回はちょうど1024個なので
    commandList->Dispatch(1, 1, 1);

    D3D12_RESOURCE_BARRIER particleBarrier{};
    particleBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    particleBarrier.UAV.pResource = dataBuffer_->GetResource();
    commandList->ResourceBarrier(1, &particleBarrier);

    commandList->SetComputeRootSignature(PSOManager::GetInstance()->GetPSO("UpdateParticle.CS").GetRootSignature().GetRS().Get());
    commandList->SetPipelineState(PSOManager::GetInstance()->GetPSO("UpdateParticle.CS").GetCPS().Get());

    commandList->SetComputeRootDescriptorTable(0, dataBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootDescriptorTable(1, freeListIndexBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootDescriptorTable(2, freeListBuffer_->GetUAVHandleGPU());
    commandList->SetComputeRootConstantBufferView(3, perFrameBuffer_->GetGPUVirtualAddress());

    // 今回はちょうど1024個なので
    commandList->Dispatch(1, 1, 1);
}
