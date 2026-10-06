#include "SceneEditor.h"
#include "DrawManager.h"
#include "FileSystem.h"
#include "MathUtils.h"

void SceneEditor::Update() {
#ifdef USE_IMGUI
	auto& objects_ = targetObjects_;

	// ---------------------------------------------------
	// 1. マウスクリックによる3Dオブジェクト選択（Raycast）
	// ---------------------------------------------------
	if (ImGui::IsMouseClicked(0) && !ImGui::GetIO().WantCaptureMouse) {

		ImVec2 mousePos = ImGui::GetMousePos();

		float windowWidth = 1280.0f;
		float windowHeight = 720.0f;
		Matrix4x4 invProjView = Matrix4x4::Inverse(CameraSystem::GetInstance()->GetActiveCamera()->GetViewProjectionMatrix());
		Vector3 camPos = CameraSystem::GetInstance()->GetActiveCamera()->GetTranslation();

		// 画面座標からレイを生成
		Ray ray = MathUtils::CalculateRayFromScreen(
			mousePos.x, mousePos.y,
			windowWidth, windowHeight,
			invProjView, camPos
		);

		int closestObjIndex = -1;
		float minDistance = FLT_MAX;
		float hitDist = 0.0f;

		for (int i = 0; i < (int)objects_.size(); ++i) {

			auto* obj = objects_[i];
			if (!obj) continue;

			Vector3 objPos = obj->GetTransform().get_.Translation();

			// 球体交差判定でクリックしたオブジェクトを探す
			if (MathUtils::IntersectRaySphere(ray, objPos, selectRadius_, &hitDist)) {

				if (hitDist < minDistance) {

					minDistance = hitDist;
					closestObjIndex = i;
				}
			}
		}

		selectedObjectIndex_ = closestObjIndex; // 選択インデックスを更新
	}

	// ---------------------------------------------------
	// 選択オブジェクト・デバッグ枠の描画
	// ---------------------------------------------------
	for (int i = 0; i < (int)objects_.size(); ++i) {

		auto* obj = objects_[i];
		if (!obj) continue;

		PrimitiveSphereData data;
		data.worldTransform.Initialize();
		data.worldTransform.set_.Translation(obj->GetTransform().get_.Translation());
		data.worldTransform.set_.Scale({ 0.5f, 0.5f, 0.5f });
		data.worldTransform.LocalToWorld();

		// 選択中のオブジェクトは黄色、その他は白で強調描画
		if (i == selectedObjectIndex_) {
			data.color = { 1.0f, 1.0f, 0.0f, 1.0f };
		}
		else {
			data.color = { 1.0f, 1.0f, 1.0f, 0.3f };
		}
		DrawManager::GetInstance()->GetSphere()->AddInstance(data);
	}
#endif
}

void SceneEditor::DrawUI() {
#ifdef USE_IMGUI
	if (!sceneMap_) return;

	// UI描画前に最新のオブジェクトリストに同期
	SyncFromSceneMap();
	auto& objects_ = targetObjects_;

	// ---------------------------------------------------
	// 1. シーン全体設定 & セーブ・ロード (JSON)
	// ---------------------------------------------------
	if (ImGui::CollapsingHeader("Scene Settings & File IO", ImGuiTreeNodeFlags_DefaultOpen)) {
		// マップ名の編集
		char mapNameBuf[128];
		strncpy_s(mapNameBuf, sceneMap_->GetMapName().c_str(), sizeof(mapNameBuf));
		if (ImGui::InputText("Map Name", mapNameBuf, sizeof(mapNameBuf))) {
			sceneMap_->SetMapName(mapNameBuf);
		}

		// ---------------------------------------------------
		// セーブ・ロード処理 (FileSystemのダイアログを使用しDrawUI内で処理)
		// ---------------------------------------------------
		if (ImGui::Button("Save Scene...")) {
			std::string savePath = FileSystem::ShowSaveFileDialogJson();
			if (!savePath.empty()) {
				sceneMap_->SaveToFile(savePath);
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Load Scene...")) {
			std::string loadPath = FileSystem::ShowOpenFileDialogJson();
			if (!loadPath.empty()) {
				if (sceneMap_->LoadFromFile(loadPath, nullptr)) {
					selectedObjectIndex_ = -1;
					SyncFromSceneMap();
				}
			}
		}
	}

	ImGui::Separator();

	// ---------------------------------------------------
	// 2. 編集操作 & ギズモモード切替
	// ---------------------------------------------------
	if (ImGui::Button("Add Object 追加")) {
		ExecuteCommand(std::make_unique<SceneObjectAddCommand>(sceneMap_, "New Object"));
		SyncFromSceneMap();
	}

	ImGui::SameLine();
	if (ImGui::Button("Undo")) { Undo(); SyncFromSceneMap(); }
	ImGui::SameLine();
	if (ImGui::Button("Redo")) { Redo(); SyncFromSceneMap(); }

	ImGui::RadioButton("Translate", &gizmoOperation_, 0); ImGui::SameLine();
	ImGui::RadioButton("Rotate", &gizmoOperation_, 1); ImGui::SameLine();
	ImGui::RadioButton("Scale", &gizmoOperation_, 2);

	ImGui::Separator();

	// ---------------------------------------------------
	// 3. オブジェクト一覧（Hierarchy）と Transform / Component 編集
	// ---------------------------------------------------
	for (int i = 0; i < (int)objects_.size(); ++i) {
		ImGui::PushID(i);

		// オブジェクト削除ボタン
		if (ImGui::Button("X")) {
			ExecuteCommand(std::make_unique<SceneObjectDeleteCommand>(sceneMap_, objects_[i]));
			if (selectedObjectIndex_ == i) selectedObjectIndex_ = -1;
			ImGui::PopID();
			break;
		}
		ImGui::SameLine();

		std::string nodeName = objects_[i]->GetName() + " ##" + std::to_string(i);
		if (ImGui::TreeNode(nodeName.c_str())) {

			if (ImGui::Selectable("Select Object", selectedObjectIndex_ == i)) {
				selectedObjectIndex_ = i;
			}

			// --- Transform 編集 ---
			TransformState oldState{
				objects_[i]->GetTransform().get_.Translation(),
				objects_[i]->GetTransform().get_.Rotation(),
				objects_[i]->GetTransform().get_.Scale()
			};
			bool isEdited = false;

			Vector3 pos = oldState.position;
			Vector3 rot = oldState.rotation;
			Vector3 scale = oldState.scale;

			if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) { objects_[i]->SetPosition(pos); }
			if (ImGui::IsItemDeactivatedAfterEdit()) isEdited = true;

			if (ImGui::DragFloat3("Rotation", &rot.x, 0.1f)) { objects_[i]->SetRotation(rot); }
			if (ImGui::IsItemDeactivatedAfterEdit()) isEdited = true;

			if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) { objects_[i]->SetScale(scale); }
			if (ImGui::IsItemDeactivatedAfterEdit()) isEdited = true;

			// ドラッグ終了時に Undo コマンド発行
			if (isEdited) {
				TransformState newState{
					objects_[i]->GetTransform().get_.Translation(),
					objects_[i]->GetTransform().get_.Rotation(),
					objects_[i]->GetTransform().get_.Scale()
				};
				objects_[i]->SetPosition(oldState.position);
				objects_[i]->SetRotation(oldState.rotation);
				objects_[i]->SetScale(oldState.scale);

				ExecuteCommand(std::make_unique<DynamicObjectTransformCommand>(objects_[i], oldState, newState));
			}

			// --- Component 管理 ---
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Attached Components:");

			const auto& components = objects_[i]->GetComponents();
			for (size_t c = 0; c < components.size(); ++c) {
				Component* comp = components[c].get();
				if (!comp) continue;

				ImGui::PushID(static_cast<int>(c));

				// GetComponentName() を使用してコンポーネント名を表示
				std::string compName = comp->GetComponentName();

				if (ImGui::TreeNode(compName.c_str())) {
					comp->DrawUI();
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			ImGui::Spacing();

			// Component 追加ポップアップ
			if (ImGui::Button("Add Component +")) {
				ImGui::OpenPopup("AddComponentPopup");
			}

			if (ImGui::BeginPopup("AddComponentPopup")) {
				auto registeredNames = ComponentFactory::GetInstance()->GetRegisteredNames();
				for (const auto& compName : registeredNames) {
					if (ImGui::Selectable(compName.c_str())) {
						ExecuteCommand(std::make_unique<AddComponentCommand>(objects_[i], compName));
					}
				}
				ImGui::EndPopup();
			}

			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// ---------------------------------------------------
	// 4. ImGuizmo 操作
	// ---------------------------------------------------
	if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= (int)objects_.size()) return;

	auto camera = CameraSystem::GetInstance()->GetActiveCamera();
	auto viewMat = camera->GetViewMatrix();
	auto projMat = camera->GetProjectionMatrix();

	float viewMatrix[16], projectionMatrix[16];
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			viewMatrix[r * 4 + c] = viewMat.m[r][c];
			projectionMatrix[r * 4 + c] = projMat.m[r][c];
		}
	}

	DynamicObject* selectedObj = objects_[selectedObjectIndex_];
	Matrix4x4 worldMat = selectedObj->GetTransform().mat_;

	float gizmoMatrix[16];
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			gizmoMatrix[r * 4 + c] = worldMat.m[r][c];
		}
	}

	ImGuizmo::BeginFrame();
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

	ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
	if (gizmoOperation_ == 1) op = ImGuizmo::ROTATE;
	if (gizmoOperation_ == 2) op = ImGuizmo::SCALE;

	ImGuizmo::Manipulate(viewMatrix, projectionMatrix, op, ImGuizmo::WORLD, gizmoMatrix);

	if (ImGuizmo::IsUsing()) {
		if (!isGizmoUsingLastFrame_) {
			gizmoOldState_ = {
				selectedObj->GetTransform().get_.Translation(),
				selectedObj->GetTransform().get_.Rotation(),
				selectedObj->GetTransform().get_.Scale()
			};
		}

		Vector3 translation, rotation, scale;
		ImGuizmo::DecomposeMatrixToComponents(gizmoMatrix, &translation.x, &rotation.x, &scale.x);

		selectedObj->SetPosition(translation);
		selectedObj->SetRotation(rotation);
		selectedObj->SetScale(scale);

		isGizmoUsingLastFrame_ = true;
	}
	else {
		if (isGizmoUsingLastFrame_) {
			TransformState gizmoNewState = {
				selectedObj->GetTransform().get_.Translation(),
				selectedObj->GetTransform().get_.Rotation(),
				selectedObj->GetTransform().get_.Scale()
			};

			selectedObj->SetPosition(gizmoOldState_.position);
			selectedObj->SetRotation(gizmoOldState_.rotation);
			selectedObj->SetScale(gizmoOldState_.scale);

			ExecuteCommand(std::make_unique<DynamicObjectTransformCommand>(selectedObj, gizmoOldState_, gizmoNewState));

			isGizmoUsingLastFrame_ = false;
		}
	}
#endif
}