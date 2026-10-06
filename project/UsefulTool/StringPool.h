#pragma once
#include "ISingleton.h"
#include <string>
#include <unordered_map>
#include <vector>

class StringPool :
	public ISingleton<StringPool>
{
public:
	friend class ISingleton<StringPool>;
	~StringPool() = default;

public:
	// 文字列を取得して、IDを返す処理
	uint32_t GetStringID(const std::string& str);

	// IDから文字列を取得する処理
	std::string  GetStringFromID(uint32_t id);

private:
	std::unordered_map<std::string, uint32_t> stringToIDMap_;
	std::vector<std::string> idToStringMap_;
};