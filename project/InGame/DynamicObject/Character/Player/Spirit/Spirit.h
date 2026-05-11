#pragma once
#include "../../../Magic/Magic.h"
#include <unordered_map>
#include <vector>

struct SpiritData {
    int spiritId;             // 霊の識別ID
    float damageMultiplier;   // ダメージ倍数
    // ... その他のステータス ...

    // 霊が持っている8個の魔法をマップで管理！
    // 例: キーは「ボタン番号(1~8)」や「魔法スロット」
    std::unordered_map<int, MagicData> equippedMagics;
};

class SpiritManager {
private:
    std::vector<SpiritData> unlockedSpirits_; // 所持している霊のリスト
    int currentSpiritIndex_ = 0;              // 現在装備中の霊

public:
    // 霊の切り替え
    void SwitchNext() {
        // ... 次のインデックスへ（クールタイムチェック等もここでやる）
    }

    // 現在の霊のデータを取得
    const SpiritData& GetCurrentSpirit() const {
        return unlockedSpirits_[currentSpiritIndex_];
    }

    // 今の霊にセットされた指定スロット(ボタン)の魔法データを取得
    const MagicData* GetMagicData(int slotId) const {
        const auto& currentSpirit = GetCurrentSpirit();
        auto it = currentSpirit.equippedMagics.find(slotId);
        if (it != currentSpirit.equippedMagics.end()) {
            return &(it->second);
        }
        return nullptr; // スロットが空の場合
    }
};