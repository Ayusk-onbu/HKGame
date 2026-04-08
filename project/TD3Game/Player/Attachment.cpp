module;
#include "Structures.h"
module Attachment;
//////////////////////
///
///   コンストラクタ・デストラクタ
/// 
//////////////////////
Attachment::Attachment() {

}

Attachment::~Attachment() {

}
//////////////////////
///
///   更新処理
/// 
//////////////////////
void Attachment::Update() {

	Matrix4x4 scaleMat = Matrix4x4::Make::Scale({1.0f,1.0f,1.0f});
	Matrix4x4 rotationMat = Matrix4x4::Make::RotateXYZ(rotation_);
	Matrix4x4 translateMat = Matrix4x4::Make::Translate(position_);

	// S * R * T
	matWorld_ = Matrix4x4::Multiply(scaleMat, rotationMat);
	matWorld_ = Matrix4x4::Multiply(matWorld_, translateMat);
	// 親子関係なら
	if (parent_) {
		matWorld_ = Matrix4x4::Multiply(matWorld_, parent_->GetMatrix());
	}
}
//////////////////////
///
///   接続処理
/// 
//////////////////////
void Attachment::AttachTo(Attachment* parentJoint) {
	// アタッチを許可されているか確認する -> 不許可なら終了
	if ((acceptType_ & parentJoint->GetType()) == 0)return;
	// 接続処理
	parent_ = parentJoint;
	attached_ = true;
}
//////////////////////
///
///   解除処理
/// 
//////////////////////
void Attachment::Detach() {
	// 解除処理
	parent_ = nullptr;
	attached_ = false;
	// ローカル座標なのでワールドに変換する
	position_ = GetWorldPos();
}