// =============================================================================
// BimCore/core/CommandHistory.cpp
// =============================================================================
#include "CommandHistory.h"

namespace BimCore {

    // =========================================================================
    // CommandHistory Manager
    // =========================================================================
    void CommandHistory::ExecuteCommand(std::unique_ptr<ICommand> command) {
        command->Execute();
        m_undoStack.push_back(std::move(command));
        m_redoStack.clear(); // Whenever you do a new action, the redo timeline is wiped
    }

    void CommandHistory::Undo() {
        if (m_undoStack.empty()) return;
        auto command = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        command->Undo();
        m_redoStack.push_back(std::move(command));
    }

    void CommandHistory::Redo() {
        if (m_redoStack.empty()) return;
        auto command = std::move(m_redoStack.back());
        m_redoStack.pop_back();
        command->Execute();
        m_undoStack.push_back(std::move(command));
    }

    void CommandHistory::Clear() {
        m_undoStack.clear();
        m_redoStack.clear();
    }

    // =========================================================================
    // CmdTransform Implementation
    // =========================================================================
    CmdTransform::CmdTransform(SelectionState& state, const std::vector<TransformData>& data)
        : m_state(state), m_data(data) {}

    void CmdTransform::Execute() {
        for (const auto& item : m_data) {
            item.doc->SetObjectTransform(item.guid, item.newTransform);
        }
        m_state.updateGeometry = true;
    }

    void CmdTransform::Undo() {
        for (const auto& item : m_data) {
            item.doc->SetObjectTransform(item.guid, item.oldTransform);
        }
        m_state.updateGeometry = true;
    }

    // =========================================================================
    // CmdDelete Implementation
    // =========================================================================
    CmdDelete::CmdDelete(SelectionState& state, const std::vector<std::string>& guids)
        : m_state(state), m_guids(guids) {
        // Capture the prior state so we can restore it exactly during Undo
        for (const auto& guid : m_guids) {
            if (m_state.deletedObjects.count(guid)) m_previouslyDeleted.insert(guid);
            if (m_state.hiddenObjects.count(guid))  m_previouslyHidden.insert(guid);
        }
    }

    void CmdDelete::Execute() {
        for (const auto& guid : m_guids) {
            m_state.deletedObjects.insert(guid);
            m_state.hiddenObjects.insert(guid);
        }
        m_state.objects.clear(); // Clear selection so we don't hold deleted items
        m_state.hiddenStateChanged = true;
        m_state.selectionChanged = true;
    }

    void CmdDelete::Undo() {
        for (const auto& guid : m_guids) {
            if (!m_previouslyDeleted.count(guid)) m_state.deletedObjects.erase(guid);
            if (!m_previouslyHidden.count(guid))  m_state.hiddenObjects.erase(guid);
        }
        m_state.hiddenStateChanged = true;
    }

} // namespace BimCore