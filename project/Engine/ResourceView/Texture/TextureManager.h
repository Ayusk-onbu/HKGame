#pragma once
#include "Fngine.h"
#include "Texture.h"
#include <string>
#include <vector>
#include <map>

class TextureManager {
public:
	TextureManager() = default;
public:
	static TextureManager* GetInstance() {
		if (instance_ == nullptr) {
			instance_ = std::make_unique<TextureManager>();
		}
		return instance_.get();
	}
	void ReleaseInstance() { instance_.reset(); }
public:
	void Initialize(Fngine& fngine);
	std::string LoadTexture(const std::string& filename,const std::string& filePath);
	Texture& GetTexture(const std::string& name);
	std::unordered_map<std::string, std::unique_ptr<Texture>>& GetData() { return textures_; }
private:
	static std::unique_ptr<TextureManager>instance_;
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