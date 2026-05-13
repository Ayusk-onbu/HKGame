#include "SkyBox.h"

void SkyBox::Initialize(Fngine* engine, std::string textureName) {
	p_engine_ = engine;
	textureName_ = textureName;

	auto& d3d12 = p_engine_->GetD3D12System();
	size_t vertexNum = 8;

	//   ================================
	// 【 Vertex Resource に関するコード 】
	//   ================================
	vertexResource_ = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(SkyBoxVertexData) * vertexNum);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// ８頂点分作る
	vertexData_[0].position = { -1.0f,  1.0f, -1.0f, 1.0f }; // 左上後
	vertexData_[1].position = { 1.0f,  1.0f, -1.0f, 1.0f }; // 右上後
	vertexData_[2].position = { -1.0f, -1.0f, -1.0f, 1.0f }; // 左下後
	vertexData_[3].position = { 1.0f, -1.0f, -1.0f, 1.0f }; // 右下後
	vertexData_[4].position = { -1.0f,  1.0f,  1.0f, 1.0f }; // 左上前
	vertexData_[5].position = { 1.0f,  1.0f,  1.0f, 1.0f }; // 右上前
	vertexData_[6].position = { -1.0f, -1.0f,  1.0f, 1.0f }; // 左下前
	vertexData_[7].position = { 1.0f, -1.0f,  1.0f, 1.0f }; // 右下前

	// [ Buffer View ]
	//リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 頂点の総数
	vertexBufferView_.SizeInBytes = sizeof(SkyBoxVertexData) * static_cast<UINT>(vertexNum);//
	//１個当たりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(SkyBoxVertexData);//

	//   ================================
	// 【 Index Resource に関するコード 】
	//   ================================
	indexResource_ = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(uint32_t) * 36);
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
	// インデックスの定義 (内側を向くように左回りで指定)
	// 前面 (Z+)
	indexData_[0] = 4; indexData_[1] = 5; indexData_[2] = 6;
	indexData_[3] = 5; indexData_[4] = 7; indexData_[5] = 6;
	// 背面 (Z-)
	indexData_[6] = 1; indexData_[7] = 0; indexData_[8] = 3;
	indexData_[9] = 0; indexData_[10] = 2; indexData_[11] = 3;
	// 左面 (X-)
	indexData_[12] = 0; indexData_[13] = 4; indexData_[14] = 2;
	indexData_[15] = 4; indexData_[16] = 6; indexData_[17] = 2;
	// 右面 (X+)
	indexData_[18] = 5; indexData_[19] = 1; indexData_[20] = 7;
	indexData_[21] = 1; indexData_[22] = 3; indexData_[23] = 7;
	// 上面 (Y+)
	indexData_[24] = 0; indexData_[25] = 1; indexData_[26] = 4;
	indexData_[27] = 1; indexData_[28] = 5; indexData_[29] = 4;
	// 下面 (Y-)
	indexData_[30] = 6; indexData_[31] = 7; indexData_[32] = 2;
	indexData_[33] = 7; indexData_[34] = 3; indexData_[35] = 2;
	
	// [ Index Buffer View ]
	// リソースの先頭のアドレスから使う
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	// インデックス ( 面の数 * 6 = 36 )
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 36;
	// インデックスはuint32_tとする
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	//   =============================
	// 【 WVP Resource に関するコード 】
	//   =============================
	wvpResource_ = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(SkyBoxTransformationMatrix));
	//書き込むためのアドレスを取得
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	//単位行列を書き込んでいく
	wvpData_->WVP = Matrix4x4::Make::Identity();

	//   ==================================
	// 【 Material Resource に関するコード 】
	//   ==================================
	materialResource_ = CreateBufferResource(d3d12.GetDevice().Get(), sizeof(SkyBoxMaterial));
	// Maping
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	// カラーの初期化
	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };


	worldTransform_.Initialize();

	worldTransform_.set_.Scale({ 500.0f,500.0f,500.0f });
}

void SkyBox::Draw() {
	worldTransform_.LocalToWorld();

	wvpData_->WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(worldTransform_.mat_);

	auto& command = p_engine_->GetCommand();

	command.GetList().GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//RootSignalの設定
	command.GetList().GetList()->SetGraphicsRootSignature(PipelineStateObjectManager::GetInstance()->GetPSO("SkyBox").GetRootSignature().GetRS().Get());
	command.GetList().GetList()->SetPipelineState(PipelineStateObjectManager::GetInstance()->GetPSO("SkyBox").GetGPS().Get());
	// VertexBufferView(VBV)の設定
	command.GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	// IndexBufferView(IBV)の設定
	command.GetList().GetList()->IASetIndexBuffer(&indexBufferView_);
	//マテリアルCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	//wvp用のCBufferの場所を設定
	command.GetList().GetList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
	command.GetList().GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture(textureName_).GetHandleGPU());
	
	command.GetList().GetList()->DrawIndexedInstanced(36, 1, 0, 0, 0);
}