#pragma once
#include <wrl.h>
#include "D3D12System.h"
#include "DescriptorHeap.h"
#include "ShaderResourceView.h"
#include "DepthStencil.h"
#include "PipelineStateObject.h"
#include "TheOrderCommand.h"

struct RenderTargetSlotDesc {
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // RTVをクリアするかどうか
    bool doClear = true;
    // クリアする際のカラー
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    RenderTargetSlotDesc() = default;
    RenderTargetSlotDesc(DXGI_FORMAT fmt, bool clear = true, const float color[4] = nullptr)
        : format(fmt), doClear(clear) {
        if (color) {
            for (int i = 0; i < 4; ++i) clearColor[i] = color[i];
        }
    }
};

enum class RenderTargetType {
    Albedo = 0,
    SSS,// Skin用SSS

    Count,
};

class RenderTargetGroup {
public:
    RenderTargetGroup() = default;
    ~RenderTargetGroup() = default;

    RenderTargetGroup(const RenderTargetGroup&) = delete;
    RenderTargetGroup& operator=(const RenderTargetGroup&) = delete;
    RenderTargetGroup(RenderTargetGroup&&) noexcept = default;
    RenderTargetGroup& operator=(RenderTargetGroup&&) noexcept = default;

public:
    /**
     * @brief 初期化（詳細設定版）
     * @param slots 各スロットのクリア設定やフォーマットの配列
     * @param isDoubleBuffer ダブルバッファ（Ping-Pong）を有効にするか
     */
    void Initialize(
        D3D12System& d3d12,
        float width,
        float height,
        const std::vector<RenderTargetSlotDesc>& slots,
        bool isDoubleBuffer = false,
        bool useDSV = true
    );

    /**
     * @brief 初期化（簡易フォーマット配列版：全スロットデフォルトでクリア有効）
     */
    void Initialize(
        D3D12System& d3d12,
        float width,
        float height,
        const std::vector<DXGI_FORMAT>& formats,
        bool isDoubleBuffer = false,
		bool useDSV = true
    );

    /**
     * @brief 描画開始処理
     * @param clearDepth 深度バッファをクリアするか（デフォルト: true）
     */
    void Begin(TheOrderCommand& command, bool clearDepth = true);

    /**
     * @brief 描画終了処理
     * @param autoSwap ダブルバッファ時、終了時に自動で SwapBuffers を呼ぶか
     */
    void End(TheOrderCommand& command, bool autoSwap = false);

    /**
     * @brief ダブルバッファの表裏（Write / Read）を入れ替える
     */
    void SwapBuffers();

    /**
     * @brief 全スロット（または指定スロット）のクリアを実行する
     */
    void Clear(TheOrderCommand& command, bool clearDepth = true);
    void ClearSlot(TheOrderCommand& command, size_t index);

    // --- 個別クリア設定の動的変更 ---
    void SetSlotClearEnabled(size_t index, bool enable);
    void SetSlotClearColor(size_t index, const float color[4]);

    // --- アクセサ（数値インデックス版） ---
    // 現在の書き込み対象 / 最新バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> GetResource(size_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(size_t index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUHandle(size_t index) const;

    // 前フレームの参照用バッファ（ダブルバッファ時専用）
    Microsoft::WRL::ComPtr<ID3D12Resource> GetPrevResource(size_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetPrevSRVGPUHandle(size_t index) const;

    // --- アクセサ（enum class 対応テンプレート版） ---
    template <typename EnumType>
    Microsoft::WRL::ComPtr<ID3D12Resource> GetResource(EnumType type) const {
        return GetResource(static_cast<size_t>(type));
    }

    template <typename EnumType>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(EnumType type) const {
        return GetSRVGPUHandle(static_cast<size_t>(type));
    }

    template <typename EnumType>
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUHandle(EnumType type) const {
        return GetRTVCPUHandle(static_cast<size_t>(type));
    }

    template <typename EnumType>
    Microsoft::WRL::ComPtr<ID3D12Resource> GetPrevResource(EnumType type) const {
        return GetPrevResource(static_cast<size_t>(type));
    }

    template <typename EnumType>
    D3D12_GPU_DESCRIPTOR_HANDLE GetPrevSRVGPUHandle(EnumType type) const {
        return GetPrevSRVGPUHandle(static_cast<size_t>(type));
    }

    // --- DSV 関連アクセサ ---
    Microsoft::WRL::ComPtr<ID3D12Resource> GetDSVResource() { return dsv_.GetResource(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetDSVDepthHandleGPU() const { return dsv_.GetSRVHandleGPU(); }
    void ChangeDSVHandleType(TheOrderCommand& command, DSV_HANDLE_TYPE type);

    // --- 情報アクセサ ---
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    size_t GetRenderTargetCount() const { return slotDescs_.size(); }
    bool IsDoubleBuffered() const { return isDoubleBuffer_; }
    uint32_t GetCurrentBufferIndex() const { return currentBufferIndex_; }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    bool isDoubleBuffer_ = false;
	bool useDSV_ = true;
    uint32_t currentBufferIndex_ = 0; // 0 または 1

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    std::vector<RenderTargetSlotDesc> slotDescs_;

    // RTV用ヒープ（ダブルバッファ時は2倍のサイズを確保）
    DescriptorHeap rtvHeap_;

    // SoA構造
    // [0] = バッファA用, [1] = バッファB用（シングルバッファの場合は[0]のみ使用）
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> renderTargets_[2];
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles_[2];
    std::vector<SRVAllocation> srvAllocations_[2];

    DSV dsv_;
};

//class OffRTV
//{
//public:
//
//	void Initialize(D3D12System* d3d12, Microsoft::WRL::ComPtr <ID3D12Resource> resource,DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D);
//
//	void MakeDesc(DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D);
//
//	void MakeHandle(Microsoft::WRL::ComPtr <ID3D12Device> device, Microsoft::WRL::ComPtr <ID3D12Resource> resource);
//
//	D3D12_CPU_DESCRIPTOR_HANDLE& GetHandle() { return handle_; }
//
//private:
//	DescriptorHeap descriptorHeap_;
//	D3D12_RENDER_TARGET_VIEW_DESC desc_;// ローカルでよくね？何で変数？
//	D3D12_CPU_DESCRIPTOR_HANDLE startHandle_;// ←お前存在価値無くて草
//	D3D12_CPU_DESCRIPTOR_HANDLE handle_;
//
//};
//
//class OffScreenRendering
//{
//public:
//
//	void Initialize(D3D12System& d3d12, float width,float height,
//		DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 
//		D3D12_RTV_DIMENSION dimension = D3D12_RTV_DIMENSION_TEXTURE2D
//		);
//	void Begin(TheOrderCommand &command);
//	void End(TheOrderCommand& command);
//	D3D12_GPU_DESCRIPTOR_HANDLE& GetHandleGPU() { return srvAllocation_.gpu; }
//	Microsoft::WRL::ComPtr <ID3D12Resource> GetResource() {return offScreenTexture_;}
//	void ChangeDSVHandleType(TheOrderCommand& command,DSV_HANDLE_TYPE type);
//
//	Microsoft::WRL::ComPtr <ID3D12Resource>GetDSVResource() { return dsv_.GetResource(); }
//	D3D12_GPU_DESCRIPTOR_HANDLE GetDSVDepthHandleGPU() { return dsv_.GetSRVHandleGPU(); }
//private:
//	
//private:
//	Microsoft::WRL::ComPtr <ID3D12Resource> offScreenTexture_;
//	OffRTV offRTV_;
//	DSV dsv_;
//	PSO pso_;
//	SRVAllocation srvAllocation_;
//};