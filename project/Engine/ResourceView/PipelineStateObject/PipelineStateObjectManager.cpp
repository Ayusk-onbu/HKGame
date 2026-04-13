#include "PipelineStateObjectManager.h"
#include "ImGuiManager.h"

std::unique_ptr<PipelineStateObjectManager> PipelineStateObjectManager::instance_ = nullptr;

void PipelineStateObjectManager::Initialize(Fngine* fngine) { 
	p_fngine_ = fngine;
}

void PipelineStateObjectManager::CreateNewPSO(const PSOKey& key,const std::string& name) {
	PSO newPSO;
    newPSO.Initialize(p_fngine_, key);
	PSOs_.emplace(name, std::move(newPSO));
}

PSO& PipelineStateObjectManager::GetPSO(const std::string& name) {
	auto it = PSOs_.find(name);
	if (it == PSOs_.end()) {
		Log::ViewFile("Not find your want File");
	}
	return it->second;
}

void PipelineStateObjectManager::ImGui() {
#ifdef USE_IMGUI
    ImGui::Begin("PSO Manager"); // メインウィンドウの開始

    // Tab Bar の開始: この中で各タブを定義します。
    if (ImGui::BeginTabBar("PSOTabBar", ImGuiTabBarFlags_None))
    {
        // PSOs_ リストをループして、各要素（PSO名とオブジェクト）に対してタブを作成
        for (const auto& pso : PSOs_)
        {
            // pso.first (PSOの名前) をタブのタイトルとして使用
            // BeginTabItem() は、そのタブが現在アクティブ（選択されている）場合に true を返します。
            if (ImGui::BeginTabItem(pso.first.c_str()))
            {
                // --- ここからが、選択されたタブの中身です ---

                // ブレンド設定をグループ化するために BeginChild や Group を使用しても良いですが、
                // ここではシンプルなボタン配置で PSO ごとに設定できるようにします。

                ImGui::Text("Current PSO: %s", pso.first.c_str());
                ImGui::Separator();

                // ブレンド設定ボタンの定義 (Horizontal Layout)

                if (ImGui::Button("Alpha")) {
                    PSOManager::GetInstance()->GetPSO(pso.first.c_str()).SetBlendState(BLENDMODE::AlphaBlend);
                }
                ImGui::SameLine(); // 次のボタンを同じ行に配置

                if (ImGui::Button("None")) {
                    PSOManager::GetInstance()->GetPSO(pso.first.c_str()).SetBlendState(BLENDMODE::None);
                }
                ImGui::SameLine(); // 次のボタンを同じ行に配置

                if (ImGui::Button("Add")) {
                    PSOManager::GetInstance()->GetPSO(pso.first.c_str()).SetBlendState(BLENDMODE::Additive);
                }
                ImGui::SameLine();

                if (ImGui::Button("Sub")) {
                    PSOManager::GetInstance()->GetPSO(pso.first.c_str()).SetBlendState(BLENDMODE::Subtractive);
                }
                ImGui::SameLine();

                if (ImGui::Button("Mul")) {
                    PSOManager::GetInstance()->GetPSO(pso.first.c_str()).SetBlendState(BLENDMODE::Multiplicative);
                }
                ImGui::SameLine();

                if (ImGui::Button("Screen")) {
                    PSOManager::GetInstance()->GetPSO(pso.first.c_str()).SetBlendState(BLENDMODE::ScreenBlend);
                }

                // --- タブの中身の終了 ---

                // Tab Item の終了
                ImGui::EndTabItem();
            }
        }

        // Tab Bar の終了
        ImGui::EndTabBar();
    }

    ImGui::End(); // メインウィンドウの終了
#endif // _DEBUG
}