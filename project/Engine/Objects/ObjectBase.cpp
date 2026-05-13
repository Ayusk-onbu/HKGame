#include "ObjectBase.h"
#include "Fngine.h"

//  ==========
//【 Draw関係 】
//  ==========

void ObjectBase::DrawBase(ObjectDrawType type) {
	//RootSignalの設定
	if (type == ObjectDrawType::SOLID) {
		fngine_->GetCommand().GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("Object3D").GetRootSignature().GetRS().Get());
		fngine_->GetCommand().GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO("Object3D").GetGPS().Get());
	}
	if (type == ObjectDrawType::WIREFRAME) {
		fngine_->GetCommand().GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("DebugObject3D").GetRootSignature().GetRS().Get());
		fngine_->GetCommand().GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO("DebugObject3D").GetGPS().Get());
	}
	fngine_->GetCommand().GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//マテリアルCBufferの場所を設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture(textureName_).GetHandleGPU());
	//DirectionLight用のCBufferの場所を設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(3, fngine_->GetLight().GetResource()->GetGPUVirtualAddress());
	// カメラの位置を送るための設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(4, fngine_->GetCameraForGPU().GetResource()->GetGPUVirtualAddress());
	// PointLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(5, fngine_->GetPointLight().GetResource()->GetGPUVirtualAddress());
	// SpotLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(6, fngine_->GetSpotLight().GetResource()->GetGPUVirtualAddress());
	// AreaLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(7, fngine_->GetAreaLight().GetResource()->GetGPUVirtualAddress());
	// t1: LTC1 (AreaLightクラスが持っているハンドルを個別に渡す)
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(8, fngine_->GetAreaLight().GetLTC1GPUHandle());
	// t2: LTC2
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(9, fngine_->GetAreaLight().GetLTC2GPUHandle());
	// t3: Environment Light
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(10, TextureManager::GetInstance()->GetTexture("rostock_laage_airport_4k").GetHandleGPU());
}

void ObjectBase::DrawBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, Texture& tex) {
	//RootSignalの設定
	command.GetList().GetList()->SetGraphicsRootSignature(pso.GetRootSignature().GetRS().Get());
	command.GetList().GetList()->SetPipelineState(pso.GetGPS().Get());
	command.GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//マテリアルCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	command.GetList().GetList()->SetGraphicsRootDescriptorTable(2, tex.GetHandleGPU());
	//DirectionLight用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(3, light.GetResource()->GetGPUVirtualAddress());
	// カメラの位置を送るための設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(4, fngine_->GetCameraForGPU().GetResource()->GetGPUVirtualAddress());
	// PointLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(5, fngine_->GetPointLight().GetResource()->GetGPUVirtualAddress());
	// SpotLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(6, fngine_->GetSpotLight().GetResource()->GetGPUVirtualAddress());
	// AreaLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(7, fngine_->GetAreaLight().GetResource()->GetGPUVirtualAddress());
	// t1: LTC1 (AreaLightクラスが持っているハンドルを個別に渡す)
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(8, fngine_->GetAreaLight().GetLTC1GPUHandle());
	// t2: LTC2
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(9, fngine_->GetAreaLight().GetLTC2GPUHandle());
}

void ObjectBase::DrawBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, D3D12_GPU_DESCRIPTOR_HANDLE& tex) {
	//RootSignalの設定
	command.GetList().GetList()->SetGraphicsRootSignature(pso.GetRootSignature().GetRS().Get());
	command.GetList().GetList()->SetPipelineState(pso.GetGPS().Get());
	command.GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_); 
	//マテリアルCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	command.GetList().GetList()->SetGraphicsRootDescriptorTable(2, tex);
	//DirectionLight用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(3, light.GetResource()->GetGPUVirtualAddress());
	// カメラの位置を送るための設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(4, fngine_->GetCameraForGPU().GetResource()->GetGPUVirtualAddress());
	// PointLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(5, fngine_->GetPointLight().GetResource()->GetGPUVirtualAddress());
	// SpotLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(6, fngine_->GetSpotLight().GetResource()->GetGPUVirtualAddress());
	// AreaLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(7, fngine_->GetAreaLight().GetResource()->GetGPUVirtualAddress());
	// t1: LTC1 (AreaLightクラスが持っているハンドルを個別に渡す)
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(8, fngine_->GetAreaLight().GetLTC1GPUHandle());
	// t2: LTC2
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(9, fngine_->GetAreaLight().GetLTC2GPUHandle());
}

void ObjectBase::DrawIndexBase(ObjectDrawType type) {
	//RootSignalの設定
	if (type == ObjectDrawType::SOLID) {
		fngine_->GetCommand().GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("Object3D").GetRootSignature().GetRS().Get());
		fngine_->GetCommand().GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO("Object3D").GetGPS().Get());
	}
	if (type == ObjectDrawType::WIREFRAME) {
		fngine_->GetCommand().GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("DebugObject3D").GetRootSignature().GetRS().Get());
		fngine_->GetCommand().GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO("DebugObject3D").GetGPS().Get());
	}
	/*D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
		vertexBufferView_,
		skinCluster_.influences_->GetSRVHandleGPU().ptr
	};
	fngine_->GetCommand().GetList().GetList()->IASetVertexBuffers(0, 2, vbvs);*/
	D3D12_VERTEX_BUFFER_VIEW skinnedVbView{};
	skinnedVbView.BufferLocation = skinCluster_.outputVertices_->GetResource()->GetGPUVirtualAddress();
	skinnedVbView.SizeInBytes = sizeof(VertexData) * skinCluster_.GetNumVertices();
	skinnedVbView.StrideInBytes = sizeof(VertexData);
	fngine_->GetCommand().GetList().GetList()->IASetVertexBuffers(0, 1, &skinnedVbView);
	// IndexBufferView(IBV)の設定
	fngine_->GetCommand().GetList().GetList()->IASetIndexBuffer(&indexBufferView_);
	//マテリアルCBufferの場所を設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture(textureName_).GetHandleGPU());
	//DirectionLight用のCBufferの場所を設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(3, fngine_->GetLight().GetResource()->GetGPUVirtualAddress());
	// カメラの位置を送るための設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(4, fngine_->GetCameraForGPU().GetResource()->GetGPUVirtualAddress());
	// PointLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(5, fngine_->GetPointLight().GetResource()->GetGPUVirtualAddress());
	// SpotLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(6, fngine_->GetSpotLight().GetResource()->GetGPUVirtualAddress());
	// AreaLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(7, fngine_->GetAreaLight().GetResource()->GetGPUVirtualAddress());
	// t1: LTC1 (AreaLightクラスが持っているハンドルを個別に渡す)
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(8, fngine_->GetAreaLight().GetLTC1GPUHandle());
	// t2: LTC2
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(9, fngine_->GetAreaLight().GetLTC2GPUHandle());
	// t3: Environment Light
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(10, TextureManager::GetInstance()->GetTexture("rostock_laage_airport_4k").GetHandleGPU());
	// MatrixPalatteの設定
	//fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(10, skinCluster_.paletteSrvHandle_.second);
}

void ObjectBase::DrawIndexBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, Texture& tex) {
	//RootSignalの設定
	command.GetList().GetList()->SetGraphicsRootSignature(pso.GetRootSignature().GetRS().Get());
	command.GetList().GetList()->SetPipelineState(pso.GetGPS().Get());
	command.GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	// IndexBufferView(IBV)の設定
	command.GetList().GetList()->IASetIndexBuffer(&indexBufferView_);
	//マテリアルCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	command.GetList().GetList()->SetGraphicsRootDescriptorTable(2, tex.GetHandleGPU());
	//DirectionLight用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(3, light.GetResource()->GetGPUVirtualAddress());
	// カメラの位置を送るための設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(4, fngine_->GetCameraForGPU().GetResource()->GetGPUVirtualAddress());
	// PointLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(5, fngine_->GetPointLight().GetResource()->GetGPUVirtualAddress());
	// SpotLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(6, fngine_->GetSpotLight().GetResource()->GetGPUVirtualAddress());
}

void ObjectBase::DrawIndexBase(TheOrderCommand& command, PSO& pso, DirectionLight& light, D3D12_GPU_DESCRIPTOR_HANDLE& tex) {
	//RootSignalの設定
	command.GetList().GetList()->SetGraphicsRootSignature(pso.GetRootSignature().GetRS().Get());
	command.GetList().GetList()->SetPipelineState(pso.GetGPS().Get());
	command.GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	// IndexBufferView(IBV)の設定
	command.GetList().GetList()->IASetIndexBuffer(&indexBufferView_);
	//マテリアルCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	command.GetList().GetList()->SetGraphicsRootDescriptorTable(2, tex);
	//DirectionLight用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(3, light.GetResource()->GetGPUVirtualAddress());
	// カメラの位置を送るための設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(4, fngine_->GetCameraForGPU().GetResource()->GetGPUVirtualAddress());
	// PointLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(5, fngine_->GetPointLight().GetResource()->GetGPUVirtualAddress());
	// SpotLight用のCBufferの設定
	fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(6, fngine_->GetSpotLight().GetResource()->GetGPUVirtualAddress());
}

void ObjectBase::InitializeMD(Vector4 color, bool isLight) {
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = color;
	materialData_->enableLighting = isLight;
	materialData_->uvTransform = Matrix4x4::Make::Identity();
	materialData_->shininess = 32.0f;
}

void ObjectBase::InitializeWVPD() {
	//書き込むためのアドレスを取得
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	//単位行列を書き込んでいく
	wvpData_->WVP = Matrix4x4::Make::Identity();
	wvpData_->World = Matrix4x4::Make::Identity();
	wvpData_->worldInverseTranspose = Matrix4x4::Make::Identity();
}

void ObjectBase::SetColor(const Vector4& color) {
	materialData_->color = color;
}