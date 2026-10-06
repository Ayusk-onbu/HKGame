#pragma once
#include "DynamicObject.h"

#include <vector>
#include <memory>

class SceneMap {
public:
    SceneMap() = default;
    ~SceneMap() = default;

public:
    /// <summary>
    /// ファイルからデータを取得
    /// </summary>
    void Initialize();

    /// <summary>
    /// DynamicObjectをすべて更新
    /// </summary>
    void Update();

public:
    // 全オブジェクトのクリア
    void Clear() {
        objects_.clear();
    }

    // オブジェクトの追加・削除
    DynamicObject* SpawnObject(const std::string& name = "New Object");

    // オブジェクトの所有権を抽出して渡す（SceneMapのリストからは外れるがメモリは解放されない）
    std::unique_ptr<DynamicObject> ExtractObject(DynamicObject* target) {
        auto it = std::find_if(objects_.begin(), objects_.end(),
            [target](const auto& ptr) { return ptr.get() == target; });

        if (it != objects_.end()) {
            std::unique_ptr<DynamicObject> ret = std::move(*it);
            objects_.erase(it);
            return ret;
        }
        return nullptr;
    }

    // 保持していたオブジェクトの所有権を SceneMap に戻す
    void RestoreObject(std::unique_ptr<DynamicObject> obj) {
        if (obj) {
            objects_.push_back(std::move(obj));
        }
    }

    void RemoveObject(DynamicObject* target) {
        std::erase_if(objects_, [target](const auto& obj) {
            return obj.get() == target;
        });
    }

    // 毎フレームの更新・描画
    void Update(float deltaTime) {
        for (auto& obj : objects_) {
            obj->Update(deltaTime);
        }
    }

    // エディタへ生ポインタのリストを渡す用
    std::vector<DynamicObject*> GetRawObjects() {
        std::vector<DynamicObject*> rawList;
        for (auto& obj : objects_) {
            rawList.push_back(obj.get());
        }
        return rawList;
    }

    // --- シーン全体の保存 / 読み込み ---
    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath, Fngine* engine);

    const std::string& GetMapName() const { return mapName_; }
    void SetMapName(const std::string& name) { mapName_ = name; }
private:
    std::string mapName_ = "NewMap";
    std::vector<std::unique_ptr<DynamicObject>> objects_;
};