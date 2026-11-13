#include "ComponentFactory.h"
#include "Render/RenderComponent.h"
#include "Physics/RigidBody.h"
#include "Physics/ColliderComponent.h"
#include "Status/Status.h"
#include "Status/DamageSource.h"
#include "State/Movement/Movement.h"
#include "Controller/Controller.h"
#include "Skinning/SkinningComponent.h"
#include "Render/RayTracingComponent.h"
#include "SDF/SDFFunctionComponent.h"

void ComponentFactory::Initialize() {
    // --------------------------------------------------
    // 描画系コンポーネントの登録
    // --------------------------------------------------
    Register("Render", []() {
        auto comp = std::make_unique<RenderComponent>();
        return comp;
    });

    // --------------------------------------------------
    // 物理系コンポーネント (RigidBody)
    // --------------------------------------------------
    Register("RigidBody", []() {
        auto comp = std::make_unique<RigidBody>();
        comp->Initialize();
        return comp;
        });

    // --------------------------------------------------
    // 衝突判定系コンポーネント (ColliderComponent)
    // --------------------------------------------------
    Register("ColliderComponent", []() {
        auto comp = std::make_unique<ColliderComponent>();
        comp->Initialize();
        return comp;
        });

    // --------------------------------------------------
    // ステータス系コンポーネント (StatusComponent)
    // --------------------------------------------------
    Register("StatusComponent", []() {
        auto comp = std::make_unique<StatusComponent>();
        comp->Initialize();
        return comp;
        });

    // --------------------------------------------------
    // ダメージソース系コンポーネント (DamageSourceComponent)
    // --------------------------------------------------
    Register("DamageSourceComponent", []() {
        auto comp = std::make_unique<DamageSourceComponent>();
        comp->Initialize();
        return comp;
        });

    // --------------------------------------------------
    // 移動系コンポーネント (MovementComponent)
    // --------------------------------------------------
    Register("MovementComponent", []() {
        auto comp = std::make_unique<MovementComponent>();
        comp->Initialize();
        return comp;
    });

    // --------------------------------------------------
    // 脳みそコンポーネント (ControllerComponent)
    // --------------------------------------------------
    Register("ControllerComponent", []() {
        auto comp = std::make_unique<ControllerComponent>();
        comp->Initialize();
        return comp;
        });

    // --------------------------------------------------
    // スキニングコンポーネント (SkinningComponent) 
    // --------------------------------------------------
    Register("SkinningComponent", []() {
        auto comp = std::make_unique<SkinningComponent>();
        comp->Initialize();
        return comp;
    });

    // --------------------------------------------------
    // レイトレーシングコンポーネント (RayTracingComponent)
    // --------------------------------------------------
    Register("RayTracingComponent", []() {
        auto comp = std::make_unique<RayTracingComponent>();
        comp->Initialize();
        return comp;
    });

    // --------------------------------------------------
    // SDF関数コンポーネント (SDFFunctionComponent)
    // --------------------------------------------------
    Register("SDFFunctionComponent", []() {
        auto comp = std::make_unique<SDFFunctionComponent>();
        comp->Initialize();
        return comp;
    });
}

std::unique_ptr<Component> ComponentFactory::Create(const std::string& name) {
    auto it = registry_.find(name);
    if (it != registry_.end()) {
        return it->second(); // 登録された生成処理（ラムダ）を実行して返す！
    }

    // 見つからなかった場合
    return nullptr;
}