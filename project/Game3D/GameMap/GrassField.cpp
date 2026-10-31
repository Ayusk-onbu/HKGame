#include "GrassField.h"
#include "DrawManager.h"
#include "TextureManager.h"

void GrassField::Initialize(Fngine* engine, uint32_t maxGrass, const std::string& densityMapName) {
	// 【ノイズテクスチャを使った草の群生配置】
	grassConfig_.shapeType = EmitterShapeType::Noise;
	grassConfig_.noiseTextureName = densityMapName; // 白黒の配置マップ
	grassConfig_.noiseThreshold = 0.4f;             // 白い部分(threshold以上)にだけ配置
	grassConfig_.size = { 10.0f, 10.0f, 0.0f };     // size.x * size.yの範囲に散布

	// エミッターに座標を計算してもらう
	std::vector<EmissionResult> spawnPoints =
		EmitterSystem::GetInstance()->GeneratePositions(grassConfig_, maxGrass);

	auto& rand = RandomUtils::GetInstance()->GetHighRandom();

	// 使いたいTextureのIndex
	auto texture = TextureManager::GetInstance();
	uint32_t gridlineIndex = texture->GetTexture("GridLine").GetSrvIndex();
	uint32_t monsterBallIndex = texture->GetTexture("monsterBall").GetSrvIndex();

	// 配置データの作成
	for (const auto& point : spawnPoints) {
		GrassObjectData grass;
		grass.worldTransform.Initialize();

		// エミッターのXY座標を、地面のXZ座標に変換
		grass.worldTransform.set_.Translation({ point.position.x - 5.0f, 1.0f, point.position.y });
		// スケールや向きをランダムにばらけさせる
		grass.worldTransform.set_.Scale({ 1.0f, rand.GetFloat(0.6f, 1.4f), 1.0f });
		grass.worldTransform.set_.Rotation({ 0.0f, rand.GetFloat(0.0f, 3.141592f * 2.0f), 0.0f });
		grass.worldTransform.LocalToWorld();

		// 緑色に個体差を持たせる
		grass.color = { 0.1f, rand.GetFloat(0.6f, 1.0f), 0.2f, 1.0f };

		// 風の揺れのタイミングをバラバラにする
		grass.windPhase = rand.GetFloat(0.0f, 6.28f);

		if (grass.color.y <= 0.85f) {
			grass.textureIndex = gridlineIndex;
		}
		else {
			grass.textureIndex = monsterBallIndex;
		}

		staticGrassList_.push_back(grass);
	}
}

void GrassField::Update() {
	// 毎フレーム、配置済みの草を描画リストに登録する
	for (const auto& grass : staticGrassList_) {
		DrawManager::GetInstance()->GetGrass()->AddInstance(grass);
	}

	// Debug 用になんか用意しても可
}