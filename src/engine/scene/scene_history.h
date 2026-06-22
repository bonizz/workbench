#pragma once

#include <cstddef>
#include <deque>
#include <string>

class Scene;

// Snapshot-based undo/redo for scene authoring. Stores serialized Scene
// snapshots (compact JSON) in memory. Each push() records the CURRENT scene as
// the "before" state of an edit; undo() restores the previous checkpoint and
// stashes the live state for redo().
//
// Scope (see docs/plans/0.15_undo_redo.md): major scene-authoring edits only —
// object create/delete/duplicate/detach/reparent, inspector transform/mesh/
// material edits, add/remove components, and lighting/sky edits. Camera
// navigation, selection, panel layout, and open/save are excluded. The camera
// is never restored by undo (navigation stays live): snapshots include the
// camera for symmetry with save(), but deserialize() skips it.
class SceneHistory
{
public:
    static constexpr size_t kMaxEntries = 50;

    // Removes all undo and redo checkpoints (e.g. on scene load/new).
    void clear();

    // Snapshots the current scene as the pre-edit checkpoint, clears redo, and
    // trims to kMaxEntries (oldest first).
    void push(Scene& scene);

    // Commits an already-serialized snapshot (e.g. one captured at the start of
    // a coalesced drag). Same bookkeeping as push() without re-serializing.
    void commitSnapshot(std::string snapshot);

    // Restores the previous checkpoint. Saves the current live state to redo.
    // Returns false if there is nothing to undo.
    bool undo(Scene& scene);

    // Restores a redo checkpoint. Saves the current live state to undo.
    // Returns false if there is nothing to redo.
    bool redo(Scene& scene);

    bool canUndo() const;
    bool canRedo() const;
    size_t undoCount() const;
    size_t redoCount() const;

private:
    std::deque<std::string> undo_;
    std::deque<std::string> redo_;

    void trimUndo();
};
