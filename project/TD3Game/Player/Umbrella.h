#pragma once
#include "ModelObject.h"
#include "UmbrellaState.h"
#include "Collider.h"

import Attachment;
import StatusComponent;
import ManaComponent;

//////////////////////
/// 
///  傘の形状・機能
/// 
//////////////////////
enum class UmbrellaForm {
    Closed,     // 閉じている（攻撃特化）
    Opened,     // 開いている（防御・マナ回収特化）
    Reverse,    // 逆さ（防御・壊れやすい）
	Flying,	   // かさが飛んでいる(ない)状態（壊れている状態など）
};

//////////////////////
/// 
///  傘
/// 
//////////////////////

namespace Umbrella {
   
    //////////////////////
    /// 
    ///  傘の[持ち手]
    /// 
    //////////////////////

    class Handle {

        //////////////////////
        /// 
        ///  コンストラクタ・デストラクタ
        /// 
        //////////////////////
    public:
        // コンストラクタ
        Handle() = default;
        // デストラクタ
        ~Handle() = default;

        //////////////////////
        /// 
        ///  基本的な関数
        /// 
        //////////////////////
    public:
        // 初期化処理
        void Initialize(Fngine* f);
        // 更新処理
        void Update(float deltaTime);
        // 描画処理
        void Draw();

        //////////////////////
        /// 
        ///  Joint関係
        /// 
        //////////////////////
    public:
        Attachment* GetTipJoint() { return &tipJoint_; }
        Attachment* GetBaseJoint() { return &baseJoint_; }

    private:
        // プレイヤーに持たれる用のJoint
        Attachment baseJoint_;

        // 「かさ」をくっつけるための先端のJoint(他にもおｋでいいかも)
        Attachment tipJoint_;

        std::unique_ptr<ModelObject>obj_;
    public:  Fngine* p_fngine_;
    };

    //////////////////////
    /// 
    ///  傘の[かさ]
    /// 
    //////////////////////
    class Top {
        //////////////////////
        /// 
        ///  コンストラクタ・デストラクタ
        /// 
        //////////////////////
    public:
        // コンストラクタ
        Top() = default;
        // デストラクタ
        ~Top() = default;

        //////////////////////
        /// 
        ///  基本的な関数
        /// 
        //////////////////////
    public:
        // 初期化処理
        void Initialize(Fngine* f);
        // 更新処理
        void Update(float deltaTime);
        // 描画処理
        void Draw();

        //////////////////////
        /// 
        ///  Joint関係
        /// 
        //////////////////////
    public:
        Attachment* GetRootJoint() { return &rootJoint_; }

    private:
        // 持ち手、壁、敵にくっつくためのJoint
        Attachment rootJoint_;

        //////////////////////
        /// 
        ///  当たり判定
        /// 
        //////////////////////
    public:
        void UpdateColliderShape();
        ConvexCollider* GetCollider() const { return collider_.get(); }
        // 攻撃判定のON/OFF（属性の切り替え）
        void EnableAttackCollision() { collider_->SetMyType(COL_Player_Attack); }
        void DisableAttackCollision() { collider_->SetMyType(COL_None); }
    private:
        std::unique_ptr<ConvexCollider>collider_;

        //////////////////////
        /// 
        ///  パラメータ
        /// 
        //////////////////////
    public:
        // 壊れているかどうかの判定（HPが0以下なら壊れている）
        bool IsBroken() const { return status_->IsDead(); }
        void Repair() {
            // MaxHP分の回復値を渡すことで全回復させる(だんだんはHealを呼び出す)
            status_->Heal(status_->GetMaxHp());
        }
    private:
        std::unique_ptr<StatusComponent>status_;// 耐久度・攻撃力
        std::unique_ptr<ManaComponent>mana_;// マナ回収用

    public:
        StatusComponent& GetStatusComponent() { return *status_; }
        ManaComponent& GetManaComponent() { return *mana_; }

        //////////////////////
        /// 
        ///  State
        /// 
        //////////////////////
    public:
        void ChangeForm(UmbrellaForm newForm) { form_ = newForm; }
        void ChangeState(UmbrellaStates::Base* newState);
    private:
        // 状態のState
        UmbrellaForm form_ = UmbrellaForm::Closed;
        bool isBroken_ = false;
        
        // 現在のステート
        UmbrellaStates::Base* currentState_;
        //// ステート達
        //std::unique_ptr<UmbrellaStates::Close>closeState_;
        //std::unique_ptr<UmbrellaStates::Open>openState_;
        //std::unique_ptr<UmbrellaStates::Reverse>reverseState_;

    public:// Get・Set関係の関数
        // 傘の「かさ」の状態を返す関数
        UmbrellaForm GetUmbrellaForm()const { return form_; }

        //////////////////////
        /// 
        ///  基本的な情報
        /// 
        //////////////////////
    private:
        std::unique_ptr<ModelObject>obj_;
        std::unique_ptr<ModelObject>openObj_;
    public:  Fngine* p_fngine_;
    };

    //////////////////////
    /// 
    ///  傘の[全部]
    /// 
    //////////////////////
    class Main {

        //////////////////////
        /// 
        ///  コンストラクタ・デストラクタ
        /// 
        //////////////////////
    public:
        // コンストラクタ
        Main() = default;
        // デストラクタ
        ~Main() = default;

        //////////////////////
        ///
        ///  基本的な関数
        ///
        //////////////////////
    public:
        // 初期化処理
        void Initialize(Fngine* f);
        // 更新処理
        void Update(float deltaTime);
        // 描画処理
        void Draw();

    public:

        //////////////////////
        ///
        ///  基本的な情報
        ///
        //////////////////////
    public:

    public:
        std::unique_ptr<Top>top_;
        std::unique_ptr<Handle>handle_;
    };
}