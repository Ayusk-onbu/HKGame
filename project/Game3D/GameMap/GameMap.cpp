#include "GameMap.h"

void GameMap::Initialize(Fngine* engine) {
	obj_ = std::make_unique<ModelObject>();
	obj_->textureName_ = "GridLine";
	obj_->modelName_ = "Map";
	obj_->Initialize(engine);

	// マップの大きさを設定する（したいなら）

	// Matrixを構築
	obj_->worldTransform_.LocalToWorld();

	// 当たり判定用のBVHを作成
	bvh_ = std::make_unique<BVH>();
	
	auto triangle = obj_->ExtractPhysicsTriangles();

	bvh_->Build(triangle);
}

void GameMap::Update() {

}

void GameMap::Draw() {
	obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
	obj_->Draw();
}