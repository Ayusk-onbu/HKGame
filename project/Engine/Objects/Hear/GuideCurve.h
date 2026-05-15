#pragma once
#include "Structures.h"
#include <vector>

namespace GuideCurve {
	struct ControllerPoint {
		// ポイントの情報
		Vector3 position;   // 制御点座標
		float radius;       // 太さ
		Vector3 color;      // この点での髪の色 (グラデーションできそう)
		bool isLocked;      // 固定するかどうか

		// 計算用の情報
		Vector3 homePosition;// 最初の制御点
		Vector3 tangentIn;  // エルミートスプラインの計算用 - 入ってくる角度		
		Vector3 tangentOut; // エルミートスプラインの計算用 - 出ていく角度
		float nextToLength; // 次までの長さ
		
	};
	
	struct GuideHear {
		uint32_t rootTriangleId; // 頭皮のどのポリゴンから生えているか
		Vector2 barycentric;     // ポリゴン内の重心座標
		float restLength;        // 髪一本の長さ
		std::vector<ControllerPoint>points_;
	};

	struct Main {
		std::vector<GuideHear>guides_;
	};
}

namespace Strands {
	/*
	* どのくらいの半径にどのくらいランダムに散らすかのオフセット
	* 周囲のある一定距離にあるガイドをいくつ、さらにどのくらいの割合参照するか
	*/
	struct ChildStrand {
		uint32_t parentGuideIds[3]; // 影響を受ける親ガイド（近傍3本程度）
		float weights[3];           // 各親ガイドからの影響度（合計1.0）
		Vector2 offset;             // 親の重心位置からの微細なズレ
		float lengthScale;          // 親に対して少し短くしたり長くしたりする係数
	};


	/*
	* 作った頭皮マップの情報から根元を決めるその点から近い三点をガイドとして参照する
	* そのガイドまでの距離に応じてWeightづけを行う(合計1.0になるようにかなたぶん)
	*/
}

/*
* 頭皮マップに持たせる情報
* 1, 毛穴の位置
* 2, ボリューム
* 3, 
*/
