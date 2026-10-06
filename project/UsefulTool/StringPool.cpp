#include "StringPool.h"
#include "Log.h"

uint32_t StringPool::GetStringID(const std::string& str) {
	auto it = stringToIDMap_.find(str);
	//Log::View("Search string: " + str);
	if (it != stringToIDMap_.end()) {
		//Log::View("String already exists, returning ID: " + std::to_string(it->second));	
		return it->second;
	}

	uint32_t id = static_cast<uint32_t>(idToStringMap_.size());
	stringToIDMap_[str] = id;
	idToStringMap_.push_back(str);
	Log::View("New string added, ID: " + std::to_string(id));
	return id;
}

std::string StringPool::GetStringFromID(uint32_t id) {
	Log::View("Getting string by ID: " + std::to_string(id));
	if (id >= idToStringMap_.size()) {
		Log::View("Invalid string ID");
		if (idToStringMap_.size() > 0) {
			Log::View("so already exists string(ID = 0) return");
			return idToStringMap_[0];
		}
		Log::View("not already exists string so return empty string");
		return "";
	}

	Log::View("Correct Get String by ID");
	return idToStringMap_[id];
}