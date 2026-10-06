#include "PipelineStateObjectManager.h"
#include "ImGuiManager.h"

#include <filesystem>

#include <json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

std::unique_ptr<PipelineStateObjectManager> PipelineStateObjectManager::instance_ = nullptr;

void PipelineStateObjectManager::Initialize(Fngine* fngine) { 
	p_fngine_ = fngine;
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

//////////////////////////////////////////////////////////.
///
/// ここからJsonファイルからデータを読み込む際に使う関数群
///
//////////////////////////////////////////////////////////

//   =====================================
// 【 RootSignatureの何で使用するかの設定 】
//   =====================================
D3D12_SHADER_VISIBILITY ParseVisibility(const std::string& str) {
    if (str == "Vertex") return D3D12_SHADER_VISIBILITY_VERTEX;
    if (str == "Pixel") return D3D12_SHADER_VISIBILITY_PIXEL;
    return D3D12_SHADER_VISIBILITY_ALL;
}

//   ===================================
// 【 最初のBlendModeを何にするかの設定 】
//   ===================================
BLENDMODE ParseBlendMode(const std::string& str) {
    if (str == "Add") return BLENDMODE::Additive;
    else if (str == "None") return BLENDMODE::None;
    else if (str == "Multiply") return BLENDMODE::Multiplicative;
    else if (str == "Screen") return BLENDMODE::ScreenBlend;
    else if (str == "Subtract") return BLENDMODE::Subtractive;
    return BLENDMODE::AlphaBlend;
}

//   =======================================
// 【 PrimitiveTopologyTypeで使用するの設定 】
//   =======================================
D3D12_PRIMITIVE_TOPOLOGY_TYPE ParseTopologyType(const std::string& str) {
    if (str == "Point")    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    if (str == "Line")     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    if (str == "Patch")    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // デフォルト
}

//   ========================================
// 【 DXGIフォーマットの変換 (InputLayout用) 】
//   ========================================
DXGI_FORMAT ParseFormat(const std::string& str) {
    if (str == "R8G8B8A8_UNORM")      return DXGI_FORMAT_R8G8B8A8_UNORM;
    if (str == "R8G8B8A8_UNORM_SRGB") return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (str == "R16G16B16A16_FLOAT")  return DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (str == "R32G32B32A32_FLOAT")  return DXGI_FORMAT_R32G32B32A32_FLOAT;
    if (str == "R32G32B32_FLOAT")     return DXGI_FORMAT_R32G32B32_FLOAT;
    if (str == "R32G32_FLOAT")        return DXGI_FORMAT_R32G32_FLOAT;
    if (str == "R32_FLOAT")           return DXGI_FORMAT_R32_FLOAT;
    if (str == "R32G32B32A32_SINT")   return DXGI_FORMAT_R32G32B32A32_SINT;
    if (str == "R10G10B10A2_UNORM")   return DXGI_FORMAT_R10G10B10A2_UNORM;
    if (str == "R11G11B10_FLOAT")     return DXGI_FORMAT_R11G11B10_FLOAT;
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
}

//   ===================
// 【ラスタライザの設定 】
//   ===================
RasterizerSettings ParseRasterizer(const json& j) {
    RasterizerSettings settings{};
    if (j.contains("cullMode")) {
        std::string mode = j["cullMode"];
        if (mode == "BACK")  settings.CullMode = D3D12_CULL_MODE_BACK;
        if (mode == "FRONT") settings.CullMode = D3D12_CULL_MODE_FRONT;
        if (mode == "NONE")  settings.CullMode = D3D12_CULL_MODE_NONE;
    }
    if (j.contains("fillMode")) {
        settings.FillMode = (j["fillMode"] == "WIREFRAME") ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    }
    return settings;
}

// 深度ステンシル設定の変換
D3D12_DEPTH_WRITE_MASK ParseDepthWriteMask(const std::string& str) {
    if (str == "Zero") return D3D12_DEPTH_WRITE_MASK_ZERO;
    return D3D12_DEPTH_WRITE_MASK_ALL;
}

D3D12_COMPARISON_FUNC ParseComparisonFunc(const std::string& str) {
    if (str == "Never")        return D3D12_COMPARISON_FUNC_NEVER;
    if (str == "Less")         return D3D12_COMPARISON_FUNC_LESS;
    if (str == "Equal")        return D3D12_COMPARISON_FUNC_EQUAL;
    if (str == "LessEqual")    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    if (str == "Greater")      return D3D12_COMPARISON_FUNC_GREATER;
    if (str == "NotEqual")     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    if (str == "GreaterEqual") return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    return D3D12_COMPARISON_FUNC_ALWAYS;
}

// テクスチャアドレスモードの変換
D3D12_TEXTURE_ADDRESS_MODE ParseAddressMode(const std::string& str) {
    if (str == "Wrap")   return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    if (str == "Clamp")  return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    if (str == "Mirror") return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    if (str == "Border") return D3D12_TEXTURE_ADDRESS_MODE_BORDER;

    // 省略時やスペルミス時は一番安全な Wrap にしておく
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}

D3D12_FILTER ParseFilter(const std::string& str) {
    if (str == "Point") return D3D12_FILTER_MIN_MAG_MIP_POINT;
    if (str == "Linear") return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    if (str == "Anisotropic") return D3D12_FILTER_ANISOTROPIC;
    return D3D12_FILTER_MIN_MAG_MIP_LINEAR; // デフォルトはリニア
}

// 指定したフォルダ内のすべてのJSONファイルを読み込む
void PipelineStateObjectManager::LoadAllPSOsFromDirectory(const std::string& directoryPath) {
    // フォルダが存在するかチェック
    if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
        Log::ViewFile("Directory not found: " + directoryPath);
        return;
    }

    // フォルダ内のファイルを1つずつ巡回
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        // 通常のファイル、かつ拡張子が「.json」のものを探す
        if (entry.is_regular_file() && entry.path().extension() == ".json") {

            std::string filePath = entry.path().string();               // フルパス (例: "Resources/PSO/SpritePSO.json")
            std::string psoName = entry.path().stem().string();       // ファイル名のみ (例: "SpritePSO")

            // ファイル名を設定名として、1ファイルずつ読み込む！
            LoadPSOsFromJson(filePath, psoName);
        }
    }
}

void PipelineStateObjectManager::LoadPSOsFromJson(const std::string& filepath, const std::string& psoName) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Log::ViewFile("Failed to open PSO json: " + filepath);
        return;
    }

    json psoJson;
    file >> psoJson;

    std::string type = psoJson["pipelineType"]; // "Graphics" or "Compute"

    // ビルダーを起動！
    PSOBuilder builder(p_fngine_, psoName);
    builder.SetPipelineType(type);

    // --- 1. シェーダーのセット ---
    if (type == "Graphics") {
        //   ============================
        // 【 シェーダーのPath情報を取得 】
        //   ============================
        // VSのPathを取得
        std::string vsRaw = psoJson["shaders"].value("vs", "");
        std::wstring vsPath = ConvertString(vsRaw);
        // PSのPathを取得
        std::string psRaw = psoJson["shaders"].value("ps", "");
        std::wstring psPath = ConvertString(psRaw);
        // データを転送
        builder.SetShaders(vsPath, psPath);
        // ※ あとはProfileをどうするか

        //   ================
        // 【 Topologyの設定 】
        //   ================
        builder.SetTopologyType(ParseTopologyType(psoJson.value("primitiveTopologyType", "Triangle")));

        //   ===================
        // 【 InputLayoutの設定 】
        //   ===================
        static std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        static std::vector<std::string> semanticNamePool; // 寿命を関数末尾まで引き伸ばすため

        inputElements.clear();
        semanticNamePool.clear();

        if (psoJson.contains("inputLayout")) {
            // vectorの再確保によるアドレス移動を防ぐため事前にリザーブ
            semanticNamePool.reserve(psoJson["inputLayout"].size());

            for (const auto& element : psoJson["inputLayout"]) {
                // プールに実体をコピーして保管 <=  ...?
                semanticNamePool.push_back(element["semanticName"].get<std::string>());

                D3D12_INPUT_ELEMENT_DESC elDesc{};
                // プールに保管された安全な文字列ポインタを渡す！
                elDesc.SemanticName = semanticNamePool.back().c_str();
                elDesc.SemanticIndex = element.value("semanticIndex", 0);
                elDesc.Format = ParseFormat(element["format"]);
                elDesc.InputSlot = element.value("inputSlot", 0);
                elDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
                elDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

                inputElements.push_back(elDesc);
            }
            builder.SetInputLayout(inputElements.data(), static_cast<UINT>(inputElements.size()));
        }

        // Blend, Cull...
        builder.SetBlendMode(ParseBlendMode(psoJson.value("blendMode", "AlphaBlend")));

        builder.SetRasterizerSettings(ParseRasterizer(psoJson["rasterizer"]));

        DepthSettings depthSettings;
        if (psoJson.contains("depthStencilState")) {
            const auto& ds = psoJson["depthStencilState"];
            if (ds.contains("depthEnable"))    depthSettings.depthEnable = ds["depthEnable"];
            if (ds.contains("depthWriteMask")) depthSettings.writeMask = ParseDepthWriteMask(ds["depthWriteMask"]);
            if (ds.contains("depthFunc"))      depthSettings.comparisonFunc = ParseComparisonFunc(ds["depthFunc"]);
            if (ds.contains("format"))         depthSettings.formats = ParseFormat(ds["format"]);
        }
        builder.SetDepthSettings(depthSettings);

        RenderTargetSettings rtSettings{};
        if (psoJson.contains("rtvFormats") && psoJson["rtvFormats"].is_array()) {
            const auto& rtvArray = psoJson["rtvFormats"];
            rtSettings.numRenderTargets = static_cast<UINT>(std::min<size_t>(rtvArray.size(), 8));
            for (UINT i = 0; i < rtSettings.numRenderTargets; ++i) {
				rtSettings.rtvFormats[i] = ParseFormat(rtvArray[i].get<std::string>());
            }
        }
        else if (psoJson.contains("rtvFormat")) { // 単一指定フォーマットにも対応
            rtSettings.numRenderTargets = 1;
            rtSettings.rtvFormats[0] = ParseFormat(psoJson["rtvFormat"].get<std::string>());
        }
        builder.SetRenderTargetSettings(rtSettings);
    }
    else if (type == "Compute") {
        // CSの設定 (Builder側に SetComputeShader 等を追加しておく)
        std::string csRaw = psoJson["shaders"]["cs"];
        std::wstring cs = ConvertString(csRaw);
        builder.SetComputeShader(cs);
    }

    // RootSignatureの組み立て
    if (psoJson.contains("rootSignature")) {
        for (const auto& param : psoJson["rootSignature"]) {
            std::string paramType = param["type"];
            UINT reg = param["register"];
            D3D12_SHADER_VISIBILITY vis = ParseVisibility(param["visibility"]);

            if (paramType == "CBV") {
                builder.AddCBV(reg, vis);
            }
            else if (paramType == "SRVTable") {
                builder.AddSRVTable(reg, param["count"], vis);
            }
            else if (paramType == "UAVTable") {
                builder.AddUAVTable(reg, param["count"], vis);
            }
            else if (paramType == "Constants") {
                // 定数の数（count）と、レジスタ番号（register）を渡す
                builder.AddConstants(param["count"], reg, vis);
            }
        }
    }

    // サンプラーの処理
    if (psoJson.contains("staticSamplers")) {
        for (const auto& samp : psoJson["staticSamplers"]) {
            D3D12_TEXTURE_ADDRESS_MODE u = ParseAddressMode(samp.value("addressU", "Wrap"));
            D3D12_TEXTURE_ADDRESS_MODE v = ParseAddressMode(samp.value("addressV", "Wrap"));
            D3D12_TEXTURE_ADDRESS_MODE w = ParseAddressMode(samp.value("addressW", "Wrap"));
			D3D12_FILTER filter = ParseFilter(samp.value("filter", "Linear"));
            D3D12_SHADER_VISIBILITY vis = ParseVisibility(samp.value("visibility", "All"));
            builder.AddStaticSampler(samp["register"], filter, u, v, w,vis);
        }
    }

    // --- 3. 構築してマネージャーに登録！ ---
    builder.Build();
}