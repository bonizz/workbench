#include "scene/scene_history.h"

#include "scene/scene.h"
#include "scene/scene_serializer.h"

void SceneHistory::clear()
{
    undo_.clear();
    redo_.clear();
    pending_.clear();
}

void SceneHistory::trimUndo()
{
    while (undo_.size() > kMaxEntries) {
        undo_.pop_front();
    }
}

void SceneHistory::push(Scene& scene)
{
    undo_.push_back(SceneSerializer::serialize(scene));
    redo_.clear();
    trimUndo();
}

void SceneHistory::beginPending(Scene& scene)
{
    pending_ = SceneSerializer::serialize(scene);
}

void SceneHistory::commitPending()
{
    if (pending_.empty()) {
        return;
    }
    undo_.push_back(std::move(pending_));
    pending_.clear();
    redo_.clear();
    trimUndo();
}

void SceneHistory::discardPending()
{
    pending_.clear();
}

bool SceneHistory::undo(Scene& scene)
{
    if (undo_.empty()) {
        return false;
    }
    // Stash the live (post-edit) state so redo can return to it, then restore
    // the pre-edit checkpoint.
    redo_.push_back(SceneSerializer::serialize(scene));
    std::string snapshot = std::move(undo_.back());
    undo_.pop_back();

    std::string error;
    SceneSerializer::deserialize(scene, snapshot, error);
    return true;
}

bool SceneHistory::redo(Scene& scene)
{
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(SceneSerializer::serialize(scene));
    trimUndo();
    std::string snapshot = std::move(redo_.back());
    redo_.pop_back();

    std::string error;
    SceneSerializer::deserialize(scene, snapshot, error);
    return true;
}

bool SceneHistory::canUndo() const { return !undo_.empty(); }
bool SceneHistory::canRedo() const { return !redo_.empty(); }
size_t SceneHistory::undoCount() const { return undo_.size(); }
size_t SceneHistory::redoCount() const { return redo_.size(); }
