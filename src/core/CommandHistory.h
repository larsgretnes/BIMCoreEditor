// =============================================================================
// BimCore/core/CommandHistory.h
// =============================================================================
#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>
#include "ui/UIState.h"
#include "scene/SceneModel.h"

namespace BimCore {

    // -------------------------------------------------------------------------
    // Base Command Interface
    // -------------------------------------------------------------------------
    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual std::string GetName() const = 0;
    };

    // -------------------------------------------------------------------------
    // The Command Manager
    // -------------------------------------------------------------------------
    class CommandHistory {
    public:
        void ExecuteCommand(std::unique_ptr<ICommand> command);
        void Undo();
        void Redo();
        void Clear();
        
        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        std::string GetLastCommandName() const { return CanUndo() ? m_undoStack.back()->GetName() : ""; }

    private:
        std::vector<std::unique_ptr<ICommand>> m_undoStack;
        std::vector<std::unique_ptr<ICommand>> m_redoStack;
    };

    // -------------------------------------------------------------------------
    // Concrete Command: Transform Objects (Gizmo Move/Rotate)
    // -------------------------------------------------------------------------
    class CmdTransform : public ICommand {
    public:
        struct TransformData {
            std::shared_ptr<SceneModel> doc;
            std::string guid;
            glm::mat4 oldTransform;
            glm::mat4 newTransform;
        };

        CmdTransform(SelectionState& state, const std::vector<TransformData>& data);
        void Execute() override;
        void Undo() override;
        std::string GetName() const override { return "Transform Objects"; }

    private:
        SelectionState& m_state;
        std::vector<TransformData> m_data;
    };

    // -------------------------------------------------------------------------
    // Concrete Command: Delete/Hide Objects
    // -------------------------------------------------------------------------
    class CmdDelete : public ICommand {
    public:
        CmdDelete(SelectionState& state, const std::vector<std::string>& guids);
        void Execute() override;
        void Undo() override;
        std::string GetName() const override { return "Delete Objects"; }

    private:
        SelectionState& m_state;
        std::vector<std::string> m_guids;
        std::unordered_set<std::string> m_previouslyDeleted; 
        std::unordered_set<std::string> m_previouslyHidden;
    };

} // namespace BimCore