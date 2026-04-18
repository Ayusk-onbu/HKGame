#pragma once
#include "Fngine.h"
#include "Scene.h"
#include "WorldTransform.h"
#include "Player3D.h"
#include "Enemy.h"
#include "CollisionManager.h"
#include "SpriteObject.h"
#include "Particle.h"
#include "Ground.h"
#include "UIContainer.h"
#include "GameMap.h"

class GameScene 
	:public Scene
{
public:
	GameScene();
	~GameScene()override;
public:// Sceneにまつわる関数
	void Initialize()override;
	void Update()override;
	void Draw()override;
	void PauseUpdate()override;
	void PauseDraw()override;
	bool CanPause()const override { return true; }
private:
	void CollisionCheck();
private:// ポーズ関係
	float startPauseTimer_ = 0.0f;
	//std::unique_ptr<SpriteObject>pause_;
	std::unique_ptr<UIContainer>pause_;
	std::unique_ptr<UIContainer>playGuide_;
private:
	void ToScene();
	void ToClearScene();
	void ToGameOverScene();
	float toSceneTimer_ = 0.0f;
private:
	// Fade関係
	void FirstFade(float time);
	std::unique_ptr<SpriteObject>fadeUp_;
	std::unique_ptr<SpriteObject>fadeDown_;
	float toGameTimer_ = 0.0f;
	bool notGame_ = true;
private:
	// UI関係
	std::unique_ptr<SpriteObject>playUI_;
	std::unique_ptr<SpriteObject>purposeUI_;
private:

	std::unique_ptr<Player3D>player_;
	std::unique_ptr<BossEnemy>boss_;
	std::unique_ptr<CollisionManager>collisionManager_;
	std::unique_ptr<GameMap>gameMap_;
	std::unique_ptr<ConvenienceModel>skySphere_;
};



