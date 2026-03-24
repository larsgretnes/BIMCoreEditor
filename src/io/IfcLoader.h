#pragma once
// =============================================================================
// BimCore/scene/IfcLoader.h
// Parses an IFC file on a background thread and produces a SceneModel.
// =============================================================================
#include <string>
#include <atomic>
#include <mutex>
#include <memory>
#include "scene/SceneModel.h"

namespace BimCore {

// Thread-safe progress/status tracker (main thread reads, loader thread writes)
struct LoadState {
    std::atomic<bool>  isLoaded { false };
    std::atomic<bool>  hasError { false };
    std::atomic<float> progress { 0.0f  };

    void SetStatus(const std::string& text, float prog = -1.0f) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusText = text;
        if (prog >= 0.0f) progress.store(prog, std::memory_order_relaxed);
    }

    std::string GetStatus() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_statusText;
    }

    void Reset() {
        isLoaded.store(false); hasError.store(false);
        progress.store(0.0f);
        SetStatus("Idle", 0.0f);
    }

private:
    std::string m_statusText = "Idle";
    std::mutex  m_mutex;
};

class IfcLoader {
public:
    static std::shared_ptr<SceneModel> LoadDocument(
        const std::string& filepath,
        LoadState* state = nullptr);
};

} // namespace BimCore