#include "Particle.h"
#include "CameraSystem.h"
#include "TextureManager.h"
#include "Easing.h"

using json = nlohmann::json;

Particle::Particle(Fngine* fngine)
	: p_fngine_(fngine){

}

// ---------------------------
// 特に特別でもない関係の処理
// ---------------------------

// [ 初期化処理 ]
void Particle::Initialize(uint32_t numInstance, const std::string& name, const std::string& textureName) {

	// *** パラメータの設定 ***
	// [ インスタンス数 ]
	numMaxInstance_ = numInstance;

	// [ 使うTextureの設定 ]
	textureName_ = textureName;

	// *** Structured 関係の初期化処理 ***
	// [ メモリの確保 ]
	// [ メモリへのアクセス手段の確保 ]
	vertexResource_ = CreateBufferResource(p_fngine_->GetD3D12System().GetDevice().Get(), sizeof(VertexData) * 4);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	indexResource_ = CreateBufferResource(p_fngine_->GetD3D12System().GetDevice().Get(), sizeof(uint32_t) * 6);
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

	materialResource_ = CreateBufferResource(p_fngine_->GetD3D12System().GetDevice().Get(), sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	// マテリアルの初期化
	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = Matrix4x4::Make::Identity();
	materialData_->shininess = 0.0f;

	// インスタンシングの初期化
	instancingBuffer_ = std::make_unique<Structured<ParticleForGPU>>(p_fngine_);
	instancingBuffer_->Initialize(numMaxInstance_);

	ParticleForGPU* instancingData = instancingBuffer_->GetMappedData();

	// インデックスの初期化
	for (uint32_t index = 0;index < numMaxInstance_;index++) {
		instancingData[index].WVP = Matrix4x4::Make::Identity();
		instancingData[index].World = Matrix4x4::Make::Identity();
		instancingData[index].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// *** View関係の初期化処理 *** ここから下の処理を関数の方に置き換えして色々な形で使えるようにしたらよさげ
	// [ リソースの先頭のアドレスから使う ]
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// [ 使用するリソースのサイズは頂点三つ分 ]
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;//
	// [ １個当たりのサイズ ]
	vertexBufferView_.StrideInBytes = sizeof(VertexData);//
	// [ リソースの先頭のアドレスから使う ]
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	// [ 使用するリソースのサイズはインデックス６つ分のサイズ ]
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	// [ インデックスはuint32_tとする ]
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// プレーンの初期化
	vertexData_[0].position = { 0.0f - 0.5f,height_ - 0.5f,0.0f,1.0f };
	vertexData_[0].texcoord = { 0.0f,1.0f };
	vertexData_[1].position = { 0.0f - 0.5f,0.0f - 0.5f,0.0f,1.0f };
	vertexData_[1].texcoord = { 0.0f,0.0f };
	vertexData_[2].position = { width_ - 0.5f,height_ - 0.5f,0.0f,1.0f };
	vertexData_[2].texcoord = { 1.0f,1.0f };
	vertexData_[3].position = { width_ - 0.5f, 0.0f - 0.5f, 0.0f, 1.0f };
	vertexData_[3].texcoord = { 1.0f, 0.0f };

	indexData_[0] = 0;indexData_[1] = 1;indexData_[2] = 2;
	indexData_[3] = 1;indexData_[4] = 3;indexData_[5] = 2;

	// -----------------------------
	// Emitter
	// -----------------------------

	// [ 使うパーティクルの設定 ]
	name_ = name;

	// 将来的にJsonファイルに記した情報からパーティクルを作れるようにする
	LoadParticle();

	// [ 位置情報の初期化 ]
	transform_.Initialize();
}

void Particle::Update() {
	// *** 更新処理 ***
	
	// [ 経過時間の設定 ]
	const float kDeltaTime_ = 1.0f / 60.0f;

	// [ エミッターの更新処理 ]
	for (auto& emitter : emitters_) {
		emitter->Update(this);
	}

	// [ *** パーティクルの更新処理 *** ]
	// [ 先頭アドレスの入手 ]
	auto it = info_.begin();
	// [ 更新 ]
	while (it != info_.end()) {
		// [ パーティクルの情報を入手 ]
		ParticleData* info = it->get();

		// [ Force Fieldの適用 ]
		for (auto& field : forceFields_) {
			field->ApplyForce(info);
		}

		// [ *** Particle Data の更新 ]
		// [ 位置の更新 ]
		info->worldTransform.set_.Translation(
			info->worldTransform.get_.Translation() + (info->velocity) * kDeltaTime_);

		// [ ビルボードの処理 ]
		info->worldTransform.LookAtToVector(CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation());

		// [ *** イージング関係の処理 *** ]
		// [ Scale ]
		float currentScale = Easing_Float(info->startScale, info->endScale, info->currentTime, info->lifeTime, EASINGTYPE::None);
		info->worldTransform.set_.Scale({ currentScale,currentScale,currentScale });
		// [ Color ]
		info->color = Easing_Vector4(info->startColor, info->endColor, info->currentTime, info->lifeTime, EASINGTYPE::None);

		// [ *** 死亡判定 ***]
		if (info->lifeTime <= info->currentTime) {
			// 死亡したBulletをリストから削除
			// unique_ptr なので、リストから削除されると自動でデストラクタが呼ばれメモリ解放される
			it = info_.erase(it);
		}
		else {
			// 削除されなかった場合はイテレータを進める
			++it;
		}
	}
	ImGui();
}

void Particle::Draw() {
	uint32_t numInstance = 0;
	uint32_t index = 0;

	transform_.isDirty_ = true;
	transform_.LocalToWorld();

	ParticleForGPU* instancingData = instancingBuffer_->GetMappedData();

	for (auto& info : info_) {
		info->currentTime += 1.0f / 60.0f;
		info->worldTransform.LocalToWorld();
		Matrix4x4 mat = info->worldTransform.mat_ * transform_.mat_;
		instancingData[index].WVP = CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(mat);
		instancingData[index].World = mat;
		instancingData[index].color = info->color;
		index++;
		numInstance++;
	}
	if (numInstance > 0) {
		p_fngine_->GetCommand().GetList().GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		p_fngine_->GetCommand().GetList().GetList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//RootSignalの設定
		p_fngine_->GetCommand().GetList().GetList()->SetGraphicsRootSignature(PSOManager::GetInstance()->GetPSO("Structured").GetRootSignature().GetRS().Get());
		p_fngine_->GetCommand().GetList().GetList()->SetPipelineState(PSOManager::GetInstance()->GetPSO("Structured").GetGPS().Get());
		p_fngine_->GetCommand().GetList().GetList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
		// IndexBufferView(IBV)の設定
		p_fngine_->GetCommand().GetList().GetList()->IASetIndexBuffer(&indexBufferView_);
		//マテリアルCBufferの場所を設定
		p_fngine_->GetCommand().GetList().GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
		//wvp用のCBufferの場所を設定
		p_fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(1, instancingBuffer_->GetSRVHandleGPU());
		//SRVのDescritorTableの先頭を設定。2はrootParameter[2]である
		p_fngine_->GetCommand().GetList().GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTexture(textureName_).GetHandleGPU());

		p_fngine_->GetCommand().GetList().GetList()->DrawIndexedInstanced(6, numInstance, 0, 0, 0);

		DrawDebug();
	}

}

void Particle::DrawDebug() {
#ifdef USE_IMGUI
	// DrawDebugの描画
	// [ Emitter ]
	for (const auto& emitter : emitters_) {
		emitter->DrawDebug();
	}

	// [ ForceField ]
	for (const auto& filed : forceFields_) {
		filed->DrawDebug();
	}
#endif//USE_IMGUI
}

void Particle::ImGui() {
#ifdef USE_IMGUI
	// Editter関係
	int size = static_cast<int>(info_.size());
	std::string beginName = "Particle" + name_;
	ImGui::Begin(name_.c_str());
	ImGui::SliderInt("Num", &size, 0, 0);
	// 1. 名前の入力フィールド
	// std::stringのポインタを渡すオーバーロードを使用
	char buf[128];
	strncpy_s(buf, sizeof(buf), name_.c_str(), _TRUNCATE);
	buf[sizeof(buf) - 1] = 0; // 確実にNULL終端
	if (ImGui::InputText("Particle Name", buf, sizeof(buf))) {
		// 入力が変更されたらstd::stringを更新
		name_ = buf;
	}
	strncpy_s(buf, sizeof(buf), input_name_buffer.c_str(), _TRUNCATE);
	buf[sizeof(buf) - 1] = 0; // 確実にNULL終端
	if (ImGui::InputText("Emitter Name", buf, sizeof(buf))) {
		// 入力が変更されたらstd::stringを更新
		input_name_buffer = buf;
	}
	if (ImGui::Button("Gravity")) {
		AddGravityForce(input_name_buffer);
	}
	ImGui::SameLine();
	if (ImGui::Button("Point")) {
		AddPointForce(input_name_buffer);
	}
	ImGui::SameLine();
	if (ImGui::Button("Emitter")) {
		AddParticleEmitter(input_name_buffer);
	}
	ImGui::DragFloat3("pos", &transform_.transform_.translation_.x);
	if (ImGui::Button("Save")) {
		SaveParticle();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		LoadParticle();
	}
	ImGui::SameLine();
	if (ImGui::Button("EmitterClear")) {
		emitters_.clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("FieldClear")) {
		forceFields_.clear();
	}

	if (ImGui::TreeNode("ForceField")) {
		for (const auto& field : forceFields_) {
			if (ImGui::TreeNode(field->GetName().c_str())) {
				field->DrawImGui();
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Emitters")) {
		for (const auto& emitter : emitters_) {
			if (ImGui::TreeNode(emitter->name_.c_str())) {
				emitter->DrawImGui();
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	ImGui::End();
#endif
}

// ---------------------
// Add 関係の処理
// ---------------------

void Particle::AddParticle(std::unique_ptr<ParticleData> data)
{
	// 1. 最大インスタンス数に達していないかチェック
	if (info_.size() < numMaxInstance_) {
		info_.push_back(std::move(data));
	}
	// 3. 最大数に達している場合（else:）、
	//    渡された unique_ptr (data) は、リストに追加されずに
	//    この関数のスコープを抜けるときに自動的に解放
}

void Particle::AddParticleEmitter(std::string name) {
	std::unique_ptr<ParticleEmitter> newEmitter = std::make_unique<ParticleEmitter>();
	newEmitter->Initialize(p_fngine_);
	newEmitter->worldTransform_.Initialize();
	newEmitter->worldTransform_.set_.Translation({ 0.0f,0.0f,0.0f });
	newEmitter->emitNum_ = 3;
	newEmitter->SetMinLifeTime(2.0f);
	newEmitter->SetMaxLifeTime(10.0f);
	// Scaleも設定出来ないとつらい
	newEmitter->name_ = name;
	emitters_.push_back(std::move(newEmitter));
}

void Particle::AddPointForce(std::string name) {
	std::unique_ptr<PointForceField>attractor = std::make_unique<PointForceField>(20.0f);
	attractor->Initialize(p_fngine_);
	attractor->worldTransform_.Initialize();
	attractor->worldTransform_.set_.Translation({ 0.0f,10.0f,0.0f });
	attractor->radius_ = 50.0f;
	attractor->name_ = name;
	forceFields_.push_back(std::move(attractor));
}

void Particle::AddGravityForce(std::string name) {
	std::unique_ptr<GravityForceField>gravity = std::make_unique<GravityForceField>(Vector3{ 0.0f,-9.8f,0.0f });
	gravity->Initialize(p_fngine_);
	gravity->radius_ = 5.0f;
	gravity->name_ = name;
	forceFields_.push_back(std::move(gravity));
}

// -------------------------
// Private Function
// -------------------------

void Particle::SaveParticle() {
	json root;

	// 1, Emitterを保存
	for (auto& emitter : emitters_) {
		root["emitters"].push_back(emitter->SaveData());
	}

	// 2, ForceFieldを保存
	for (auto& forceField : forceFields_) {
		root["forceFields"].push_back(forceField->SaveData());
	}

	// 3, ファイルに書き出す
	std::string filePath = "resources/Particle/Data";
	
	// ディレクトリが無ければ作成する
	std::filesystem::path dir(filePath);
	if (!std::filesystem::exists(dir)) {
		std::filesystem::create_directory(dir);
	}
	// 保存する名前を設計
	filePath = filePath + "/" + name_ + ".json";
	
	// 出力するファイルの決定
	std::ofstream file(filePath);
	
	// ファイルが開けるか確認する
	if (file.is_open()) {
		// ファイルにデータを保存する
		file << root.dump(4);
	}
}

void Particle::LoadParticle() {
	// ここでParticleに応じたパスを作成する
	std::string filePath = "resources/Particle/Data/" + name_ + ".json";
	//std::string filePath = "resources/Particle/Data/Config.json";
	
	// Input File Stream で引数にいれたPathを開く
	std::ifstream file(filePath);
	
	// ファイルが開けなかったら処理を行わない
	if (!file.is_open()) {
		return;
	}

	// データの受け取り窓口を作成
	json root;
	
	// fileのデータを取得
	file >> root;

	// 既存のリストはクリア
	emitters_.clear();
	forceFields_.clear();

	// 1, Emitterの復元
	for (auto& eData : root["emitters"]) {
		auto emitter = std::make_unique<ParticleEmitter>();
		emitter->Initialize(p_fngine_);
		emitter->worldTransform_.Initialize();
		emitter->LoadData(eData);
		emitters_.push_back(std::move(emitter));
	}

	for (auto& fieldData : root["forceFields"]) {
		int typeInt = fieldData["type"];
		ForceField::Type type = static_cast<ForceField::Type>(typeInt);
		if (type == ForceField::Type::kGravity) {
			// 重力場を再生
			auto gravity = std::make_unique<GravityForceField>();
			gravity->Initialize(p_fngine_);
			gravity->LoadData(fieldData);
			forceFields_.push_back(std::move(gravity));
		}
		else if (type == ForceField::Type::kPoint) {
			// ポイントフォースとして生成
			auto point = std::make_unique<PointForceField>();
			point->Initialize(p_fngine_);
			point->LoadData(fieldData); // 個別のパラメータを読み込む
			forceFields_.push_back(std::move(point));
		}
	}
}