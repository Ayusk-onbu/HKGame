#pragma once
#include "Primitive/Box/PrimitiveBox.h"
#include "Primitive/Ring/Ring.h"
#include "Primitive/Sphere/PrimitiveSphere.h"
#include "Primitive/Cylinder/Cylinder.h"
#include "Primitive/MagicPlane.h"
#include "Primitive/Line/Line.h"
#include "Primitive/Grass/Grass.h"
#include "Primitive/Column/Column.h"
#include "Primitive/Bone/Bone.h"
#include "Primitive/Character/PrimitiveCharcter.h"
#include "Primitive/Mesh/NormalMesh.h"

enum class RenderPass {
	DepthPrePass,// 深度のみ書き込み
	MainColor    // 通常通りの描画
};

class DrawManager :
	public ISingleton<DrawManager>
{
public:
	friend class ISingleton<DrawManager>;
	// 基本的な関数
public:
	void Initialize(Fngine* engine);
	void Draw(RenderPass pass);

private:
	void RegisterModel(IDrawable* model) {
		models_.push_back(model);
	}

	template <typename T>
	void CreateAndRegister(std::unique_ptr<T>& target, Fngine* engine, uint32_t numInstance) {
		target = std::make_unique<T>();
		target->Initialize(engine, numInstance);
		RegisterModel(target.get());
	}

private:
	Fngine* engine_;
	std::vector<IDrawable*>models_;
	
	//------------------------
	//  Modelたち
	//------------------------
private:
	//   =====
	// 【 Box 】
	//   =====
	std::unique_ptr<PrimitiveBox>box_;


	//   ========
	// 【 Ring 】
	//   ========
	std::unique_ptr<PrimitiveRing>ring_;


	//   ========
	// 【 Sphere 】
	//   ========
	std::unique_ptr<PrimitiveSphere>sphere_;
	

	//   ========
	// 【 Cylinder 】
	//   ========
	std::unique_ptr<PrimitiveCylinder>cylinder_;


	//   ======
	// 【 Line 】
	//   ======
	std::unique_ptr<PrimitiveLine>line_;


	//   ======
	// 【 MagicCircle 】
	//   ======
	std::unique_ptr<MagicCircle>magicCircle_;


	//   =======
	// 【 Grass 】
	//   =======
	std::unique_ptr<Grass>grass_;


	//   =======
	// 【 Column 】
	//   =======
	std::unique_ptr<Column>column_;


	//   =======
	// 【 Bone 】
	//   =======
	std::unique_ptr<BoneDrawer>bone_;


	//   ==========
	// 【 Charcter 】
	//   ==========
	std::unique_ptr<PrimitiveCharacter>character_;


	//   ==========
	// 【 NormalMesh 】
	//   ==========
	std::unique_ptr<NormalMeshModel>normalMesh_;


	//=============
	//  Getの関数 
	//=============
public:
	PrimitiveBox* GetBox() { return box_.get(); }

	PrimitiveRing* GetRing() { return ring_.get(); }

	PrimitiveSphere* GetSphere() { return sphere_.get(); }

	PrimitiveCylinder* GetCylinder() { return cylinder_.get(); }

	PrimitiveLine* GetLine() { return line_.get(); }

	MagicCircle* GetMagicCircle() { return magicCircle_.get(); }

	Grass* GetGrass() { return grass_.get(); }

	Column* GetColumn() { return column_.get(); }

	BoneDrawer* GetBone() { return bone_.get(); }

	PrimitiveCharacter* GetCharacter() { return character_.get(); }

	NormalMeshModel* GetNormalMesh() { return normalMesh_.get(); }
};

