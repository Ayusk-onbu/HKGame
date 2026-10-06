#include "SceneMap.h"
#include "Chronos.h"

void SceneMap::Initialize() {

}

void SceneMap::Update() {
	float deltaTime = Chronos::GetInstance()->GetGameDeltaTime();
	for (auto& obj : objects_) {
		obj->Update(deltaTime);
	}
}

DynamicObject* SceneMap::SpawnObject(const std::string& name) {
    auto newObj = std::make_unique<DynamicObject>();
    DynamicObject* ptr = newObj.get();

    ptr->Initialize(nullptr);
    ptr->SetName(name);

    objects_.push_back(std::move(newObj));
    return ptr;
}

// --- マップ全体のセーブ処理 ---
bool SceneMap::SaveToFile(const std::string& filepath) const {
    json rootJson;
    rootJson["mapName"] = mapName_;
    rootJson["objectsCount"] = objects_.size();

    json objectsArray = json::array();
    for (const auto& obj : objects_) {
        if (obj) {
            objectsArray.push_back(obj->Serialize());
        }
    }
    rootJson["objects"] = objectsArray;

    // ファイルへの書き込み（4文字インデントで整列）
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for saving: " << filepath << std::endl;
        return false;
    }

    file << rootJson.dump(4);
    file.close();
    return true;
}

// --- マップ全体のロード処理 ---
bool SceneMap::LoadFromFile(const std::string& filepath, Fngine* engine) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for loading: " << filepath << std::endl;
        return false;
    }

    json rootJson;
    try {
        file >> rootJson;
    }
    catch (const json::parse_error& e) {
        std::cerr << "JSON Parse Error: " << e.what() << std::endl;
        return false;
    }
    file.close();

    // 既存のオブジェクトをクリアしてロード状態にリセット
    objects_.clear();

    if (rootJson.contains("mapName")) {
        mapName_ = rootJson["mapName"].get<std::string>();
    }

    if (rootJson.contains("objects") && rootJson["objects"].is_array()) {
        for (const auto& objJson : rootJson["objects"]) {
            auto newObj = std::make_unique<DynamicObject>();

            // エンジン基本初期化が必要な場合は呼ぶ
            if (engine) {
                newObj->Initialize(engine);
            }

            // JSON から座標やコンポーネントを全復元
            newObj->Deserialize(objJson);

            objects_.push_back(std::move(newObj));
        }
    }

    return true;
}