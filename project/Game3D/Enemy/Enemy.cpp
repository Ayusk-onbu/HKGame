#include "Enemy.h"
#include "CameraSystem.h"
#include "MathUtils.h"

/*
* 近接攻撃が避けるの難しいー＞あらかじめ出すか、出すとき特有の演出をいれる
* 
* 
*/

void BossEnemy::Initialize(Fngine* fngine, Player3D* target) {

    target_ = target;

    obj_ = std::make_unique<ModelObject>();
    obj_->modelName_ = "enemy";
    obj_->textureName_ = "bullet";
    obj_->Initialize(fngine);
    obj_->worldTransform_.set_.Translation({ 0.0f,0.0f,30.0f });
    obj_->worldTransform_.set_.Scale({ bossBodyRadius,bossBodyRadius,bossBodyRadius });
    // HPを初期化
    hp_ = maxHp_;

    // Colliderの初期化
    /*bodyCollider_ = std::make_unique<BossBodyCollider>(this);
    bodyCollider_->SetRadius(bossBodyRadius);

    bodyColliderObj_ = std::make_unique<ModelObject>();
    bodyColliderObj_->modelName_ = "stone";
    bodyColliderObj_->textureName_ = "GridLine";
    bodyColliderObj_->Initialize(fngine);
    bodyColliderObj_->worldTransform_.set_.Scale({ bossBodyRadius,bossBodyRadius,bossBodyRadius });
    bodyColliderObj_->SetColor({ 0.27f,0.16f,0.0f,1.0f });

    attackCollider_ = std::make_unique<AttackCollider>();
    attackCollider_->SetMyType(COL_None);
    attackCollider_->SetYourType(COL_None);
    attackCollider_->SetWorldPosition({ 0.0f,0.0f,30.0f });*/

    isViewAttack_ = false;

    // UI関係
    // [ HP ]
    mainHPBar_ = std::make_unique<SpriteObject>(fngine);
    mainHPBar_->Initialize("GridLine", SPRITE_ANCHOR_TYPE::LeftMiddle);
    mainHPBar_->worldTransform_.set_.Translation({ 640.0f - 480.0f,670.0f,0.0f });
    subHPBar_ = std::make_unique<SpriteObject>(fngine);
    subHPBar_->Initialize(mainHPBar_->textureName_, SPRITE_ANCHOR_TYPE::LeftMiddle);
    subHPBar_->SetColor({ 1.0f,0.0f,0.0f,1.0f });
    subHPBar_->worldTransform_.set_.Translation(mainHPBar_->worldTransform_.get_.Translation());

    bullets_ = std::make_unique<BulletManager>();
    bullets_->SetFngine(fngine);

    ChangeState(new BossDecisionState);

    // [ 炎 ]
    for (int i = 0;i < 4;i++) {
        std::unique_ptr<Particle> fire = std::make_unique<Particle>(fngine);
        fire->Initialize(400, "Fire");
        if (i == 0) {
            fire->SetPosition({-20.0f,0.0f,20.0f});
        }
        else if (i == 1) {
            fire->SetPosition({ 20.0f,0.0f,20.0f });
        }
        else if (i == 2) {
            fire->SetPosition({ -20.0f,0.0f,-20.0f });
        }
        else if (i == 3) {
            fire->SetPosition({ 20.0f,0.0f,-20.0f });
        }
		fire_.push_back(std::move(fire));
    }
	
}

void BossEnemy::Update(){
    if (InputManager::IsLockOn()) {
        lockOn_ = !lockOn_;
    }
    if (lockOn_) {
        Vector3 enemyPos = obj_->worldTransform_.get_.Translation();
        Vector3 playerPos = target_->GetPosition();
        // ベクトルを入手
        float dx = enemyPos.x - playerPos.x;
        float dz = enemyPos.z - playerPos.z;
        // ベクトルから向きたい方向の角度を算出
        float targetPhiRad = std::atan2(dz, dx);
        float targetPhiDeg = Rad2Deg(targetPhiRad - 135);
        // 角度を補間する（しないと位置関係の逆転の際に酔う）
        //float lockOnPhi = Lerp(CameraSystem::GetInstance()->GetActiveCamera()->GetPhi(), targetPhiDeg, 0.5f);
        // 適用
        CameraSystem::GetInstance()->GetActiveCamera()->SetPhi(targetPhiDeg);

        CameraSystem::GetInstance()->GetActiveCamera()->SetTargetPos(
            { obj_->worldTransform_.get_.Translation().x,obj_->worldTransform_.get_.Translation().y ,obj_->worldTransform_.get_.Translation().z });
        //CameraSystem::GetInstance()->GetActiveCamera()->SetRadius(30);
    }

    // 1. フレーム開始時、前フレームの移動量をリセット（新しい移動量が設定されない場合に備えて）
    moveAmount_ = { 0.0f, 0.0f, 0.0f };

    // 2. Stateの更新 (ここで State が SetMovement を呼び出す)
    if (state_) {
        state_->Update();
    }

    // 3. 実際の移動の適用
    obj_->worldTransform_.set_.Translation(obj_->worldTransform_.get_.Translation() + moveAmount_);

    bullets_->Update();

    if (IsDead()) {
        obj_->worldTransform_.set_.Rotation({ obj_->worldTransform_.get_.Rotation().x,obj_->worldTransform_.get_.Rotation().y - Deg2Rad(45) ,obj_->worldTransform_.get_.Rotation().z});
        obj_->worldTransform_.set_.Scale({ obj_->worldTransform_.get_.Scale().x - 0.02f,obj_->worldTransform_.get_.Scale().y - 0.02f ,obj_->worldTransform_.get_.Scale().z - 0.02f });
    }

    HPBarUpdate();

    for (auto& fire : fire_) {
        if (hp_ <= maxHp_ / 2.0f) {
            if (fire->name_ != "FireMax") {
                fire->name_ = "FireMax";
                fire->LoadParticle();
            }
        }

        fire->Update();
    }
}

void BossEnemy::Draw(){
    obj_->LocalToWorld();
    obj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(obj_->worldTransform_.mat_));
    obj_->Draw();

    if (isViewAttack_) {
       /* bodyColliderObj_->worldTransform_.set_.Translation(attackCollider_->GetWorldPosition());
        bodyColliderObj_->LocalToWorld();
        bodyColliderObj_->SetWVPData(CameraSystem::GetInstance()->GetActiveCamera()->DrawCamera(bodyColliderObj_->worldTransform_.mat_));
        bodyColliderObj_->Draw();*/
    }

    bullets_->Draw();

    subHPBar_->Draw();
    mainHPBar_->Draw();

    for (auto& fire : fire_) {
        fire->Draw();
    }
}

void BossEnemy::TakeDamage(float damage) {
    // 死んでいたら無視
    if (IsDead()) { return; }

    // HPを減らす
    hp_ -= damage;

    // HPで怒り状態に遷移
    if ((hp_ < maxHp_ * 0.5f) && phase_ == 1) {
        phase_ = 2;
        // 咆哮ステートに移行
        ChangeState(new BossRoarState());
    }

    // HPの下限をチェック
    if (hp_ < 0.0f) {
        hp_ = 0.0f;
    }

    // 状態遷移の判断
    if (IsDead()) {
        // HPが０になったら死亡状態へ
        ChangeState(new BossDeathState());
        return;
    }
    else {
        // のけぞり状態にしたっていい
        //ChangeState(new BossHurtState());
        return;
    }
}

void BossEnemy::EnableAttackHitBox(bool enable, const Vector3& worldPos, float radius)
{
    //if (!attackCollider_) return;

    //if (enable) {
    //    // 1. 位置と半径を設定
    //    attackCollider_->SetWorldPosition(worldPos);
    //    attackCollider_->SetRadius(radius);

    //    // 2. 攻撃判定をアクティブにする
    //    // 自身の属性をボスの攻撃 (COL_ENEMY_ATTACK) に設定
    //    attackCollider_->SetMyType(COL_Enemy_Attack);

    //    // 3. マスクを設定
    //    // プレイヤー本体 (COL_PLAYER) に当たるように設定
    //    attackCollider_->SetYourType(COL_Player);
    //}
    //else {
    //    // 攻撃判定を非アクティブにする
    //    // 自身の属性を COL_NONE に設定することで、CollisionManagerでのチェックをスキップさせる
    //    attackCollider_->SetMyType(COL_None);
    //    attackCollider_->SetYourType(COL_None);
    //}
}

void BossEnemy::ChangeState(BossState* newState) {
    if (state_) {
        state_->Exit();

        delete state_;
        state_ = nullptr;
    }
    state_ = newState;
    state_->SetBoss(this);
    state_->Initialize();
}

void BossEnemy::LookAtTarget()
{
    // 1. 方向ベクトルを計算 (水平方向のみ)
    Vector3 bossPos = obj_->worldTransform_.GetWorldPos();
    Vector3 targetPos = GetTargetPosition();

    if (std::isnan(targetPos.x) || std::isnan(targetPos.y) || std::isnan(targetPos.z)) {
        // プレイヤーがNaNなので、これ以上計算せず、回転処理をスキップ
        return;
    }

    Vector3 direction = targetPos - bossPos;
    direction.y = 0.0f; // 水平方向のみを考慮
    direction = Normalize(direction); // Normalize関数を使用

    // 2. Y軸周りの回転角度を計算 (Z軸を前方基準として)
    // atan2(X, Z) はZ軸正方向からY軸周りの回転角度を求める
    float angle = std::atan2(direction.x, direction.z);

    // 3. Y軸回転のクォータニオンを生成
    Quaternion targetRotationQ = Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f, 1.0f, 0.0f }, angle);

    // 4. WorldTransformに設定
    obj_->worldTransform_.set_.Quaternion(targetRotationQ);
}

Vector3 BossEnemy::GetForwardVector()
{
    // 1. 現在の回転クォータニオンを取得
    Quaternion currentRotationQ = obj_->worldTransform_.get_.Quaternion();

    // 2. ローカル座標系での前方ベクトル（Z軸）
    Vector3 forward = { 0.0f, 0.0f, 1.0f };

    // 3. クォータニオンを使ってベクトルを回転させる
    Vector3 worldForward = Quaternion::RotateVector(forward, currentRotationQ);

    // 4. 必要に応じて正規化（移動に使う場合、既に正規化されている可能性が高いが念のため）
    // worldForward = Normalize(worldForward); 

    return worldForward;
}

void BossEnemy::HPBarUpdate() {
    float hpWidth = hp_ / 16.0f / 500.0f * 960.0f;
    mainHPBar_->worldTransform_.set_.Scale({ hpWidth,1.0f,0.0f });
    subHPBar_->worldTransform_.set_.Scale({ Easing_Float(subHPBar_->worldTransform_.get_.Scale().x, hpWidth, 0.2f, 1.0f, EASINGTYPE::InSine),1.0f, 1.0f });
    if (hp_ > maxHp_ * 0.5f) {
        mainHPBar_->SetColor({ 0.1f,1.0f,0.2f,1.0f });
    }
    else if (hp_ > maxHp_ * 0.2f) {
        mainHPBar_->SetColor({ 0.65f,0.65f,0.1f,1.0f });
    }
    else {
        mainHPBar_->SetColor({ 0.8f,0.1f,0.1f,1.0f });
    }
}

bool BossEnemy::StartCutscene(float time) {
    bool isEnd = false;
    // なぜか武器が現れるので無理やり隠す
    isViewAttack_ = false;
    // カメラをもとに戻すのが一旦面倒なのでここで隠す
    Vector3 enemyPos = obj_->worldTransform_.get_.Translation();
    Vector3 playerPos = target_->GetPosition();
    // [ ベクトルを入手 ]
    float dx = enemyPos.x - playerPos.x;
    float dz = enemyPos.z - playerPos.z;
    // [ ベクトルから向きたい方向の角度を算出 ]
    float targetPhiRad = std::atan2(dz, dx);
    float targetPhiDeg = Rad2Deg(targetPhiRad - 135);
    // [ 適用 ]
    CameraSystem::GetInstance()->GetActiveCamera()->SetPhi(targetPhiDeg);

    if (time < 2.0f) {
        CameraSystem::GetInstance()->GetActiveCamera()->SetTargetPos(obj_->worldTransform_.get_.Translation() +
            Vector3{ RandomUtils::GetInstance()->GetHighRandom().GetFloat(-0.2f,0.2f), RandomUtils::GetInstance()->GetHighRandom().GetFloat(-0.2f,0.2f),0.0f });
    }
    else {
        CameraSystem::GetInstance()->GetActiveCamera()->SetTargetPos(obj_->worldTransform_.get_.Translation());
    }

    if (time > 3.0f) {
        isEnd = true;
    }

    return isEnd;
}