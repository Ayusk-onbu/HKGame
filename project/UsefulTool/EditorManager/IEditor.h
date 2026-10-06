#pragma once
#include "../EventManager/ICommand.h"
#include "FileSystem.h"
#include <vector>
#include <memory>
#include <string>

class IEditor {
public:
    virtual ~IEditor() = default;
    virtual void Update() = 0;
    virtual void DrawUI() = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    // UI表示用の名前を取得する関数
    virtual const char* GetName() const = 0;
};

template <typename T>
class BaseEditor : public IEditor {
public:
    BaseEditor(const std::string& name) : editorName_(name) {}
protected:
    std::string editorName_; // エディタの名前

    std::vector<T*> targetObjects_; // 制御する物体のアドレスリスト

    std::vector<std::unique_ptr<ICommand>> commandHistory_;
    size_t currentCommandIndex_ = 0; // Undo/Redo用の現在位置
public:
    // 制御する物体をセット
    void SetTargetObjects(const std::vector<T*>& targets) {
        targetObjects_ = targets;
    }

    const std::vector<T*>& GetTargetObject()const {
        return targetObjects_;
    }

    // コマンドを実行し、履歴に積む
    void ExecuteCommand(std::unique_ptr<ICommand> command) {
        // 過去に戻っている状態で新しいコマンドを実行した場合、それ以降のRedo履歴を破棄する
        if (currentCommandIndex_ < commandHistory_.size()) {
            commandHistory_.erase(commandHistory_.begin() + currentCommandIndex_, commandHistory_.end());
        }

        command->Execute();
        commandHistory_.push_back(std::move(command));
        currentCommandIndex_++;
    }

    void Undo() override {
        if (currentCommandIndex_ > 0) {
            currentCommandIndex_--;
            commandHistory_[currentCommandIndex_]->Undo();
        }
    }

    void Redo() override {
        if (currentCommandIndex_ < commandHistory_.size()) {
            commandHistory_[currentCommandIndex_]->Redo();
            currentCommandIndex_++;
        }
    }

    const char* GetName() const override {
        return editorName_.c_str();
    }

	void ClearHistory() {
		commandHistory_.clear();
		currentCommandIndex_ = 0;
	}
};