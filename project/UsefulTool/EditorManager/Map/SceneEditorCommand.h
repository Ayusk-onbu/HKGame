#pragma once
#include "ICommand.h"
#include "InGame/Map/SceneMap.h"

using ObjectPtrVector = std::vector<DynamicObject*>;

// トランスフォーム保存用の簡易構造体
struct TransformState {
    Vector3 position{ 0,0,0 };
    Vector3 rotation{ 0,0,0 };
    Vector3 scale{ 1,1,1 };
};

// =================================================================
// Transform変更コマンド（位置・回転・スケールのUndo/Redo）
// =================================================================
class DynamicObjectTransformCommand : public ICommand {
public:
    DynamicObjectTransformCommand(DynamicObject* obj, TransformState oldState, TransformState newState)
        : obj_(obj), oldState_(oldState), newState_(newState) {
    }

    void Execute() override { ApplyState(newState_); }
    void Undo() override { ApplyState(oldState_); }
    void Redo() override { Execute(); }

private:
    void ApplyState(const TransformState& state) {
        if (!obj_) return;
        obj_->SetPosition(state.position);
        obj_->SetRotation(state.rotation);
        obj_->SetScale(state.scale);
    }

private:
    DynamicObject* obj_;
    TransformState oldState_;
    TransformState newState_;
};

// =================================================================
// オブジェクト追加コマンド（アドレス固定・Undo/Redo対応）
// =================================================================
class SceneObjectAddCommand : public ICommand {
public:
    SceneObjectAddCommand(SceneMap* sceneMap, const std::string& name = "New Object")
        : sceneMap_(sceneMap), objectName_(name) {
    }

    void Execute() override {
        if (!sceneMap_) return;

        if (!heldObj_) {
            // 初回実行：SceneMap に生成させ、生ポインタを記憶
            rawPtr_ = sceneMap_->SpawnObject(objectName_);
        }
        else {
            // Redo時：Command 内で保管していたオブジェクトを SceneMap へ復元（アドレスはそのまま！）
            rawPtr_ = heldObj_.get();
            sceneMap_->RestoreObject(std::move(heldObj_));
        }
    }

    void Undo() override {
        if (sceneMap_ && rawPtr_) {
            // Undo時：delete はせず、SceneMap から Extract して Command 内で保管
            heldObj_ = sceneMap_->ExtractObject(rawPtr_);
        }
    }

    void Redo() override { Execute(); }

private:
    SceneMap* sceneMap_;
    std::string objectName_;
    DynamicObject* rawPtr_ = nullptr;
    std::unique_ptr<DynamicObject> heldObj_ = nullptr; // Undo 中に所有権を保持
};

// =================================================================
// オブジェクト削除コマンド（アドレス固定・Undo/Redo対応）
// =================================================================
class SceneObjectDeleteCommand : public ICommand {
public:
    SceneObjectDeleteCommand(SceneMap* sceneMap, DynamicObject* target)
        : sceneMap_(sceneMap), targetObj_(target) {
    }

    void Execute() override {
        if (sceneMap_ && targetObj_) {
            // Execute / Redo時：SceneMap から所有権を引き抜いて Command で保管（ delete はしない！）
            deletedObj_ = sceneMap_->ExtractObject(targetObj_);
        }
    }

    void Undo() override {
        if (sceneMap_ && deletedObj_) {
            // Undo時：保管していたオブジェクトを SceneMap に戻す
            sceneMap_->RestoreObject(std::move(deletedObj_));
        }
    }

    void Redo() override { Execute(); }

private:
    SceneMap* sceneMap_;
    DynamicObject* targetObj_;
    std::unique_ptr<DynamicObject> deletedObj_ = nullptr; // 削除された実体を保持
};

// =================================================================
// コンポーネント追加コマンド（アドレス固定・Undo/Redo対応）
// =================================================================
class AddComponentCommand : public ICommand {
public:
    AddComponentCommand(DynamicObject* target, const std::string& compName)
        : target_(target), compName_(compName) {
    }

    void Execute() override {
        if (!target_) return;

        if (!heldComp_) {
            // 初回実行：コンポーネント生成
            addedCompPtr_ = target_->AddComponent(compName_);
            if (addedCompPtr_) {
                addedCompPtr_->Initialize();
            }
        }
        else {
            // Redo時：保管していたコンポーネントを復元
            addedCompPtr_ = target_->RestoreComponent(std::move(heldComp_));
        }
    }

    void Undo() override {
        if (target_ && addedCompPtr_) {
            // Undo時：DynamicObject から引き抜いて Command 内に保管
            heldComp_ = target_->ExtractComponent(addedCompPtr_);
        }
    }

    void Redo() override { Execute(); }

private:
    DynamicObject* target_;
    std::string compName_;
    Component* addedCompPtr_ = nullptr;
    std::unique_ptr<Component> heldComp_ = nullptr; // Undo 中に保持
};