#include "HairEditor.h"
#include "FileSystem.h"
#include "MathUtils.h"
#include "DrawManager.h"

void HairGuideEditor::GenerateDefaultSphereHair(GuideCurve::ControllerPoint* data, uint32_t totalCount,float headRadius,
    float segmentLength,Vector3 headCenter, Vector3 rootColor,
    Vector3 tipColor) {
    const int POINTS_PER_GUIDE = hairSystem_->GetCPUGuideConfig()->pointPerGuide;
    int totalGuides = totalCount / POINTS_PER_GUIDE;

    for (int g = 0; g < totalGuides; ++g) {

        // 1. フィボナッチ球面により、球の上半分～側面に1000本の根元を完全に均等配置する
        // yの範囲を 1.0（真上） から -0.3（うなじ・耳の下あたり）まで綺麗に分散させる
        float t_geo = (float)g / (float)(totalGuides - 1);
        float y_local = 1.0f - t_geo * 1.3f;

        // 高さに応じた円の半径を計算
        float radiusAtY = std::sqrt((std::max)(0.0f, 1.0f - y_local * y_local));

        // 黄金角（約137.5度 = 2.39996322ラジアン）ずつ回転させて密を作る
        float goldenAngle = 2.39996322f;
        float theta = g * goldenAngle;

        float x_local = std::cos(theta) * radiusAtY;
        float z_local = std::sin(theta) * radiusAtY;

        // 根元の法線方向（頭の中心から外側へ広がるベクトル）
        Vector3 normal = { x_local, y_local, z_local };
        float n_len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (n_len > 0.0f) { normal.x /= n_len; normal.y /= n_len; normal.z /= n_len; }

        // 根元（Point 0）の座標を決定
        Vector3 rootPos = {
            headCenter.x + normal.x * headRadius,
            headCenter.y + normal.y * headRadius,
            headCenter.z + normal.z * headRadius
        };

        Vector3 currentPos = rootPos;

        // 2. 1本のガイドに紐づく16個の制御点の形状を計算
        for (int p = 0; p < POINTS_PER_GUIDE; ++p) {
            int targetIndex = (g * POINTS_PER_GUIDE) + p;

            // 根元から毛先への進捗度（0.0 ～ 1.0）
            float progress = (float)p / (float)(POINTS_PER_GUIDE - 1);

            if (p == 0) {
                data[targetIndex].position = rootPos;
            }
            else {
                // 【超重要】毛先に向かうにつれて、法線方向（外向き）から徐々に「真下（重力）」へ流れるベクトルにブレンドする
                Vector3 dir;
                float curveInfluence = progress * progress; // 後半（毛先）ほど急に垂れ下がるカーブ

                dir.x = normal.x * (1.0f - curveInfluence * 0.6f);
                dir.y = normal.y * (1.0f - progress) - curveInfluence * 1.3f; // 下方向への引力を強く
                dir.z = normal.z * (1.0f - curveInfluence * 0.6f);

                // ベクトルの正規化
                float d_len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (d_len > 0.0f) { dir.x /= d_len; dir.y /= d_len; dir.z /= d_len; }

                // 前のポイントから計算した方向へ一定距離進める（骨組みの維持）
                currentPos.x += dir.x * segmentLength;
                currentPos.y += dir.y * segmentLength;
                currentPos.z += dir.z * segmentLength;

                data[targetIndex].position = currentPos;
            }

            // 3. 面倒な周辺パラメータも最高の設定で自動セット！
            data[targetIndex].homePosition = data[targetIndex].position;

            // 太さ：根元は太く（0.004f）、毛先にかけて徐々に細くテーパリング
            data[targetIndex].radius = 0.0035f * (1.0f - progress * 0.8f) + 0.0005f;
            data[targetIndex].nextToLength = segmentLength;

            // 物理演算ウェイト：根元はアニメーションに固定(0.0)、毛先ほど物理で揺れる(1.0)
            data[targetIndex].physicsWeight = progress * progress;

            //// デバッグカラー：写真のように綺麗に見えるよう、ガイドの位置や進捗でグラデーションをかける
            //data[targetIndex].color.x = 0.2f + progress * 0.6f;                       // R
            //data[targetIndex].color.y = 0.15f + (1.0f - progress) * 0.3f;              // G
            //data[targetIndex].color.z = 0.4f + std::sin((float)g * 0.05f) * 0.2f + progress * 0.4f; // B

            data[targetIndex].color.x = rootColor.x * (1.0f - progress) + tipColor.x * progress;
            data[targetIndex].color.y = rootColor.y * (1.0f - progress) + tipColor.y * progress;
            data[targetIndex].color.z = rootColor.z * (1.0f - progress) + tipColor.z * progress;
        }
    }
}

void HairGuideEditor::GenerateDefaultShortHair(GuideCurve::ControllerPoint* data, uint32_t totalCount, float headRadius,
    float bangLength, float backLength, Vector3 headCenter, Vector3 rootColor,
    Vector3 tipColor) {
    // 1. IHair から各種メタデータバッファのポインタを取得
    auto* guideInfoData = hairSystem_->GetCPUGuideInfoData();
    auto* strandInfoData = hairSystem_->GetCPUStrandInfoData();
    auto* segmentData = hairSystem_->GetCPUSegmentData();

    // ガイドの総数を取得
    int totalGuides = hairSystem_->GetCPUGuideInfoCount();
    if (totalGuides == 0) return;

    // 🌟 累積オフセット（インデックス）の初期化
    uint32_t currentVertexOffset = 0;
    uint32_t currentStrandOffset = 0;
    uint32_t currentSegmentOffset = 0;

    // 分割数の基準（例として最大16点 = 15セグメントとする場合）
    float bangSegLen = bangLength / 15.0f;
    float backSegLen = backLength / 15.0f;

    bool isZPlusFront = true;
    float forwardSign = isZPlusFront ? 1.0f : -1.0f;

    for (int g = 0; g < totalGuides; ++g) {
        // --- ジオメトリ・配置計算（既存のロジック） ---
        float t_geo = (float)g / (float)(totalGuides - 1);
        float y_local = 1.0f - t_geo * 1.3f;
        float radiusAtY = std::sqrt((std::max)(0.0f, 1.0f - y_local * y_local));
        float goldenAngle = 2.39996322f;
        float theta = g * goldenAngle;
        float x_local = std::cos(theta) * radiusAtY;
        float z_local = std::sin(theta) * radiusAtY;

        Vector3 normal = { x_local, y_local, z_local };
        float n_len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (n_len > 0.0f) { normal.x /= n_len; normal.y /= n_len; normal.z /= n_len; }

        float heightFactor = 0.3f + (std::max)(0.0f, normal.y) * 0.7f;
        float currentBackSegLen = backSegLen * heightFactor;
        float currentBangSegLen = bangSegLen * (0.5f + (std::max)(0.0f, normal.y) * 0.5f);

        float localSegmentLength = currentBackSegLen;
        bool isFrontArea = isZPlusFront ? (normal.z > 0.0f) : (normal.z < 0.0f);
        bool isBang = false;

        if (isFrontArea && normal.y > 0.0f) {
            float blend = std::abs(normal.z);
            if (blend > 0.5f) { localSegmentLength = currentBangSegLen; isBang = true; }
            else { float t = blend / 0.5f; localSegmentLength = currentBackSegLen * (1.0f - t) + currentBangSegLen * t; }
        }

        // 🌟 【修正ポイント1】ガイドごとに最適な頂点数を動的に決定する
        int pointsForThisGuide = 16; // デフォルトの頂点数
        if (isBang) {
            pointsForThisGuide = 10; // 前髪は短いので頂点数を減らす、といった調整が可能
        }
        else if (normal.y < -0.1f) {
            pointsForThisGuide = 8;  // 襟足の短い部分はさらに頂点数を減らす、など
        }

        // 🌟 【修正ポイント2】GuideInfo の組み直し
        if (guideInfoData) {
            guideInfoData[g].vertexStartIndex = currentVertexOffset;
            guideInfoData[g].vertexCount = pointsForThisGuide;
            // 必要に応じて他の物理パラメータや初期化フラグ等があればここでセット
        }

        // 🌟 【修正ポイント3】StrandInfo の組み直し
        // ガイド1本に対して生成される子ストランド（Strand）の情報をセット
        // 例: ガイド1本につき4本のストランドが生成される場合
        int strandsPerGuide = 4;
        for (int s = 0; s < strandsPerGuide; ++s) {
            uint32_t strandIdx = currentStrandOffset + s;
            if (strandIdx < hairSystem_->GetCPUStrandInfoCount() && strandInfoData) {
                // ストランド固有の頂点バッファがある場合はそのオフセット、
                // ガイドと同期する場合は共通のオフセットやカウントをセット
                strandInfoData[strandIdx].vertexStartIndex = currentVertexOffset;
                strandInfoData[strandIdx].vertexCount = pointsForThisGuide;
                strandInfoData[strandIdx].aabbStartIndex = g;
            }
        }

        // 🌟 【修正ポイント4】SegmentData の組み直し (DXR/AABB用)
        // セグメント数は (頂点数 - 1)
        int segmentsForThisGuide = pointsForThisGuide - 1;
        for (int seg = 0; seg < segmentsForThisGuide; ++seg) {
            uint32_t segIdx = currentSegmentOffset + seg;
            if (segmentData) {
                // segmentData[segIdx] に対する初期化や、対応する頂点インデックスの紐付けを行う
                // 例: segmentData[segIdx].globalVertexIndex = currentVertexOffset + seg;
            }
        }

        // --- 制御点の座標・パラメータ計算（POINTS_PER_GUIDE を pointsForThisGuide に置換） ---
        Vector3 rootPos = {
            headCenter.x + normal.x * headRadius,
            headCenter.y + normal.y * headRadius,
            headCenter.z + normal.z * headRadius
        };
        Vector3 currentPos = rootPos;

        for (int p = 0; p < pointsForThisGuide; ++p) {
            // 🌟 ターゲットインデックスは累積オフセットを基準にする
            int targetIndex = currentVertexOffset + p;
            float progress = (float)p / (float)(pointsForThisGuide - 1);

            if (p == 0) {
                data[targetIndex].position = rootPos;
            }
            else {
                Vector3 dir;
                float curveInfluence = progress * progress;

                if (isBang) {
                    float volumeBoost = std::sin(progress * 3.141592f);
                    dir.x = normal.x * (1.0f - progress * 0.2f);
                    dir.y = -0.2f - curveInfluence * 1.5f;
                    dir.z = normal.z * (1.0f - progress * 0.2f) + (forwardSign * volumeBoost * 0.15f);
                }
                else {
                    dir.x = normal.x * (1.0f - progress * 0.2f);
                    dir.y = -0.2f - curveInfluence * 2.0f;
                    dir.z = normal.z * (1.0f - progress * 0.2f);
                }

                float d_len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (d_len > 0.0f) { dir.x /= d_len; dir.y /= d_len; dir.z /= d_len; }

                currentPos.x += dir.x * localSegmentLength;
                currentPos.y += dir.y * localSegmentLength;
                currentPos.z += dir.z * localSegmentLength;

                data[targetIndex].position = currentPos;
            }

            data[targetIndex].homePosition = data[targetIndex].position;
            data[targetIndex].radius = 0.0035f * (1.0f - progress * 0.8f) + 0.0005f;
            data[targetIndex].nextToLength = localSegmentLength;
            data[targetIndex].physicsWeight = progress * progress;

            data[targetIndex].color.x = rootColor.x * (1.0f - progress) + tipColor.x * progress;
            data[targetIndex].color.y = rootColor.y * (1.0f - progress) + tipColor.y * progress;
            data[targetIndex].color.z = rootColor.z * (1.0f - progress) + tipColor.z * progress;
        }

        // 🌟 【最重要】次のガイドのために累積オフセットを進める
        currentVertexOffset += pointsForThisGuide;
        currentStrandOffset += strandsPerGuide;
        currentSegmentOffset += segmentsForThisGuide;
    }
}

void HairGuideEditor::GenerateDefaultShortHair2(GuideCurve::ControllerPoint* data, uint32_t totalCount, float headRadius,
    float bangLength, float sideLength, float backLength, Vector3 headCenter, Vector3 rootColor,
    Vector3 tipColor) {
    const int POINTS_PER_GUIDE = hairSystem_->GetCPUGuideConfig()->pointPerGuide;
    int totalGuides = totalCount / POINTS_PER_GUIDE;

    float bangSegLen = bangLength / (POINTS_PER_GUIDE - 1);
    float sideSegLen = sideLength / (POINTS_PER_GUIDE - 1);
    float backSegLen = backLength / (POINTS_PER_GUIDE - 1);

    bool isZPlusFront = true;
    float forwardSign = isZPlusFront ? 1.0f : -1.0f;

    for (int g = 0; g < totalGuides; ++g) {

        float t_geo = (float)g / (float)(totalGuides - 1);
        float y_local = 1.0f - t_geo * 1.3f;

        float radiusAtY = std::sqrt((std::max)(0.0f, 1.0f - y_local * y_local));
        float goldenAngle = 2.39996322f;
        float theta = g * goldenAngle;

        float x_local = std::cos(theta) * radiusAtY;
        float z_local = std::sin(theta) * radiusAtY;

        Vector3 normal = { x_local, y_local, z_local };
        float n_len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (n_len > 0.0f) { normal.x /= n_len; normal.y /= n_len; normal.z /= n_len; }

        // -----------------------------------------------------------
        // 毛先を水平に揃えるための高さ補正(頭頂部ほど長め)
        // -----------------------------------------------------------
        float heightFactor = 0.3f + (std::max)(0.0f, normal.y) * 0.7f;
        float currentBackSegLen = backSegLen * heightFactor;
        float currentSideSegLen = sideSegLen * heightFactor;
        float currentBangSegLen = bangSegLen * (0.5f + (std::max)(0.0f, normal.y) * 0.5f);

        bool isFrontArea = isZPlusFront ? (normal.z > 0.0f) : (normal.z < 0.0f);
        bool isBang = false;

        // デフォルトは襟足(短い)
        float localSegmentLength = currentBackSegLen;

        if (isFrontArea) {
            if (normal.y > 0.0f) {
                // --- おでこ〜頭上部: 前髪 or サイドへのブレンド ---
                float blend = std::abs(normal.z); // 0=横(サイド) ~ 1=正面(前髪センター)

                if (blend > 0.5f) {
                    // 真正面に近いエリア → ぱっつん前髪
                    localSegmentLength = currentBangSegLen;
                    isBang = true;
                }
                else {
                    // 前髪の脇 → サイドの長い毛束へ滑らかに移行
                    float t = blend / 0.5f;
                    localSegmentLength = currentSideSegLen * (1.0f - t) + currentBangSegLen * t;
                }
            }
            else {
                // --- もみあげ〜頬の高さ: 顔まわりの長いサイド毛束 ---
                // z が 0 を超えた瞬間から素早く sideLength に立ち上げる
                // (= 襟足のすぐ前から顔まわりは長くなる、グラデーションボブの境界線)
                float t = (std::min)(1.0f, std::abs(normal.z) / 0.4f);
                localSegmentLength = currentBackSegLen * (1.0f - t) + currentSideSegLen * t;
            }
        }

        Vector3 rootPos = {
            headCenter.x + normal.x * headRadius,
            headCenter.y + normal.y * headRadius,
            headCenter.z + normal.z * headRadius
        };

        Vector3 currentPos = rootPos;

        for (int p = 0; p < POINTS_PER_GUIDE; ++p) {
            int targetIndex = (g * POINTS_PER_GUIDE) + p;
            float progress = (float)p / (float)(POINTS_PER_GUIDE - 1);

            if (p == 0) {
                data[targetIndex].position = rootPos;
            }
            else {
                Vector3 dir;
                float curveInfluence = progress * progress;

                // 毛先側でわずかに外ハネさせる(写真のボブの「外に流れる毛先」用)
                float flickOut = 0.0f;
                if (progress > 0.65f) {
                    float ft = (progress - 0.65f) / 0.35f;
                    flickOut = ft * ft * 0.35f;
                }

                if (isBang) {
                    float volumeBoost = std::sin(progress * 3.141592f);

                    // 横幅(X)を絞りすぎない。額の幅を保ったまま下に落とす。
                    dir.x = normal.x * (1.0f - progress * 0.2f);
                    dir.y = -0.2f - curveInfluence * 1.5f;
                    dir.z = normal.z * (1.0f - progress * 0.2f) + (forwardSign * volumeBoost * 0.15f);
                }
                else {
                    // 後ろ・サイドも横幅をキープしつつ、毛先だけわずかに外ハネ
                    dir.x = normal.x * (1.0f - progress * 0.2f) + normal.x * flickOut;
                    dir.y = -0.2f - curveInfluence * 2.0f;
                    dir.z = normal.z * (1.0f - progress * 0.2f);
                }

                float d_len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                if (d_len > 0.0f) { dir.x /= d_len; dir.y /= d_len; dir.z /= d_len; }

                currentPos.x += dir.x * localSegmentLength;
                currentPos.y += dir.y * localSegmentLength;
                currentPos.z += dir.z * localSegmentLength;

                data[targetIndex].position = currentPos;
            }

            data[targetIndex].homePosition = data[targetIndex].position;
            data[targetIndex].radius = 0.0035f * (1.0f - progress * 0.8f) + 0.0005f;
            data[targetIndex].nextToLength = localSegmentLength;
            data[targetIndex].physicsWeight = progress * progress;

            data[targetIndex].color.x = rootColor.x * (1.0f - progress) + tipColor.x * progress;
            data[targetIndex].color.y = rootColor.y * (1.0f - progress) + tipColor.y * progress;
            data[targetIndex].color.z = rootColor.z * (1.0f - progress) + tipColor.z * progress;
        }
    }
}

void HairGuideEditor::Update(){
#ifdef USE_IMGUI
    EventManager::GetInstance()->FireEvent(GAMEEVENTID::HairEditor);
    auto* cpuData = hairSystem_->GetCPUGuideData();
    const int POINTS_PER_GUIDE = hairSystem_->GetCPUGuideConfig()->pointPerGuide;

    if (isDrawPoint_) {
        float defaultScale = 0.005f;
        float highlightScale = 0.008f; // 選択中の球を少し大きくして目立たせる

        auto* guideInfoData = hairSystem_->GetCPUGuideInfoData();
        uint32_t guideInfoCount = hairSystem_->GetCPUGuideInfoCount();

        // 1. 選択中ガイド（4本）の頂点インデックス範囲と表示色を事前に設定
        struct GuideRange {
            uint32_t start = 0;
            uint32_t end = 0;
            Vector4 color;
            bool isValid = false;
        };

        GuideRange targetRanges[4];
        auto setupRange = [&](int slot, int guideIndex, Vector4 color) {
            if (guideIndex >= 0 && guideIndex < static_cast<int>(guideInfoCount)) {
                const auto& info = guideInfoData[guideIndex];
                if (info.vertexCount > 0) {
                    targetRanges[slot].start = info.vertexStartIndex;
                    targetRanges[slot].end = info.vertexStartIndex + info.vertexCount;
                    targetRanges[slot].color = color;
                    targetRanges[slot].isValid = true;
                }
            }
            };

        // 優先度順にセット（0:単一編集=黄色, 1:親1=赤, 2:親2=緑, 3:親3=青）
        setupRange(0, selectedGuideIndex_, { 1.0f, 0.85f, 0.0f,  1.0f }); // 黄色
        setupRange(1, selectedGuideIndices_[0], { 1.0f, 0.25f, 0.25f, 1.0f }); // 赤色
        setupRange(2, selectedGuideIndices_[1], { 0.25f, 1.0f, 0.25f, 1.0f }); // 緑色
        setupRange(3, selectedGuideIndices_[2], { 0.25f, 0.65f, 1.0f, 1.0f }); // 青色

        // 2. 既存のループ内で判定＆描画
        for (uint32_t i = 0; i < hairSystem_->GetCPUActiveGuideCount(); ++i) {
            // デフォルト値（非選択時は白）
            Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            float pointScale = defaultScale;

            // 現在の頂点インデックス(i)が選択中ガイドの範囲に含まれるか判定
            for (int r = 0; r < 4; ++r) {
                if (targetRanges[r].isValid && i >= targetRanges[r].start && i < targetRanges[r].end) {
                    pointColor = targetRanges[r].color;
                    pointScale = highlightScale;
                    break; // 該当したら色を確定して抜ける
                }
            }

            PrimitiveSphereData data;
            data.worldTransform.Initialize();

            Vector3 localPos = cpuData[i].position;
            Vector4 localPos4 = { localPos.x, localPos.y, localPos.z, 1.0f };
            Vector4 worldPos4 = Matrix4x4::Transform(hairSystem_->characterMatrix_, localPos4);
            Vector3 worldPos = { worldPos4.x / worldPos4.w, worldPos4.y / worldPos4.w, worldPos4.z / worldPos4.w };

            data.worldTransform.set_.Translation(worldPos);
            data.worldTransform.set_.Scale({ pointScale, pointScale, pointScale });
            data.worldTransform.LocalToWorld();
            data.color = pointColor;
            DrawManager::GetInstance()->GetSphere()->AddInstance(data);
        }
    }

    if (isEditingSpline_ && hermiteEditor_) {
        hermiteEditor_->Update();
       // return;
    }

    if (ImGui::IsMouseClicked(0) && !ImGui::GetIO().WantCaptureMouse) {

        ImVec2 mousePos = ImGui::GetMousePos();

        // 画面サイズ・カメラの逆行列・カメラ位置を取得
        float windowWidth = 1280.0f;  // 実際の画面幅に合わせてください
        float windowHeight = 720.0f;  // 実際の画面高に合わせてください

        Matrix4x4 invProjView = Matrix4x4::Inverse(CameraSystem::GetInstance()->GetActiveCamera()->GetViewProjectionMatrix());
        Vector3 camPos = CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation();

        // マウス座標から3D空間のレイを生成
        Ray ray = MathUtils::CalculateRayFromScreen(
            mousePos.x, mousePos.y,
            windowWidth, windowHeight,
            invProjView, camPos
        );

        int closestPoint = -1;
        float minDistance = FLT_MAX;
        float hitDist = 0.0f;
        float clickRadius = 0.005f; // 点の当たり判定の大きさ（細かければ調整）

        // CPU側のガイドデータを取得
        uint32_t count = hairSystem_->GetCPUGuideCount();

        // 全ての制御点に対してレイ判定を行う
        for (uint32_t i = 0; i < count; ++i) {
            Vector3 localPos = cpuData[i].position;
            Vector4 localPos4 = { localPos.x, localPos.y, localPos.z, 1.0f };
            Vector4 worldPos4 = Matrix4x4::Transform(hairSystem_->characterMatrix_, localPos4);
            Vector3 worldPos = { worldPos4.x / worldPos4.w, worldPos4.y / worldPos4.w, worldPos4.z / worldPos4.w };

            if (MathUtils::IntersectRaySphere(ray, worldPos, clickRadius, &hitDist)) {
                if (hitDist < minDistance) {
                    minDistance = hitDist;
                    closestPoint = i;
                }
            }
        }

        ImGuiIO& io = ImGui::GetIO();

        if (closestPoint != -1) {
            selectedPointIndices_.clear(); // まず選択状態をリセット

            if (io.KeyCtrl) {
                // Ctrl: 全部選択
                for (uint32_t i = 0; i < count; ++i) {
                    selectedPointIndices_.push_back(i);
                }
            }
            else if (io.KeyShift) {
                // Alt: クリックしたポイントが属するガイド全体を選択
                auto* guideInfoData = hairSystem_->GetCPUGuideInfoData();
                uint32_t guideInfoCount = hairSystem_->GetCPUGuideInfoCount();

                for (uint32_t guideIndex = 0; guideIndex < guideInfoCount; ++guideIndex) {
                    const auto& info = guideInfoData[guideIndex];

                    // 頂点が割り当てられていない（無効な）ガイドはスキップ
                    if (info.vertexCount == 0) continue;

                    uint32_t startIndex = info.vertexStartIndex;
                    uint32_t endIndex = startIndex + info.vertexCount;

                    // closestPoint がこのガイドの範囲内に含まれているかチェック
                    if (closestPoint >= (int)startIndex && closestPoint < (int)endIndex) {
                        // 属しているガイドの全頂点を選択リストに追加
                        for (uint32_t i = startIndex; i < endIndex; ++i) {
                            selectedPointIndices_.push_back(i);
                        }
                        break; // 該当するガイドが見つかったらループを抜ける
                    }
                }
            }
            else {
                // 通常クリック: 単一選択
                selectedPointIndices_.push_back(closestPoint);
            }
        }
        else {
            // 何も無い場所をクリックしたら選択解除
            selectedPointIndices_.clear();
        }

        //if (!io.WantCaptureKeyboard) {
        //    // [Delete] キー: 選択中頂点（非選択時は末尾頂点）を削除
        //    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && selectedGuideIndex_ >= 0) {
        //        hairSystem_->RemoveVertexFromGuide(
        //            static_cast<uint32_t>(selectedGuideIndex_),
        //            selectedVertexIndexWithinGuide_
        //        );
        //        // 頂点が削除されたので選択解除/調整
        //        selectedVertexIndexWithinGuide_ = -1;
        //    }

        //    // [Shift + A] キー: 選択中のガイドの末尾に頂点を追加
        //    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A) && selectedGuideIndex_ >= 0) {
        //        hairSystem_->AddVertexToGuide(static_cast<uint32_t>(selectedGuideIndex_));
        //    }

        //    // [Shift + M] キー: 選択中のガイドをミラー複製
        //    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_M) && selectedGuideIndex_ >= 0) {
        //        CreateMirrorGuide(static_cast<uint32_t>(selectedGuideIndex_), mirrorAxisX_);
        //    }
        //}
    }

    // 髪を作成するガイドの可視化
	PrimitiveSphereData directionData;
	directionData.worldTransform.Initialize();
    Vector3 localPos = newGuidePos;
    Vector4 localPos4 = { localPos.x, localPos.y, localPos.z, 1.0f };
    Vector4 worldPos4 = Matrix4x4::Transform(hairSystem_->characterMatrix_, localPos4);
    Vector3 worldPos = { worldPos4.x / worldPos4.w, worldPos4.y / worldPos4.w, worldPos4.z / worldPos4.w };

    directionData.worldTransform.set_.Translation(worldPos);
    directionData.worldTransform.set_.Scale({ 0.005f,0.005f,0.005f });
    directionData.worldTransform.LocalToWorld();
    directionData.color = { 1.0f,0.0f,1.0f,1.0f };
    DrawManager::GetInstance()->GetSphere()->AddInstance(directionData);

	PrimitiveLineData lineData;
	lineData.startPoint = worldPos;
    localPos = newGuidePos + newGuideDir * newGuideLength;
    localPos4 = { localPos.x, localPos.y, localPos.z, 1.0f };
    worldPos4 = Matrix4x4::Transform(hairSystem_->characterMatrix_, localPos4);
    worldPos = { worldPos4.x / worldPos4.w, worldPos4.y / worldPos4.w, worldPos4.z / worldPos4.w };
    lineData.endPoint = worldPos;
	lineData.color = { 1.0f,0.0f,1.0f,1.0f };
    DrawManager::GetInstance()->GetLine()->AddInstance(lineData);

#endif
}

////////////////////////////////
//
// 【 UIに関する関数 】
//
////////////////////////////////
void HairGuideEditor::DrawUI(){
#ifdef USE_IMGUI
    uint32_t count = hairSystem_->GetCPUGuideCount();
    auto* cpuData = hairSystem_->GetCPUGuideData();

    // 色識別用のカラーチップを描画するラムダ関数
    auto DrawColorChip = [](const char* id, const ImVec4& color) {
        ImGui::ColorButton(id, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs, ImVec2(16, 16));
        ImGui::SameLine();
    };

    if (ImGui::Button(("Draw Point" + std::string(isDrawPoint_ ? " On" : " Off")).c_str())) {
        isDrawPoint_ = !isDrawPoint_;
    }

    if (!selectedPointIndices_.empty()) {
        ImGui::Text("Selected Points Sum: %d", (int)selectedPointIndices_.size());
        ImGui::Separator();

        // UI表示には、代表として「選択されている最初のポイント」の値を表示
        auto& firstPoint = cpuData[selectedPointIndices_[0]];
        bool isEdited = false;

        float tempRadius = firstPoint.radius;
        Vector3 tempColor = firstPoint.color;
        float tempWeight = firstPoint.physicsWeight;

        if (ImGui::SliderFloat("Radius", &tempRadius, 0.01f, 1.0f)) isEdited = true;
        if (ImGui::ColorEdit3("Color", &tempColor.x)) isEdited = true;
        if (ImGui::SliderFloat("Physics Weight", &tempWeight, 0.0f, 1.0f)) isEdited = true;

        // パラメータを動かしたら、選択されているすべてのポイントに一括適用！
        if (isEdited) {
            for (int idx : selectedPointIndices_) {
                cpuData[idx].radius = tempRadius;
                cpuData[idx].color = tempColor;
                cpuData[idx].physicsWeight = tempWeight;
            }
            hairSystem_->RequestNotifyUpdate();
        }

#pragma region ギズモ
        // =================================================================
        // 🛠️ ギズモ操作（重心でまとめて動かす ＆ ガタガタ防止版）
        // =================================================================
        float viewMatrix[16];
        float projectionMatrix[16];
        auto camera = CameraSystem::GetInstance()->GetActiveCamera();

        // 1. ビュー行列の詰め替え
        auto viewMat = camera->GetViewMatrix();
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                viewMatrix[row * 4 + col] = viewMat.m[row][col];
            }
        }

        // 2. プロジェクション行列の詰め替え
        auto projMat = camera->GetProjectionMatrix();
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                projectionMatrix[row * 4 + col] = projMat.m[row][col];
            }
        }

        // 3. 選択されたすべてのポイントの「ローカル空間での重心」を計算する
        Vector3 currentLocalCenter = { 0, 0, 0 };
        for (int idx : selectedPointIndices_) {
            currentLocalCenter.x += cpuData[idx].position.x;
            currentLocalCenter.y += cpuData[idx].position.y;
            currentLocalCenter.z += cpuData[idx].position.z;
        }
        currentLocalCenter.x /= selectedPointIndices_.size();
        currentLocalCenter.y /= selectedPointIndices_.size();
        currentLocalCenter.z /= selectedPointIndices_.size();

        // 🌟 4. ガタガタ防止！
        // ドラッグ中なら「開始時のローカル重心」、そうでないなら「現在のローカル重心」を使う
        Vector3 targetLocalCenter = isGizmoUsingLastFrame_ ? gizmoStartCenter_ : currentLocalCenter;

        // ローカル重心をワールド空間に変換してギズモに渡す
        Vector4 localPos4 = { targetLocalCenter.x, targetLocalCenter.y, targetLocalCenter.z, 1.0f };
        Vector4 worldPos4 = Matrix4x4::Transform(hairSystem_->characterMatrix_, localPos4);
        Vector3 targetWorldCenter = { worldPos4.x / worldPos4.w, worldPos4.y / worldPos4.w, worldPos4.z / worldPos4.w };

        float gizmoMatrix[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            targetWorldCenter.x, targetWorldCenter.y, targetWorldCenter.z, 1
        };

        ImGuizmo::BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::Manipulate(viewMatrix, projectionMatrix, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, gizmoMatrix);

        if (ImGuizmo::IsUsing()) {
            if (!isGizmoUsingLastFrame_) {
                // 操作開始時に全員の「ローカル位置」と「ローカル重心」をバックアップ
                gizmoOldPositions_.clear();
                for (int idx : selectedPointIndices_) {
                    gizmoOldPositions_[idx] = cpuData[idx].position;
                }
                gizmoStartCenter_ = currentLocalCenter;
            }

            // 5. ギズモから動かされた「新しいワールド重心」を取得
            Vector3 newWorldCenter = { gizmoMatrix[12], gizmoMatrix[13], gizmoMatrix[14] };

            // 6. 「新しいワールド重心」を「新しいローカル重心」に逆変換！
            Matrix4x4 invCharacterMatrix = Matrix4x4::Inverse(hairSystem_->characterMatrix_);
            Vector4 newWorldCenter4 = { newWorldCenter.x, newWorldCenter.y, newWorldCenter.z, 1.0f };
            Vector4 newLocalCenter4 = Matrix4x4::Transform(invCharacterMatrix, newWorldCenter4);
            Vector3 newLocalCenter = { newLocalCenter4.x / newLocalCenter4.w, newLocalCenter4.y / newLocalCenter4.w, newLocalCenter4.z / newLocalCenter4.w };

            // 7. ローカル空間での「総移動量（デルタ）」を計算
            Vector3 deltaLocalMove = {
                newLocalCenter.x - gizmoStartCenter_.x,
                newLocalCenter.y - gizmoStartCenter_.y,
                newLocalCenter.z - gizmoStartCenter_.z
            };

            // 8. すべてのポイントの元の位置に、ローカル空間の総移動量を足す
            for (int idx : selectedPointIndices_) {
                cpuData[idx].position.x = gizmoOldPositions_[idx].x + deltaLocalMove.x;
                cpuData[idx].position.y = gizmoOldPositions_[idx].y + deltaLocalMove.y;
                cpuData[idx].position.z = gizmoOldPositions_[idx].z + deltaLocalMove.z;

                cpuData[idx].homePosition = cpuData[idx].position;
            }
            hairSystem_->RequestNotifyUpdate();
            isGizmoUsingLastFrame_ = true;
        }
        else {
            if (isGizmoUsingLastFrame_) {
                // 操作を終えた瞬間にコマンド発行
                std::vector<int> indices;
                std::vector<Vector3> oldPos;
                std::vector<Vector3> newPos;

                for (int idx : selectedPointIndices_) {
                    indices.push_back(idx);
                    oldPos.push_back(gizmoOldPositions_[idx]);
                    newPos.push_back(cpuData[idx].position);

                    // 一旦戻す（コマンド内で書き換えて歴史に刻むため）
                    cpuData[idx].position = gizmoOldPositions_[idx];
                    cpuData[idx].homePosition = gizmoOldPositions_[idx];
                }

                ExecuteCommand(std::make_unique<HairGuideMoveCommand>(
                    hairSystem_, indices, oldPos, newPos
                ));

                isGizmoUsingLastFrame_ = false;
            }
        }
    }
#pragma endregion
    ImGui::Separator();

    // ----------------------------------------------------------------
    // 🆕 選択中ガイドの構造編集（頂点追加・削除・ミラー複製）
    // ----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Selected Guide Operations", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uint32_t totalGuideCount = hairSystem_->GetCPUGuideInfoCount();
        auto* guideInfos = hairSystem_->GetCPUGuideInfoData();

        // 有効なガイド総数を取得
        int guideMaxCount = 0;
        for (size_t i = 0; i < totalGuideCount; ++i) {
            if (guideInfos[i].vertexCount > 0) {
                guideMaxCount++;
            }
        }

        if (guideMaxCount > 0) {
            // ミラー・単一編集用に独立した selectedGuideIndex_ を直接選択
            DrawColorChip("##YellowMarker", ImVec4(1.0f, 0.85f, 0.0f, 1.0f));
            ImGui::SliderInt("Target Guide Index", &selectedGuideIndex_, 0, guideMaxCount - 1);

            if (selectedGuideIndex_ >= 0 && selectedGuideIndex_ < static_cast<int>(totalGuideCount)) {
                const auto& activeGuide = guideInfos[selectedGuideIndex_];
                ImGui::Text("Vertex Count: %u", activeGuide.vertexCount);

                // --- 頂点追加ボタン ---
                if (ImGui::Button("+ Add Vertex (Tip)", ImVec2(150, 26))) {
                    hairSystem_->AddVertexToGuide(static_cast<uint32_t>(selectedGuideIndex_));
                }

                ImGui::SameLine();

                // --- 頂点削除ボタン ---
                bool canRemove = (activeGuide.vertexCount > 2); // 最小2個維持
                if (!canRemove) ImGui::BeginDisabled();

                if (ImGui::Button("- Remove Tip Vertex", ImVec2(150, 26))) {
                    hairSystem_->RemoveVertexFromGuide(static_cast<uint32_t>(selectedGuideIndex_));
                }

                if (!canRemove) {
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("A guide must have at least 2 points.");
                    }
                }

                ImGui::Spacing();

                // --- ミラー複製 ---
                static float mirrorAxisX = 0.0f;
                ImGui::DragFloat("Mirror Axis X", &mirrorAxisX, 0.01f, -10.0f, 10.0f, "%.3f");

                if (ImGui::Button("Mirror Copy Selected Guide", ImVec2(-1, 26))) {
                    CreateMirrorGuide(static_cast<uint32_t>(selectedGuideIndex_), mirrorAxisX);
                }
            }
        }
        else {
            ImGui::TextDisabled("No active guides available.");
        }
    }

    ImGui::Separator();

    // ----------------------------------------------------------------
    // 🆕 要素の動的追加セクション
    // ----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Add New Hair Elements", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // ------------------------------------------------------------
        // 1. ガイドの動的追加 UI
        // ------------------------------------------------------------
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "--- Add New Guide ---");

        ImGui::SliderInt("ガイドを構成する頂点の数", &addGuideNum_, 1, 100);
        ImGui::DragFloat3("Root Position", &newGuidePos.x, 0.01f);
        ImGui::DragFloat3("Direction", &newGuideDir.x, 0.01f);
        ImGui::DragFloat("Total Length", &newGuideLength, 0.01f, 0.05f, 2.0f);
        ImGui::DragFloat("Root Radius", &newGuideRootRad, 0.001f, 0.001f, 0.1f);
        ImGui::DragFloat("Tip Radius", &newGuideTipRad, 0.001f, 0.001f, 0.1f);
        ImGui::ColorEdit3("Root Color", newGuideRootCol);
        ImGui::ColorEdit3("Tip Color", newGuideTipCol);

        //if (ImGui::Button("Spawn New Guide", ImVec2(-1, 26)))
        //{
        //    Vector3 rCol = { newGuideRootCol[0], newGuideRootCol[1], newGuideRootCol[2] };
        //    Vector3 tCol = { newGuideTipCol[0],  newGuideTipCol[1],  newGuideTipCol[2] };

        //    // ガイドを追加！
        //    AddGuide(newGuidePos, newGuideDir, newGuideLength, newGuideRootRad, newGuideTipRad, rCol, tCol);
        //}

        ImGui::Separator();

        // ------------------------------------------------------------
        // 子髪（ChildStrand）の動的追加 UI
        // ------------------------------------------------------------
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "--- Add New Child Strand ---");

        // エディタが現在選択しているガイド（selectedGuideIndex_）を親として自動指定
        // 赤マーカー
        DrawColorChip("##TxtRed", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        ImGui::Text("Target Parent Guide ID.1: %d", selectedGuideIndices_[0]);

        // 緑マーカー
        DrawColorChip("##TxtGreen", ImVec4(0.25f, 1.0f, 0.25f, 1.0f));
        ImGui::Text("Target Parent Guide ID.2: %d", selectedGuideIndices_[1]);

        // 青マーカー
        DrawColorChip("##TxtBlue", ImVec4(0.25f, 0.65f, 1.0f, 1.0f));
        ImGui::Text("Target Parent Guide ID.3: %d", selectedGuideIndices_[2]);

        static Vector2 childOffset = { 0.03f, 0.0f }; // ガイドから少しずらす
        static int childVertexCount = 16;             // 補間後の標準的な頂点数
        static float childLengthScale = 1.0f;
        static float childTwistAngle = 0.0f;
        static float childClumpForce = 0.0f;
        static int generateCount = 10;// 生成する子髪の数
        static float spreadRadius = 0.05f;// 生成する範囲(単一の場合のみ使用する)

        if(ImGui::Button(("参照しているガイドのモード\n変換　Now：" + std::string(blendMode_ == false ? " 単一" : " 複数")).c_str()))
        {
            blendMode_ = !blendMode_;
        }

        // アクティブなガイドの数を計算している
        int guideMaxCount = 0;
        for (size_t i = 0; i < hairSystem_->GetCPUGuideInfoCount(); ++i) {
            if (hairSystem_->GetCPUGuideInfoData()[i].vertexCount > 0) {
                guideMaxCount++;
            }
        }
        
        if (blendMode_) {
            // 赤マーカー
            DrawColorChip("##SldRed", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
            ImGui::SliderInt("Parent Guide ID.1", &selectedGuideIndices_[0], 0, (int)(guideMaxCount - 1));

            // 緑マーカー
            DrawColorChip("##SldGreen", ImVec4(0.25f, 1.0f, 0.25f, 1.0f));
            ImGui::SliderInt("Parent Guide ID.2", &selectedGuideIndices_[1], 0, (int)(guideMaxCount - 1));

            // 青マーカー
            DrawColorChip("##SldBlue", ImVec4(0.25f, 0.65f, 1.0f, 1.0f));
            ImGui::SliderInt("Parent Guide ID.3", &selectedGuideIndices_[2], 0, (int)(guideMaxCount - 1));
        }
        else {
            // 赤マーカー
            DrawColorChip("##SldRedSingle", ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
            ImGui::SliderInt("Parent Guide ID", &selectedGuideIndices_[0], 0, (int)(guideMaxCount - 1));
        }

        ImGui::DragFloat2("Offset (X, Y)", &childOffset.x, 0.002f, -0.2f, 0.2f);
        ImGui::SliderInt("構成する頂点の数 / Vertex Count", &childVertexCount, 2, 256);
        ImGui::SliderFloat("Length Scale", &childLengthScale, 0.1f, 2.0f);
        ImGui::SliderFloat("Twist Angle", &childTwistAngle, -3.14f, 3.14f);
        ImGui::SliderFloat("髪先がガイドに近づく強度 / Clump Force", &childClumpForce, 0.0f, 1.0f);

        ImGui::Separator();

        ImGui::SliderInt("一括生成数 / Generate Count", &generateCount, 1, 500);
        if (!blendMode_) {
            ImGui::SliderFloat("散布半径 / Spread Radius", &spreadRadius, 0.0f, 0.2f);
        }
        else {
            ImGui::BeginDisabled(); // 面生成時はOffset無効なのでグレーアウト
            ImGui::SliderFloat("散布半径 / Spread Radius", &spreadRadius, 0.0f, 0.2f);
            ImGui::EndDisabled();
        }

        if (ImGui::Button("上記の設定で子髪を追加", ImVec2(-1, 26)))
        {
            // RandomUtils の高精度乱数ジェネレータを取得
            auto& random = RandomUtils::GetInstance()->GetHighRandom();

            for (int i = 0; i < generateCount; ++i) {
                Vector2 currentOffset = childOffset;

                // Weightの計算とOffsetの計算
                if (blendMode_ == false) {
                    // 💡 単一ガイド参照モード（円柱状に散らす）
                    weights_[0] = 1.0f;
                    weights_[1] = 0.0f;
                    weights_[2] = 0.0f;

                    // ランダムな円の計算
                    float angle = random.GetFloat(0.0f, 3.14159265f * 2.0f);
                    float radius = std::sqrt(random.GetFloat(0.0f, 1.0f)) * spreadRadius;
                    currentOffset.x += std::cos(angle) * radius;
                    currentOffset.y += std::sin(angle) * radius;
                }
                else {
                    // 複数ガイド（3本）参照モード（三角形の面内に散らす）
                    float r1 = std::sqrt(random.GetFloat(0.0f, 1.0f));
                    float r2 = random.GetFloat(0.0f, 1.0f);

                    weights_[0] = 1.0f - r1;
                    weights_[1] = r1 * (1.0f - r2);
                    weights_[2] = r1 * r2;

                    // 面の時はOffsetによる散らばりはゼロにする
                    currentOffset = { 0.0f, 0.0f };
                }

                // 現在エディタで選択しているガイドのIDを親にして子髪を追加！
                AddChildStrand(
                    selectedGuideIndices_,
                    weights_,
                    currentOffset,
                    static_cast<uint32_t>(childVertexCount),
                    childLengthScale,
                    childTwistAngle,
                    childClumpForce
                );
            }
        }
    }

    // UIのボタン
    if (ImGui::Button("Save to JSON")) {

        // 1. JSON用の保存ダイアログを開く
        std::string savePath = FileSystem::ShowSaveFileDialogJson();

        // 2. パスが取得できたらJSON保存を実行
        if (!savePath.empty()) {
            //SaveHairDataToJson(savePath, cpuData, count);
            if (hairSystem_->SaveToFile(savePath)) {
                // セーブ成功のログなど
            }
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Load from JSON")) {
        // 1. ファイル選択ダイアログを開く
        std::string openPath = FileSystem::ShowOpenFileDialogJson();

        // 2. パスが取得できたら読み込み処理を実行
        if (!openPath.empty()) {
            //LoadHairDataFromJson(openPath, cpuData, count, hairSystem_);
            if (hairSystem_->LoadFromFile(openPath)) {
                // ロードに成功したら、エディタ側の選択中のインデックス等を安全にリセット
                selectedGuideIndices_[0] = 0;
                selectedGuideIndices_[1] = -1;
                selectedGuideIndices_[2] = -1;
                selectedPointIndices_ = { 0 };
            }
        }
    }

    ImGui::Separator();

    // スプライン編集モードのUI
    if (!isEditingSpline_) {
        // 通常時：スプライン編集モードを開始するボタン
        if (ImGui::Button("Start Spline Edit Mode", ImVec2(-1, 26))) {
            // HermiteEditorをその時だけ生成する
            hermiteEditor_ = std::make_unique<HermiteEditor>();

            isEditingSpline_ = true;
        }
    }
    else {
        // スプライン編集中：専用のUIを表示
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "--- Spline Edit Mode Active ---");

        // HermiteEditor側のUIを描画（HermiteEditor自体にUIがあれば）
        if (hermiteEditor_) {
            hermiteEditor_->DrawUI();
        }

        // 編集したスプラインからGuideを生成して終了
        if (ImGui::Button("Apply Spline to Guide", ImVec2(-1, 30))) {
            // 編集中のノードを取得
            const auto& activeNodes = hermiteEditor_->GetTargetObject();

            Vector3 rCol = { newGuideRootCol[0], newGuideRootCol[1], newGuideRootCol[2] };
            Vector3 tCol = { newGuideTipCol[0],  newGuideTipCol[1],  newGuideTipCol[2] };

            // Guideを生成
            AddGuideFromSpline(activeNodes, newGuideRootRad, newGuideTipRad, rCol, tCol);

            // 終了処理
            isEditingSpline_ = false;
            hermiteEditor_.reset();
        }

        ImGui::SameLine();

        // 破棄してキャンセル
        if (ImGui::Button("Cancel", ImVec2(-1, 30))) {
            isEditingSpline_ = false;
            hermiteEditor_.reset();
        }
    }
#endif
}

void HairGuideEditor::AddGuide(const Vector3& rootPosition, const Vector3& direction, float totalLength,
    float rootRadius, float tipRadius, const Vector3& rootColor, const Vector3& tipColor) {
#ifdef USE_IMGUI
    if (addGuideNum_ < 2) {
        // ガイドの頂点数が1以下の場合は無効なので処理を中断
        Log::View("Invalid guide count. Please set a value greater than 1.");
        return;
    }

    auto* guideInfoData = hairSystem_->GetCPUGuideInfoData();
    int targetGuideIndex = -1;      // 空いている「何本目か」のインデックス
    uint32_t nextVertexStartIndex = 0; // 次のガイドが使い始めるべき「頂点開始インデックス」

    // 空きスロットを探しつつ、同時にこれまでに使われている頂点数の合計を計算する
    for (uint32_t i = 0; i < hairSystem_->GetCPUGuideInfoCount(); ++i) {
        if (guideInfoData[i].vertexCount == 0) {
            targetGuideIndex = i; // 最初の空きスロットを見つけた
            break;
        }
        // 使用中のガイドがある場合、そのガイドの「開始位置 + 頂点数」が次のガイドの開始位置になる
        nextVertexStartIndex = guideInfoData[i].vertexStartIndex + guideInfoData[i].vertexCount;
    }

    // もし最大本数まで使い切っていたら中断
    if (targetGuideIndex == -1) {
        Log::View("Guide limit reached. Cannot add more guides.");
        return;
    }

    // 新しいガイドInfoの構築
    GuideCurve::GuideInfo info;
    info.vertexStartIndex = nextVertexStartIndex; // 正しい頂点バッファの開始位置
    info.vertexCount = static_cast<uint32_t>(addGuideNum_);

    // CPU頂点バッファに数値を流し込む
    auto cpuData = hairSystem_->GetCPUGuideData();
    float segmentLength = totalLength / (addGuideNum_ - 1);
    Vector3 normDir = Normalize(direction); // 伸びる方向

    for (int i = 0; i < addGuideNum_; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(addGuideNum_ - 1);

        GuideCurve::ControllerPoint p;
        p.position = rootPosition + normDir * (segmentLength * i);
        p.homePosition = p.position; // 初期姿勢として保存
        p.radius = Lerp(rootRadius, tipRadius, t); // 根元から毛先へ細くする
        p.color = Lerp(rootColor, tipColor, t);   // 根元から毛先へのグラデーション
        p.nextToLength = (i == addGuideNum_ - 1) ? 0.0f : segmentLength;
        p.physicsWeight = t; // 根元 0.0 (固定) ～ 毛先 1.0 (物理で揺れる)

        // 正しい位置（info.vertexStartIndex からのオフセット）に書き込む
        cpuData[info.vertexStartIndex + i] = p;
    }

    // 【重要】見つけた空きスロットに、新しく作ったGuideInfoを書き戻す！
    guideInfoData[targetGuideIndex] = info;

    // GPU側に更新を通知
    hairSystem_->RequestNotifyUpdate();
    hairSystem_->MarkGuideTopologyDirty();
#endif
}

void HairGuideEditor::AddGuideFromSpline(const std::vector<MathUtils::Spline::Node<Vector3>*>& splineNodePtrs,
    float rootRadius, float tipRadius, const Vector3& rootColor, const Vector3& tipColor) {
#ifdef USE_IMGUI
    if (addGuideNum_ < 2) {
        Log::View("Invalid guide count. Please set a value greater than 1.");
        return;
    }
    if (splineNodePtrs.size() < 2) {
        Log::View("Spline must have at least 2 nodes to generate a guide.");
        return;
    }

    std::vector<MathUtils::Spline::Node<Vector3>> splineNodes;
    splineNodes.reserve(splineNodePtrs.size());
    for (const auto* nodePtr : splineNodePtrs) {
        if (nodePtr) {
            splineNodes.push_back(*nodePtr);
        }
    }

    auto* guideInfoData = hairSystem_->GetCPUGuideInfoData();
    int targetGuideIndex = -1;
    uint32_t nextVertexStartIndex = 0;

    for (uint32_t i = 0; i < hairSystem_->GetCPUGuideInfoCount(); ++i) {
        if (guideInfoData[i].vertexCount == 0) {
            targetGuideIndex = i;
            break;
        }
        nextVertexStartIndex = guideInfoData[i].vertexStartIndex + guideInfoData[i].vertexCount;
    }

    if (targetGuideIndex == -1) {
        Log::View("Guide limit reached. Cannot add more guides.");
        return;
    }

    // 1. ループ外でキャラクターのワールド行列の逆行列を事前計算
    Matrix4x4 invCharMatrix = Matrix4x4::Inverse(hairSystem_->characterMatrix_);

    GuideCurve::GuideInfo info;
    info.vertexStartIndex = nextVertexStartIndex;
    info.vertexCount = static_cast<uint32_t>(addGuideNum_);

    std::vector<GuideCurve::ControllerPoint> newPoints(addGuideNum_);

    // 2. スプライン上の座標を取得してローカル座標に変換
    for (int i = 0; i < addGuideNum_; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(addGuideNum_ - 1);

        GuideCurve::ControllerPoint p;

        // ワールド座標を取得
        Vector3 worldPos = MathUtils::Spline::GetPointSpline(splineNodes, t);

        // キャラクターのローカル空間座標へ変換
        p.position = Matrix4x4::Transform(worldPos, invCharMatrix);
        p.homePosition = p.position;

        // 根元から毛先への補間
        p.radius = rootRadius * (1.0f - t) + tipRadius * t;
        p.color.x = rootColor.x * (1.0f - t) + tipColor.x * t;
        p.color.y = rootColor.y * (1.0f - t) + tipColor.y * t;
        p.color.z = rootColor.z * (1.0f - t) + tipColor.z * t;

        // 次の点への距離(nextToLength)の計算
        if (i < addGuideNum_ - 1) {
            float nextT = static_cast<float>(i + 1) / static_cast<float>(addGuideNum_ - 1);

            Vector3 nextWorldPos = MathUtils::Spline::GetPointSpline(splineNodes, nextT);
            Vector3 nextLocalPos = Matrix4x4::Transform(nextWorldPos, invCharMatrix);

            // ローカル空間での距離（必要に応じてVector3::Distanceなどのヘルパー関数を使用）
            p.nextToLength = Length(nextLocalPos - p.position);
        }
        else {
            p.nextToLength = 0.0f;
        }

        p.physicsWeight = t;

        newPoints[i] = p;
    }

    GuideCurve::GuideInfo oldInfo = guideInfoData[targetGuideIndex];

    auto command = std::make_unique<AddGuideCommand>(
        hairSystem_, targetGuideIndex, oldInfo, info, newPoints
    );

    ExecuteCommand(std::move(command));
}

void HairGuideEditor::AddChildStrand(
    int parentGuideIds[3],   // 追従する親ガイドのID
    float parentWeights[3],
    const Vector2& offset,    // ガイドからの2Dオフセット（散らばり具合）
    uint32_t vertexCount,     // この髪の毛1本の頂点数（例: 8や16など補間後の数）
    float lengthScale,        // 長さの倍率
    float twistAngle,         // ねじれ
    float clumpForce          // 束感の強さ
) {
    if (vertexCount < 2) {
        Log::View("Invalid strand vertex count. Please set a value greater than 1.");
        return;
    }

    // 髪システムから各CPUバッファのポインタと最大数を取得
    auto strandInfoData = hairSystem_->GetCPUStrandInfoData();
    auto childStrandData = hairSystem_->GetCPUChildStrandData();
    auto segmentData = hairSystem_->GetCPUSegmentData();
    int maxStrandCount = hairSystem_->GetCPUStrandInfoCount();

    int targetStrandIndex = -1;      // 空いている「何本目か」のインデックス
    uint32_t nextVertexStartIndex = 0; // 次の髪頂点が使い始めるべきインデックス
    uint32_t nextAABBStartIndex = 0;   // 次のAABB（セグメント）が使い始めるべきインデックス

    // 1. 空きスロットを探しつつ、同時にこれまでに消費されている頂点数・AABB数の合計を累積計算
    for (int i = 0; i < maxStrandCount; ++i) {
        if (strandInfoData[i].vertexCount == 0) {
            targetStrandIndex = i; // 最初の空きスロットを発見
            break;
        }
        // 使用中のストランドから、次のストランドのための開始位置を累積
        nextVertexStartIndex = strandInfoData[i].vertexStartIndex + strandInfoData[i].vertexCount;

        // AABB（セグメント）の数は「頂点数 - 1」個消費されるため、その合計を累積
        nextAABBStartIndex = strandInfoData[i].aabbStartIndex + (strandInfoData[i].vertexCount - 1);
    }

    // プールが満杯なら中断
    if (targetStrandIndex == -1) {
        Log::View("Strand limit reached. Cannot add more child strands.");
        return;
    }

    // StrandInfo（管理メタデータ）の構築
    Strands::StrandInfo info;
    info.vertexStartIndex = nextVertexStartIndex;
    info.vertexCount = vertexCount;
    info.aabbStartIndex = nextAABBStartIndex; // 計算したAABBの開始位置をセット

    // ChildStrand（生成パラメータ）の構築
    // まだ単一だが、ちゃんと実装できていれば複数に対応させる
    Strands::ChildStrand child;
    child.parentGuideIds[0] = parentGuideIds[0];
    child.parentGuideIds[1] = parentGuideIds[1];
    child.parentGuideIds[2] = parentGuideIds[2];
    child.blendMode = blendMode_;         // 0: 単一ガイド追従モード
    child.weights[0] = parentWeights[0];     // このガイドの影響度100%
    child.weights[1] = parentWeights[1];
    child.weights[2] = parentWeights[2];
    child.offset = offset;
    child.lengthScale = lengthScale;
    child.twistAngle = twistAngle;
    child.clumpForce = clumpForce;
	child.waveAmplitude = 0.0f; // 波打ちの振幅（未使用）
	child.waveFrequency = 0.0f; // 波打ちの周波数（未使用）
    child.noise = 0.0f;
    // ※もしHLSL側の構造体に他にもメンバ（paddingや追加パラメータ）があればここで初期化

    // セグメントデータを一時配列に格納
    uint32_t segmentCount = info.vertexCount - 1;
    std::vector<Strands::SegmentData> newSegments(segmentCount);
    for (uint32_t j = 0; j < segmentCount; ++j) {
        Strands::SegmentData seg;
        seg.v0_Index = info.vertexStartIndex + j;
        seg.v1_Index = info.vertexStartIndex + j + 1;
        newSegments[j] = seg;
    }

    // 3. Undo用の古い状態を取得
    Strands::StrandInfo oldInfo = strandInfoData[targetStrandIndex];

    // 4. コマンドを発行して履歴に積む
    auto command = std::make_unique<AddChildStrandCommand>(
        hairSystem_,
        targetStrandIndex,
        oldInfo,
        info,
        child,
        newSegments
    );
    ExecuteCommand(std::move(command));

    {
        //// 固定長バッファの空いたスロットに数値をぶち込む！
        //strandInfoData[targetStrandIndex] = info;
        //childStrandData[targetStrandIndex] = child;

        //// 1本の髪に含まれるセグメントの数は必ず (頂点数 - 1) 個になります
        //uint32_t segmentCount = info.vertexCount - 1;
        //for (uint32_t j = 0; j < segmentCount; ++j) {
        //    Strands::SegmentData seg;
        //    // このセグメントを構成する「始点」と「終点」の絶対頂点インデックスを計算
        //    seg.v0_Index = info.vertexStartIndex + j;
        //    seg.v1_Index = info.vertexStartIndex + j + 1;

        //    // グローバルなAABB配列の正しいオフセット位置に流し込む
        //    uint32_t globalAABBIndex = info.aabbStartIndex + j;
        //    segmentData[globalAABBIndex] = seg;
        //}

        //// もし ConstantBuffer 等の numStrands を有効数として使っている場合、更新する
        //// 現在のインデックス + 1 が、最低限必要な有効ストランド数になります
        ///*if (hairSystem_->GetNumStrands() <= targetStrandIndex) {
        //    hairSystem_->SetNumStrands(targetStrandIndex + 1);
        //}*/

        //// GPU側に更新と、Compute Shaderによる髪再生成をリクエスト
        //hairSystem_->RequestNotifyUpdate();
    }
#endif
}

bool HairGuideEditor::SaveHairSaveData(const std::string& filename, const Strands::HairSaveData& saveData) {
    // バイナリ書き込みモードでファイルを開く
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return false;
    }

    // 1. 固定サイズの構造体をそのまま書き出す
    ofs.write(reinterpret_cast<const char*>(&saveData.physicsConfig), sizeof(saveData.physicsConfig));
    ofs.write(reinterpret_cast<const char*>(&saveData.makeConfig), sizeof(saveData.makeConfig));
    ofs.write(reinterpret_cast<const char*>(&saveData.hairConfig), sizeof(saveData.hairConfig));

    // 2. 各 std::vector の要素数とデータ本体を書き出す

    // points
    uint32_t pointsCount = static_cast<uint32_t>(saveData.points.size());
    ofs.write(reinterpret_cast<const char*>(&pointsCount), sizeof(pointsCount));
    if (pointsCount > 0) {
        ofs.write(reinterpret_cast<const char*>(saveData.points.data()), pointsCount * sizeof(GuideCurve::ControllerPoint));
    }

    // guideInfo
    uint32_t guideInfoCount = static_cast<uint32_t>(saveData.guideInfo.size());
    ofs.write(reinterpret_cast<const char*>(&guideInfoCount), sizeof(guideInfoCount));
    if (guideInfoCount > 0) {
        ofs.write(reinterpret_cast<const char*>(saveData.guideInfo.data()), guideInfoCount * sizeof(GuideCurve::GuideInfo));
    }

    // childStrands
    uint32_t childStrandsCount = static_cast<uint32_t>(saveData.childStrands.size());
    ofs.write(reinterpret_cast<const char*>(&childStrandsCount), sizeof(childStrandsCount));
    if (childStrandsCount > 0) {
        ofs.write(reinterpret_cast<const char*>(saveData.childStrands.data()), childStrandsCount * sizeof(Strands::ChildStrand));
    }

    // strandInfos
    uint32_t strandInfosCount = static_cast<uint32_t>(saveData.strandInfos.size());
    ofs.write(reinterpret_cast<const char*>(&strandInfosCount), sizeof(strandInfosCount));
    if (strandInfosCount > 0) {
        ofs.write(reinterpret_cast<const char*>(saveData.strandInfos.data()), strandInfosCount * sizeof(Strands::StrandInfo));
    }

    // segments
    uint32_t segmentsCount = static_cast<uint32_t>(saveData.segments.size());
    ofs.write(reinterpret_cast<const char*>(&segmentsCount), sizeof(segmentsCount));
    if (segmentsCount > 0) {
        ofs.write(reinterpret_cast<const char*>(saveData.segments.data()), segmentsCount * sizeof(Strands::SegmentData));
    }

    ofs.close();
    return true;
}

bool HairGuideEditor::LoadHairSaveData(const std::string& filename, Strands::HairSaveData& outSaveData) {
    // バイナリ読み込みモードでファイルを開く
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open file for loading: " << filename << std::endl;
        return false;
    }

    // 1. 固定サイズの構造体を読み込む
    ifs.read(reinterpret_cast<char*>(&outSaveData.physicsConfig), sizeof(outSaveData.physicsConfig));
    ifs.read(reinterpret_cast<char*>(&outSaveData.makeConfig), sizeof(outSaveData.makeConfig));
    ifs.read(reinterpret_cast<char*>(&outSaveData.hairConfig), sizeof(outSaveData.hairConfig));

    // 2. 各 std::vector の要素数を読み込み、resize してからデータ本体を読み込む

    // points
    uint32_t pointsCount = 0;
    ifs.read(reinterpret_cast<char*>(&pointsCount), sizeof(pointsCount));
    outSaveData.points.resize(pointsCount);
    if (pointsCount > 0) {
        ifs.read(reinterpret_cast<char*>(outSaveData.points.data()), pointsCount * sizeof(GuideCurve::ControllerPoint));
    }

    // guideInfo
    uint32_t guideInfoCount = 0;
    ifs.read(reinterpret_cast<char*>(&guideInfoCount), sizeof(guideInfoCount));
    outSaveData.guideInfo.resize(guideInfoCount);
    if (guideInfoCount > 0) {
        ifs.read(reinterpret_cast<char*>(outSaveData.guideInfo.data()), guideInfoCount * sizeof(GuideCurve::GuideInfo));
    }

    // childStrands
    uint32_t childStrandsCount = 0;
    ifs.read(reinterpret_cast<char*>(&childStrandsCount), sizeof(childStrandsCount));
    outSaveData.childStrands.resize(childStrandsCount);
    if (childStrandsCount > 0) {
        ifs.read(reinterpret_cast<char*>(outSaveData.childStrands.data()), childStrandsCount * sizeof(Strands::ChildStrand));
    }

    // strandInfos
    uint32_t strandInfosCount = 0;
    ifs.read(reinterpret_cast<char*>(&strandInfosCount), sizeof(strandInfosCount));
    outSaveData.strandInfos.resize(strandInfosCount);
    if (strandInfosCount > 0) {
        ifs.read(reinterpret_cast<char*>(outSaveData.strandInfos.data()), strandInfosCount * sizeof(Strands::StrandInfo));
    }

    // segments
    uint32_t segmentsCount = 0;
    ifs.read(reinterpret_cast<char*>(&segmentsCount), sizeof(segmentsCount));
    outSaveData.segments.resize(segmentsCount);
    if (segmentsCount > 0) {
        ifs.read(reinterpret_cast<char*>(outSaveData.segments.data()), segmentsCount * sizeof(Strands::SegmentData));
    }

    ifs.close();
    return true;
}

bool HairGuideEditor::SaveToFile(const std::string& filename) {
    if (!hairSystem_) return false;

    Strands::HairSaveData saveData{};

    // 1. 各種コンフィグ設定の取得
    /*if (auto* physics = hairSystem_->GetCPUPhysicsConfig()) {
        saveData.physicsConfig = *physics;
    }
    if (auto* make = hairSystem_->GetCPUMakeConfig()) {
        saveData.makeConfig = *make;
    }*/
    if (auto* config = hairSystem_->GetCPUGuideConfig()) {
        saveData.hairConfig = *config;
    }

    // 2. ガイド制御点（ControllerPoint）の吸い出し
    uint32_t guideCount = hairSystem_->GetCPUGuideCount();
    auto* guideData = hairSystem_->GetCPUGuideData();
    if (guideCount > 0 && guideData) {
        saveData.points.assign(guideData, guideData + guideCount);
    }

    // 3. ガイド情報（GuideInfo）の吸い出し
    uint32_t guideInfoCount = hairSystem_->GetCPUGuideInfoCount();
    auto* guideInfoData = hairSystem_->GetCPUGuideInfoData();
    if (guideInfoCount > 0 && guideInfoData) {
        saveData.guideInfo.assign(guideInfoData, guideInfoData + guideInfoCount);
    }

    // 4. 子ストランド設定（ChildStrand）とストランド情報（StrandInfo）の吸い出し
    uint32_t strandCount = hairSystem_->GetCPUStrandInfoCount();
    auto* strandInfoData = hairSystem_->GetCPUStrandInfoData();
    auto* childStrandData = hairSystem_->GetCPUChildStrandData();

    if (strandCount > 0) {
        if (strandInfoData) {
            saveData.strandInfos.assign(strandInfoData, strandInfoData + strandCount);
        }
        if (childStrandData) {
            saveData.childStrands.assign(childStrandData, childStrandData + strandCount);
        }
    }

    // 5. セグメントデータ（SegmentData）の吸い出し
    // strandInfos から全髪の総セグメント数を計算
    uint32_t totalSegments = 0;
    for (const auto& info : saveData.strandInfos) {
        if (info.vertexCount > 1) {
            totalSegments += (info.vertexCount - 1); // (頂点数 - 1) がセグメント数
        }
    }
    auto* segmentData = hairSystem_->GetCPUSegmentData();
    if (totalSegments > 0 && segmentData) {
        saveData.segments.assign(segmentData, segmentData + totalSegments);
    }

    // 6. バイナリファイルへの書き出し
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return false;
    }

    // 固定サイズ構造体の保存
    ofs.write(reinterpret_cast<const char*>(&saveData.physicsConfig), sizeof(saveData.physicsConfig));
    ofs.write(reinterpret_cast<const char*>(&saveData.makeConfig), sizeof(saveData.makeConfig));
    ofs.write(reinterpret_cast<const char*>(&saveData.hairConfig), sizeof(saveData.hairConfig));

    // 各 std::vector を「要素数 + データ本体」の順で保存するラムダ関数
    auto writeVector = [&ofs](const auto& vec) {
        uint32_t count = static_cast<uint32_t>(vec.size());
        ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));
        if (count > 0) {
            using ElementType = typename std::decay_t<decltype(vec)>::value_type;
            ofs.write(reinterpret_cast<const char*>(vec.data()), count * sizeof(ElementType));
        }
        };

    writeVector(saveData.points);
    writeVector(saveData.guideInfo);
    writeVector(saveData.childStrands);
    writeVector(saveData.strandInfos);
    writeVector(saveData.segments);

    ofs.close();
    return true;
}

bool HairGuideEditor::LoadFromFile(const std::string& filename, Fngine* engine) {
    if (!hairSystem_ || !engine) return false;

    Strands::HairSaveData loadedData{};

    // 1. バイナリファイルの読み込み
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open file for loading: " << filename << std::endl;
        return false;
    }

    // 固定サイズ構造体の読み込み
    ifs.read(reinterpret_cast<char*>(&loadedData.physicsConfig), sizeof(loadedData.physicsConfig));
    ifs.read(reinterpret_cast<char*>(&loadedData.makeConfig), sizeof(loadedData.makeConfig));
    ifs.read(reinterpret_cast<char*>(&loadedData.hairConfig), sizeof(loadedData.hairConfig));

    // 各 std::vector を「要素数を取得 -> resize -> データ本体を復元」するラムダ関数
    auto readVector = [&ifs](auto& vec) {
        uint32_t count = 0;
        ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
        vec.resize(count);
        if (count > 0) {
            using ElementType = typename std::decay_t<decltype(vec)>::value_type;
            ifs.read(reinterpret_cast<char*>(vec.data()), count * sizeof(ElementType));
        }
        };

    readVector(loadedData.points);
    readVector(loadedData.guideInfo);
    readVector(loadedData.childStrands);
    readVector(loadedData.strandInfos);
    readVector(loadedData.segments);

    ifs.close();

    // 2. 読み込んだデータを渡し、IHair のバッファ群を再初期化（再生成）させる
    // IHair::Initialize(engine, isLoad=true, savedata) を呼び出す
    hairSystem_->Initialize(engine, true, loadedData);

    // エディタ側の選択中のガイドインデックスなども安全のために初期化
    selectedGuideIndices_[0] = 0;
    selectedGuideIndices_[1] = -1;
    selectedGuideIndices_[2] = -1;
    selectedPointIndices_ = { 0 };

    return true;
}

bool HairGuideEditor::CreateMirrorGuide(uint32_t sourceGuideIndex, float mirrorAxisX) {
    if (!hairSystem_) return false;

    // ガイドの構成情報
    auto* guideInfos = hairSystem_->GetCPUGuideInfoData();
    // ガイドの制御点情報
    auto* points = hairSystem_->GetCPUGuideData();
    // ガイドの総数（最大容量）
    uint32_t totalGuideCount = hairSystem_->GetCPUGuideInfoCount();

    // 引数のインデックスが範囲外なら処理不可能なので終了
    if (sourceGuideIndex >= totalGuideCount) return false;
    // 複製元のガイド情報を取得
    const auto& srcInfo = guideInfos[sourceGuideIndex];
    // 空のガイドは処理不可
    if (srcInfo.vertexCount == 0) return false;

    // 未使用のガイドスロットを検索
    int targetGuideIndex = -1;
    for (uint32_t g = 0; g < totalGuideCount; ++g) {
        if (guideInfos[g].vertexCount == 0) {
            targetGuideIndex = static_cast<int>(g);
            break;
        }
    }

    if (targetGuideIndex < 0) return false; // 空きスロットなし
    
    uint32_t totalActivePoints = hairSystem_->GetCPUActiveGuideCount();
    uint32_t maxCapacity = hairSystem_->GetCPUGuideCount();
    if (totalActivePoints + srcInfo.vertexCount > maxCapacity) return false; // バッファ容量オーバー

    // アクティブ頂点配列の末尾を新ガイドの開始位置とする
    uint32_t newStartIndex = totalActivePoints;

    // 複製元データを反転してセット
    for (uint32_t i = 0; i < srcInfo.vertexCount; ++i) {
        const auto& srcPoint = points[srcInfo.vertexStartIndex + i];
        GuideCurve::ControllerPoint mirrorPoint = srcPoint;

        // X軸反転 ( mirrorAxisX - (x - mirrorAxisX) )
        mirrorPoint.position.x = mirrorAxisX - (srcPoint.position.x - mirrorAxisX);
        mirrorPoint.homePosition.x = mirrorAxisX - (srcPoint.homePosition.x - mirrorAxisX);

        points[newStartIndex + i] = mirrorPoint;
    }

    // ガイド情報の登録
    guideInfos[targetGuideIndex].vertexStartIndex = newStartIndex;
    guideInfos[targetGuideIndex].vertexCount = srcInfo.vertexCount;

    // GPU更新リクエスト
    hairSystem_->RequestNotifyUpdate();
    hairSystem_->MarkGuideTopologyDirty();
    return true;
}