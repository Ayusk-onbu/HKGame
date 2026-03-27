#pragma once
#include "Collider.h"
#include <list>
#include <vector>

class CollisionManager
{
public:
	void Begin();
	void SetColliders(Collider* collider);
	void CheckAllCollisions();
private:
	bool CheckNarrowPhase(Collider* a, Collider* b, Vector3& outPush);
    // GJKアルゴリズムの本体
    bool GJK(ConvexCollider* a, ConvexCollider* b, Vector3& outPush);
	bool CheckFilter(Collider* colA, Collider* colB);
private:
	std::vector<Collider*> colliders_;
};

//void Player::CollisionMap(MapChipField& map)
//{
//	if (combineYourType_ == TypeCT(block)) {
//		return;
//	}
//
//	MapChipField::IndexSet rightTopIndex;
//	MapChipField::IndexSet leftTopIndex;
//	MapChipField::IndexSet rightDownIndex;
//	MapChipField::IndexSet leftDownIndex;
//	rightTopIndex = map.GetMapChipIndexSetByPosition({ colliderObj_.AABB.max.x - 0.001f, colliderObj_.AABB.max.y, 0.0f });
//	leftTopIndex = map.GetMapChipIndexSetByPosition({ colliderObj_.AABB.min.x, colliderObj_.AABB.max.y, 0.0f });
//	rightDownIndex = map.GetMapChipIndexSetByPosition({ colliderObj_.AABB.max.x - 0.001f, colliderObj_.AABB.min.y + 0.001f, 0.0f });
//	leftDownIndex = map.GetMapChipIndexSetByPosition({ colliderObj_.AABB.min.x, colliderObj_.AABB.min.y + 0.001f, 0.0f });
//
//	MapChipField::IndexSet rightTopPreIndex;
//	MapChipField::IndexSet leftTopPreIndex;
//	MapChipField::IndexSet rightDownPreIndex;
//	MapChipField::IndexSet leftDownPreIndex;
//	rightTopPreIndex = map.GetMapChipIndexSetByPosition({ preAABB_.max.x - 0.001f, preAABB_.max.y, 0.0f });
//	leftTopPreIndex = map.GetMapChipIndexSetByPosition({ preAABB_.min.x, preAABB_.max.y, 0.0f });
//	rightDownPreIndex = map.GetMapChipIndexSetByPosition({ preAABB_.max.x - 0.001f, preAABB_.min.y + 0.001f, 0.0f });
//	leftDownPreIndex = map.GetMapChipIndexSetByPosition({ preAABB_.min.x, preAABB_.min.y + 0.001f, 0.0f });
//
//	// 4隅のブロックを調べる
//	// 右上がブロックで左下が空白なら
//	if (IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//		IsNonBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//		// 右下も左上も空白なら
//		if (IsNonBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			if (isMove_ & 0b0001) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).y;
//				isJump_ = false; // ジャンプ終了
//			}
//			if (isMove_ & 0b0010) {
//				obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).x;
//			}
//			// x座標が変わっていなかったら
//			if (isMove_ == 0) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).y;
//				isJump_ = false; // ジャンプ終了
//			}
//		}
//		// 右下がブロックで左上が空白なら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).x;
//			onGround_ = false;
//		}
//		// 左上がブロックで右下が空白なら
//		else if (IsNonBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).y;
//			isJump_ = false; // ジャンプ終了
//			onGround_ = false;
//		}
//		// 右下も左上もブロックなら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).x;
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).y;
//			isJump_ = false; // ジャンプ終了
//			onGround_ = false;
//		}
//	}
//	// 左上がブロックで右下が空白なら
//	else if (IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex)) &&
//		IsNonBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex))) {
//		// 右上も左下も空白なら
//		if (IsNonBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			if (isMove_ & 0b0001) {
//				obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).x;
//			}
//			if (isMove_ & 0b0010) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).y;
//				isJump_ = false; // ジャンプ終了
//			}
//			// x座標が変わっていなかったら
//			if (isMove_ == 0) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).y;
//				isJump_ = false; // ジャンプ終了
//			}
//		}
//		// 右上がブロックで左下が空白なら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).y;
//			isJump_ = false; // ジャンプ終了
//			onGround_ = false;
//		}
//		// 左下がブロックで右上が空白なら
//		else if (IsNonBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).x;
//			onGround_ = false;
//		}
//		// 右上も左下もブロックなら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).x;
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).y;
//			isJump_ = false; // ジャンプ終了
//			onGround_ = false;
//		}
//	}
//	// 右下がブロックで左上が空白なら
//	else if (IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//		IsNonBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//		// 右上も左下も空白なら
//		if (IsNonBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			if (onGround_) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//			}
//			else if (!onGround_) {
//				if (isJump_) {
//					obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).x - blankMapChip_;
//				}
//				if (!isJump_) {
//					// すでに右にいたとき
//					if (rightTopPreIndex.xindex == rightTopIndex.xindex) {
//						obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//						onGround_ = true;
//					}
//					else {
//						obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).x - blankMapChip_;
//					}
//				}
//			}
//		}
//		// 右上がブロックで左下が空白なら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).x;
//		}
//		// 左下がブロックで右上が空白なら
//		else if (IsNonBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//			onGround_ = true;
//		}
//		// 右上も左下もブロックなら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).x;
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//			onGround_ = true;
//		}
//	}
//	// 左下がブロックで右上が空白なら
//	else if (IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex)) &&
//		IsNonBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex))) {
//		// 右下も左上も空白なら
//		if (IsNonBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			if (onGround_) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//			}
//			else if (!onGround_) {
//				if (isJump_) {
//					obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).x + blankMapChip_;
//				}
//				if (!isJump_) {
//					// すでに左にいたとき
//					if (leftTopPreIndex.xindex == leftTopIndex.xindex) {
//						obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//						onGround_ = true;
//					}
//					else {
//						obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).x + blankMapChip_;
//					}
//				}
//			}
//			// x座標が変わっていなかったら
//			if (isMove_ == 0) {
//				obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//				onGround_ = true;
//			}
//		}
//		// 右下がブロックで左上が空白なら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsNonBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//			onGround_ = true;
//		}
//		// 左上がブロックで右下が空白なら
//		else if (IsNonBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).x;
//		}
//		// 右下も左上もブロックなら
//		else if (IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//			IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex))) {
//			obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).x;
//			obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//			onGround_ = true;
//		}
//	}
//
//	else if (IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//		IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex)) &&
//		isMove_ & 0b0010 && isJump_) {
//		// 右に移動しながらジャンプしているとき、右下と左上がブロックなら
//		obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).x;
//		obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftDownIndex.xindex, leftDownIndex.yindex).y;
//		isJump_ = false; // ジャンプ終了
//		onGround_ = false;
//	}
//	else if (IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex)) &&
//		IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//		isMove_ & 0b0001 && isJump_) {
//		// 左に移動しながらジャンプしているとき、左下と右上がブロックなら
//		obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).x;
//		obj_.transforms.translate.y = map.GetMapChipPositionByIndex(rightDownIndex.xindex, rightDownIndex.yindex).y;
//		isJump_ = false; // ジャンプ終了
//		onGround_ = false;
//	}
//	else if (IsBlockType(map.GetMapChipTypeByIndex(rightTopIndex.xindex, rightTopIndex.yindex)) &&
//		IsBlockType(map.GetMapChipTypeByIndex(leftDownIndex.xindex, leftDownIndex.yindex)) &&
//		isMove_ & 0b0010 && !isJump_) {
//		// 右に移動しているとき、右上と左下がブロックなら
//		obj_.transforms.translate.x = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).x;
//		obj_.transforms.translate.y = map.GetMapChipPositionByIndex(leftTopIndex.xindex, leftTopIndex.yindex).y;
//		onGround_ = true;
//	}
//	else if (IsBlockType(map.GetMapChipTypeByIndex(leftTopIndex.xindex, leftTopIndex.yindex)) &&
//		IsBlockType(map.GetMapChipTypeByIndex(rightDownIndex.xindex, rightDownIndex.yindex)) &&
//		isMove_ & 0b0001 && !isJump_) {
//		// 左に移動しているとき、左上と右下がブロックなら
//		obj_.transforms.translate.x = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).x;
//		obj_.transforms.translate.y = map.GetMapChipPositionByIndex(rightTopIndex.xindex, rightTopIndex.yindex).y;
//		onGround_ = true;
//	}
//	else {
//		onGround_ = false;
//	}
//}
