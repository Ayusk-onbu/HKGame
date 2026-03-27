#include "MotionManager.h"
#include <json.hpp>
#include <fstream>
#include <iostream>
#include "Structures.h"
#include "ImGuiManager.h"
#include "Easing.h"

using json = nlohmann::json;
using namespace MathUtils;

std::unique_ptr<MotionManager>MotionManager::instance_ = nullptr;
std::unique_ptr<MotionEditor>MotionEditor::instance_ = nullptr;

void MotionManager::LoadActionData(const std::string& fileName, std::vector<MathUtils::Spline::Node<Vector3>>& outNodes) {
	std::string fullPath = fileName + ".json";
	std::ifstream file(fullPath);

	if (file.is_open()) {
		json j;
		file >> j; // ファイルからJSONを読み込む
		file.close();

		// JSONからNodeの配列に復元して上書き
		outNodes = MathUtils::Spline::DeserializeNodes<Vector3>(j);
	}
}

void MotionManager::LoadMotions(const std::string& directoryPath) {
	motions_.clear();
	for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			std::string motionName = entry.path().stem().string();
			MotionData motionData;
			LoadActionData(directoryPath + motionName, motionData);
			motions_[motionName] = motionData;
		}
	}
}

const MotionData& MotionManager::GetMotion(const std::string& name) const {
	auto it = motions_.find(name);
	if (it != motions_.end()) {
		return it->second;
	}
	/*throw std::runtime_error("Motion not found: " + name);*/
	return motions_.begin()->second; // データがないときは先頭のデータを返す（要注意）
}

void MotionController::Play(const std::string& motionName, const Vector3& startPosition, float motionDuration) {
	currentMotionName_ = motionName;
	motionDuration_ = motionDuration;
	motionTimer_ = 0.0f;
	isPlaying_ = true;
	actionStartPosition_ = startPosition;
}

Vector3 MotionController::Update(float deltaTime, const Vector3& direction) {
	if (!isPlaying_) return Vector3{};
	auto& motionData = MotionManager::GetInstance()->GetMotion(currentMotionName_);
	motionTimer_ += deltaTime;
	float t = motionTimer_ / motionDuration_;
    float x = InSine(t);
	Vector3 localOffset = MathUtils::Spline::GetPointSpline(motionData, x);
	localOffset.y *= -1.0f;
	localOffset.x *= direction.x >= 0 ? 1.0f : -1.0f; // 方向に応じて左右反転

	if (motionTimer_ >= motionDuration_) {
		isPlaying_ = false; // 再生終了
	}

	return actionStartPosition_ + localOffset;
}

void MotionEditor::NodeImGui() {
#ifdef USE_IMGUI
    if (ImGui::IsKeyPressed(ImGuiKey_P)) {
        nodes_.push_back(Spline::Node<Vector3>({ 0.0f,0.0f,0.0f }));
    }

    ImGui::Begin("Action Editor (Hermite Spline)");

    // --- 1. キャンバスの準備 ---
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // ピクセル設定
    const float DISPLAY_SCALE = 75.0f;

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    ImGui::InvisibleButton("canvas", canvas_sz);
    ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_p0.x, ImGui::GetIO().MousePos.y - canvas_p0.y);
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemHovered();

    // --- 2. スプライン曲線の描画 ---
    if (nodes_.size() >= 2) {
        const int num_segments = 100;
        // キャンバス中心を使ってノード描画と同じ基準にする
        ImVec2 canvas_center = ImVec2(canvas_p0.x + canvas_sz.x * 0.5f, canvas_p0.y + canvas_sz.y * 0.5f);

        Vector3 prev_point = nodes_[0].position;
        for (int i = 1; i <= num_segments; ++i) {
            float t = (float)i / (float)num_segments;
            Vector3 current_point = MathUtils::Spline::GetPointSpline(nodes_, t);

            // 中心基準でスクリーン座標に変換
            ImVec2 p1 = ImVec2(canvas_center.x + prev_point.x * DISPLAY_SCALE, canvas_center.y + prev_point.y * DISPLAY_SCALE);
            ImVec2 p2 = ImVec2(canvas_center.x + current_point.x * DISPLAY_SCALE, canvas_center.y + current_point.y * DISPLAY_SCALE);

            draw_list->AddLine(p1, p2, IM_COL32(255, 200, 0, 255), 2.0f);
            prev_point = current_point;
        }
    }

    // --- 3. ノードとハンドルの操作＆描画 ---
    const float NODE_RADIUS = 6.0f;
    const float HANDLE_RADIUS = 4.0f;

    for (int i = 0; i < (int)nodes_.size(); ++i) {
        auto& node = nodes_[i];

        ImVec2 canvas_center = ImVec2(canvas_p0.x + canvas_sz.x * 0.5f, canvas_p0.y + canvas_sz.y * 0.5f);
        ImVec2 pos_screen = ImVec2(canvas_center.x + node.position.x * DISPLAY_SCALE, canvas_center.y + node.position.y * DISPLAY_SCALE);
        ImVec2 in_screen = ImVec2(canvas_center.x + node.TangentIn.x * DISPLAY_SCALE, canvas_center.y + node.TangentIn.y * DISPLAY_SCALE);
        ImVec2 out_screen = ImVec2(canvas_center.x + node.TangentOut.x * DISPLAY_SCALE, canvas_center.y + node.TangentOut.y * DISPLAY_SCALE);

        draw_list->AddLine(pos_screen, in_screen, IM_COL32(150, 150, 150, 200), 1.0f);
        draw_list->AddLine(pos_screen, out_screen, IM_COL32(150, 150, 150, 200), 1.0f);

        // --- クリック判定（左/右/中 を全てチェック） ---
        if (is_hovered && ImGui::IsMouseClicked(0)) {
            auto dist = [](ImVec2 a, ImVec2 b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y); };
            ImVec2 mouse_p = ImGui::GetIO().MousePos;
            if (dist(mouse_p, pos_screen) < NODE_RADIUS * NODE_RADIUS * 4.0f) {
                draggedNodeIndex = i; draggedHandleType = 0; // left on node
            }
        }
        else if (is_hovered && ImGui::IsMouseClicked(1)) {
            auto dist = [](ImVec2 a, ImVec2 b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y); };
            ImVec2 mouse_p = ImGui::GetIO().MousePos;
            if (dist(mouse_p, in_screen) < HANDLE_RADIUS * HANDLE_RADIUS * 4.0f) {
                draggedNodeIndex = i; draggedHandleType = 1; // right -> in
            }
        }
        else if (is_hovered && ImGui::IsMouseClicked(2)) {
            auto dist = [](ImVec2 a, ImVec2 b) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y); };
            ImVec2 mouse_p = ImGui::GetIO().MousePos;
            if (dist(mouse_p, out_screen) < HANDLE_RADIUS * HANDLE_RADIUS * 4.0f) {
                draggedNodeIndex = i; draggedHandleType = 2; // middle -> out
            }
        }

        draw_list->AddCircleFilled(in_screen, HANDLE_RADIUS, IM_COL32(100, 200, 100, 255));
        draw_list->AddCircleFilled(out_screen, HANDLE_RADIUS, IM_COL32(200, 100, 100, 255));
        draw_list->AddCircleFilled(pos_screen, NODE_RADIUS, IM_COL32(255, 255, 255, 255));
    }

    // --- 4. ドラッグ中の座標更新 ---
    if (draggedNodeIndex >= 0 && draggedHandleType >= 0 && ImGui::IsMouseDragging(draggedHandleType)) {
        auto& node = nodes_[draggedNodeIndex];
        ImVec2 delta = ImGui::GetIO().MouseDelta;

        bool isAltDown = ImGui::IsKeyDown(ImGuiKey_LeftAlt);
        node.isBroken = isAltDown;

        if (draggedHandleType == 0) {
            node.position.x += delta.x / DISPLAY_SCALE; node.position.y += delta.y / DISPLAY_SCALE;
            node.TangentIn.x += delta.x / DISPLAY_SCALE; node.TangentIn.y += delta.y / DISPLAY_SCALE;
            node.TangentOut.x += delta.x / DISPLAY_SCALE; node.TangentOut.y += delta.y / DISPLAY_SCALE;
        }
        else if (draggedHandleType == 1) {
            node.TangentIn.x += delta.x / DISPLAY_SCALE; node.TangentIn.y += delta.y / DISPLAY_SCALE;
            if (node.isBroken) {
                node.TangentOut.x = node.position.x + (node.position.x - node.TangentIn.x);
                node.TangentOut.y = node.position.y + (node.position.y - node.TangentIn.y);
            }
        }
        else if (draggedHandleType == 2) {
            node.TangentOut.x += delta.x / DISPLAY_SCALE; node.TangentOut.y += delta.y / DISPLAY_SCALE;
            if (node.isBroken) {
                node.TangentIn.x = node.position.x + (node.position.x - node.TangentOut.x);
                node.TangentIn.y = node.position.y + (node.position.y - node.TangentOut.y);
            }
        }
    }

    // ドラッグ解除（押していたボタンが離れたら解除）
    if (draggedNodeIndex >= 0) {
        if (!ImGui::IsMouseDown(draggedHandleType)) {
            draggedNodeIndex = -1;
            draggedHandleType = -1;
        }
    }

    // 保存システム ここから↓↓↓
    char buf[128];
    strncpy_s(buf, sizeof(buf), inputNodeName_.c_str(), _TRUNCATE);
    buf[sizeof(buf) - 1] = 0; // 確実にNULL終端
    if (ImGui::InputText("Node Name", buf, sizeof(buf))) {
        inputNodeName_ = std::string(buf);
    }
    if (ImGui::Button("Save")) {
        SaveNode(inputNodeName_, nodes_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        nodes_ = MotionManager::GetInstance()->GetMotion(inputNodeName_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
		nodes_.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("ReLoad")) {
		MotionManager::GetInstance()->LoadMotions("resources/Data/Motion/Hermite/");
    }

    // 保存システム ここまで↑↑↑

    ImGui::End();
    ImGui::Begin("Hermite Positions");
    int index = 0;
    if (ImGui::TreeNodeEx("Positions")) {
        // 参照渡しで実際のノードを編集する
        for (int idx = 0; idx < (int)nodes_.size(); ++idx) {
            if (ImGui::TreeNodeEx(std::to_string(index).c_str())) {
                auto& node = nodes_[idx];
                ImGui::DragFloat3((std::string("node") + std::to_string(index)).c_str(), &node.position.x);
                ImGui::DragFloat3((std::string("in") + std::to_string(index)).c_str(), &node.TangentIn.x);
                ImGui::DragFloat3((std::string("out") + std::to_string(index)).c_str(), &node.TangentOut.x);
                if (ImGui::Button("Delete")) {
                    nodes_.erase(nodes_.begin() + idx);
                }
                index++;
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    ImGui::End();
#endif // USE_IMGUI
}

void MotionEditor::SaveNode(const std::string& filename, const std::vector<Spline::Node<Vector3>>& nodes) {
    std::string filePath = "resources/Data/Motion/Hermite/" + filename + ".json";
    json j = MathUtils::Spline::SerializeNodes(nodes);
    std::ofstream file(filePath);
    if (file.is_open()) {
        file << j.dump(4); // インデント幅を4スペースにして保存
        file.close();
    }
}