#pragma once
#include "Fngine.h"
#include "Texture.h"
#include <string>
#include <vector>
#include <map>
#include "../../../UsefulTool/ISingleton.h"

class TextureManager :
	public ISingleton<TextureManager>
{
public:
	friend class ISingleton<TextureManager>;
public:
	TextureManager() = default;
public:
	void Initialize(Fngine& fngine);
	std::string LoadTexture(const std::string& filename,const std::string& filePath);
	Texture& GetTexture(const std::string& name);
	std::unordered_map<std::string, std::unique_ptr<Texture>>& GetData() { return textures_; }

    // 登録されているテクスチャ名の一覧を取得
    std::vector<std::string> GetTextureNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : textures_) {
            names.push_back(name);
        }
        return names;
    }

    // SRVインデックス(Bindless ID)からテクスチャ名を取得（逆引き用）
    std::string GetTextureNameBySrvIndex(uint32_t srvIndex) const {
        for (const auto& [name, tex] : textures_) {
            if (tex && tex->GetSrvIndex() == srvIndex) {
                return name;
            }
        }
        return "None / Unknown";
    }

	uint32_t GetTextureSrvIndexByName(const std::string& name) const {
		auto it = textures_.find(name);
		if (it != textures_.end() && it->second) {
			return it->second->GetSrvIndex();
		}
		return UINT32_MAX; // Return an invalid index if not found
	}

private:
	Fngine* p_fngine_ = nullptr;
	std::unordered_map<std::string, std::unique_ptr<Texture>>textures_;
	uint32_t textureCount_ = 0;
	uint32_t textureMax_ = 100;

//////////////////
/// 
///  ロードをまとめて行うための関数
/// 
//////////////////
public:
	void BeginLoad();
	void EndLoad();
private:
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources_;
};