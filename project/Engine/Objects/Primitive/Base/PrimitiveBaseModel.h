#pragma once
#include "Fngine.h"
#include "PrimitiveBaseModelData.h"

class PrimitiveBaseModel
{
public:
	virtual ~PrimitiveBaseModel() = default;
	virtual void Initialize() = 0;
	virtual void Draw() = 0;


private:
	Fngine* p_engine_ = nullptr;
	PrimitiveBaseModelData data_;

};

