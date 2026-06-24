#include "editor/editor.h"
#include "core/application.h"
#include "agent/command.h"
#include "agent/script_runner.h"
#include "capture/capture.h"
#include "core/math.h"
#include "core/settings.h"
#include "debug/debug_state.h"
#include "renderer/metal_renderer.h"
#include "renderer/sky_presets.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
#include "scene/scene_io.h"
#include "scene/game_object.h"

using scene::MeshRenderer;

#include <cstdio>
#include <filesystem>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_osx.h"
#include "imgui/imgui_impl_metal.h"

#import <Metal/Metal.h>

Editor::Editor()
{
}

Editor::~Editor()
{
    shutdown();
}

bool Editor::init(void* nativeView, void* device)
{
    if (initialized_) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    NSView* view = (NSView*)nativeView;
    id<MTLDevice> mtlDevice = (id<MTLDevice>)device;

    std::filesystem::path iniDir(Settings::settingsDirectory());
    if (iniDir.empty()) iniDir = ".";
    imguiIniPath_ = (iniDir / "imgui.ini").string();
    io.IniFilename = imguiIniPath_.c_str();
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    if (!ImGui_ImplOSX_Init(view)) return false;
    if (!ImGui_ImplMetal_Init(mtlDevice)) return false;

    loadWindowStates();

    initialized_ = true;
    return true;
}

void Editor::shutdown()
{
    if (!initialized_) return;
    saveWindowStates();
    if (ImGui::GetCurrentContext()) {
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    }
    ImGui_ImplMetal_Shutdown();
    ImGui_ImplOSX_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void Editor::beginFrame(void* nativeView)
{
    NSView* view = (NSView*)nativeView;
    ImGui_ImplOSX_NewFrame(view);
    ImGui::NewFrame();
}

void Editor::endFrame()
{
    ImGui::Render();
}

void Editor::saveLayout()
{
    if (ImGui::GetCurrentContext()) {
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    }
}

void Editor::render(void* commandBuffer, void* renderEncoder, void* renderPassDescriptor)
{
    ImGui_ImplMetal_NewFrame((MTLRenderPassDescriptor*)renderPassDescriptor);

    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplMetal_RenderDrawData(drawData,
                                       (id<MTLCommandBuffer>)commandBuffer,
                                       (id<MTLRenderCommandEncoder>)renderEncoder);
    }
}

void Editor::drawUI(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount)
{
    drawMainMenuBar(scene);

    // Save (Ctrl+S or menu) on an unsaved scene routes here so the modal is
    // opened and drawn from the top-level path, not from inside the menu block.
    if (application_ && application_->consumeSaveAsRequest()) {
        requestSaveAs(scene);
    }
    drawSaveAsModal(scene);
    drawOpenModal();

    if (showHierarchy_) {
        drawHierarchy(scene, fps, frameTimeMs);
    }
    if (showInspector_) {
        drawInspector(scene);
    }
    if (showLighting_) {
        drawLightingPanel(scene);
    }
    if (showDiagnostics_) {
        drawDiagnostics(frame, fps, frameTimeMs, renderCommandCount, scene);
    }
    if (showAgentConsole_) {
        drawAgentConsole(scene, frame, fps, frameTimeMs, renderCommandCount);
    }
    if (showScriptRunner_) {
        drawScriptRunner(scene);
    }
    if (showScreenshot_) {
        drawScreenshotPanel(scene);
    }
    if (showReproBundle_) {
        drawReproBundlePanel(scene, frame, fps, frameTimeMs, renderCommandCount);
    }
    if (showShadowMap_) {
        drawShadowMapPanel();
    }
}

void Editor::markSceneDirty()
{
    if (application_) {
        application_->markSceneDirty();
    }
}

void Editor::pushUndo(Scene& scene)
{
    history_.push(scene);
}

void Editor::coalesceEdit(Scene& scene)
{
    // One undo entry per continuous drag/slider/color gesture: snapshot the
    // scene when the widget is activated (its value is still pre-edit), and
    // commit that snapshot only if the widget was edited before release. A
    // click-without-drag (activated but not edited) is discarded so it leaves
    // no phantom undo entry. Must be called immediately after the widget,
    // while it is still ImGui's "current item".
    if (ImGui::IsItemActivated()) {
        history_.beginPending(scene);
    } else if (ImGui::IsItemDeactivatedAfterEdit()) {
        history_.commitPending();
    } else if (ImGui::IsItemDeactivated()) {
        history_.discardPending();
    }
}

void Editor::beginDragEdit(Scene& scene)
{
    history_.beginPending(scene);
}

void Editor::endDragEdit()
{
    history_.commitPending();
}

void Editor::undo(Scene& scene)
{
    if (history_.undo(scene)) {
        // Snapshots don't track selection, and restored objects have fresh
        // pointers, so drop the selection. Mark dirty so the title updates.
        selected_ = nullptr;
        nameBuffer_[0] = '\0';
        markSceneDirty();
    }
}

void Editor::redo(Scene& scene)
{
    if (history_.redo(scene)) {
        selected_ = nullptr;
        nameBuffer_[0] = '\0';
        markSceneDirty();
    }
}

void Editor::clearHistory()
{
    history_.clear();
}

void Editor::requestSaveAs(Scene& scene)
{
    // Prefill with the current scene's base name (sans directory and the
    // .scene.json suffix), falling back to "untitled".
    std::string suggestion = "untitled";
    const std::string& path = scene.loadedScenePath();
    if (!path.empty()) {
        size_t slash = path.find_last_of("/\\");
        std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
        const std::string ext = ".scene.json";
        if (base.size() > ext.size() &&
            base.compare(base.size() - ext.size(), ext.size(), ext) == 0) {
            base = base.substr(0, base.size() - ext.size());
        }
        if (!base.empty()) {
            suggestion = base;
        }
    }
    std::snprintf(saveAsBuffer_, sizeof(saveAsBuffer_), "%s", suggestion.c_str());
    saveAsError_.clear();
    saveAsPending_ = true;
}

void Editor::drawSaveAsModal(Scene&)
{
    // OpenPopup is called here (top-level), not from the menu, so it shares the
    // ImGui ID stack with the matching BeginPopupModal below.
    if (saveAsPending_) {
        ImGui::OpenPopup("Save Scene As");
        saveAsPending_ = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Save to assets/scenes/");
    ImGui::SetNextItemWidth(240.0f);
    bool submit = ImGui::InputText("##saveas", saveAsBuffer_, sizeof(saveAsBuffer_),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::TextDisabled(".scene.json");

    if (SceneIO::exists(saveAsBuffer_)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Will overwrite existing file.");
    }
    if (!saveAsError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", saveAsError_.c_str());
    }

    bool empty = saveAsBuffer_[0] == '\0';
    ImGui::BeginDisabled(empty);
    bool doSave = ImGui::Button("Save", ImVec2(120.0f, 0.0f)) || (submit && !empty);
    ImGui::EndDisabled();

    if (doSave && application_) {
        if (application_->saveSceneAs(saveAsBuffer_, saveAsError_)) {
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

namespace {

// Display name for a scene path: the base filename without the .scene.json
// suffix (e.g. "assets/scenes/arena.scene.json" -> "arena").
std::string sceneDisplayName(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const std::string ext = ".scene.json";
    if (base.size() > ext.size() &&
        base.compare(base.size() - ext.size(), ext.size(), ext) == 0) {
        base = base.substr(0, base.size() - ext.size());
    }
    return base;
}

} // namespace

void Editor::requestOpen()
{
    openScenes_ = SceneIO::listScenes();
    openSelected_ = openScenes_.empty() ? -1 : 0;
    openError_.clear();
    openPending_ = true;
}

void Editor::drawOpenModal()
{
    // OpenPopup is called here (top-level), matching BeginPopupModal's ID stack.
    if (openPending_) {
        ImGui::OpenPopup("Open Scene");
        openPending_ = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("Open Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Scenes in assets/scenes/");

    if (openScenes_.empty()) {
        ImGui::TextDisabled("(no scenes found)");
    } else {
        ImGui::BeginChild("##scenelist", ImVec2(280.0f, 200.0f), true);
        for (int i = 0; i < static_cast<int>(openScenes_.size()); ++i) {
            const std::string name = sceneDisplayName(openScenes_[i]);
            if (ImGui::Selectable(name.c_str(), openSelected_ == i,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                openSelected_ = i;
                if (ImGui::IsMouseDoubleClicked(0) && application_) {
                    if (application_->loadScene(openScenes_[i], openError_)) {
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    if (!openError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", openError_.c_str());
    }

    bool hasSelection = openSelected_ >= 0 &&
                        openSelected_ < static_cast<int>(openScenes_.size());
    ImGui::BeginDisabled(!hasSelection);
    if (ImGui::Button("Open", ImVec2(120.0f, 0.0f)) && hasSelection && application_) {
        if (application_->loadScene(openScenes_[openSelected_], openError_)) {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Editor::drawMainMenuBar(Scene& scene)
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New",
#ifdef __APPLE__
                            "Cmd+N")) {
#else
                            "Ctrl+N")) {
#endif
            if (application_) {
                application_->newScene();
            }
        }
        if (ImGui::MenuItem("Open...")) {
            requestOpen();
        }
        if (ImGui::BeginMenu("Recent Scenes")) {
            std::vector<std::string> recent;
            Settings::loadRecentScenes(recent);
            if (recent.empty()) {
                ImGui::TextDisabled("(none)");
            }
            for (const std::string& path : recent) {
                bool present = std::filesystem::exists(path);
                // Missing files stay visible but disabled so the list still
                // reflects history without offering a broken action.
                ImGui::BeginDisabled(!present);
                if (ImGui::MenuItem(sceneDisplayName(path).c_str()) && application_) {
                    std::string error;
                    application_->loadScene(path, error);
                }
                ImGui::EndDisabled();
            }
            ImGui::EndMenu();
        }
#ifdef __APPLE__
        if (ImGui::MenuItem("Save", "Cmd+S")) {
#else
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
#endif
            if (application_) {
                application_->saveScene();
            }
        }
        if (ImGui::MenuItem("Save As...")) {
            requestSaveAs(scene);
        }
        ImGui::Separator();
#ifdef __APPLE__
        if (ImGui::MenuItem("Quit", "Cmd+Q")) {
#else
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
#endif
            if (application_) {
                application_->requestQuit();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::BeginDisabled(!history_.canUndo());
        if (ImGui::MenuItem("Undo",
#ifdef __APPLE__
                            "Cmd+Z")) {
#else
                            "Ctrl+Z")) {
#endif
            undo(scene);
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!history_.canRedo());
        if (ImGui::MenuItem("Redo",
#ifdef __APPLE__
                            "Cmd+Shift+Z")) {
#else
                            "Ctrl+Shift+Z")) {
#endif
            redo(scene);
        }
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Windows")) {
        struct Toggle {
            const char* label;
            bool* visible;
        };

        Toggle toggles[] = {
            {"Hierarchy", &showHierarchy_},
            {"Inspector", &showInspector_},
            {"Lighting", &showLighting_},
            {"Diagnostics", &showDiagnostics_},
            {"Agent Console", &showAgentConsole_},
            {"Script Runner", &showScriptRunner_},
            {"Screenshot", &showScreenshot_},
            {"Repro Bundle", &showReproBundle_},
            {"Shadow Map", &showShadowMap_},
        };

        for (const auto& toggle : toggles) {
            ImGui::MenuItem(toggle.label, nullptr, toggle.visible);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Camera")) {
        if (ImGui::MenuItem("Reset Camera")) {
            scene.camera().reset();
            markSceneDirty();
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void Editor::drawHierarchy(Scene& scene, float fps, float frameTimeMs)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("Hierarchy", &showHierarchy_);

    ImGui::Text("%.1f FPS  %.2f ms", fps, frameTimeMs);
    ImGui::Separator();

    drawHierarchyToolbar(scene);

    ImGui::Separator();

    // Recursive tree: roots in objects_ order, children in insertion order.
    for (const auto& obj : scene.objects()) {
        if (obj->parent() == nullptr) {
            drawHierarchyNode(scene, obj.get());
        }
    }

    // Empty-area drop zone: dropping an object here detaches it (makes a root).
    {
        ImVec2 remaining = ImGui::GetContentRegionAvail();
        if (remaining.y < 40.0f) remaining.y = 40.0f;
        ImGui::InvisibleButton("##detach_zone", ImVec2(-FLT_MIN, remaining.y));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
                if (p->DataSize == sizeof(GameObject*)) {
                    GameObject* dragged = *static_cast<GameObject**>(p->Data);
                    pushUndo(scene);
                    scene.setParent(dragged, nullptr);
                    markSceneDirty();
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::End();
}

void Editor::drawHierarchyToolbar(Scene& scene)
{
    if (ImGui::Button("Create ▾")) {
        ImGui::OpenPopup("##CreateMenu");
    }

    if (ImGui::BeginPopup("##CreateMenu")) {
        if (ImGui::MenuItem("Cube")) {
            createPrimitive(scene, scene::MeshShape::Cube, "Cube");
        }
        if (ImGui::MenuItem("Sphere")) {
            createPrimitive(scene, scene::MeshShape::Sphere, "Sphere");
        }
        if (ImGui::MenuItem("Plane")) {
            createPrimitive(scene, scene::MeshShape::Plane, "Plane");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Empty Object")) {
            pushUndo(scene);
            GameObject* obj = scene.createObject("Object");
            selected_ = obj;
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", obj->name().c_str());
            markSceneDirty();
        }
        ImGui::EndPopup();
    }

    bool canModifySelection = selected_ && !scene.isCamera(selected_);

    ImGui::SameLine();
    ImGui::BeginDisabled(!canModifySelection);
    if (ImGui::Button("Duplicate")) {
        pushUndo(scene);
        GameObject* dup = scene.duplicateObject(selected_);
        if (dup) {
            selected_ = dup;
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", dup->name().c_str());
            markSceneDirty();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        pushUndo(scene);
        scene.deleteObject(selected_);
        selected_ = nullptr;
        nameBuffer_[0] = '\0';
        markSceneDirty();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!(canModifySelection && selected_->parent() != nullptr));
    if (ImGui::Button("Detach")) {
        pushUndo(scene);
        scene.setParent(selected_, nullptr);
        markSceneDirty();
    }
    ImGui::EndDisabled();
}

void Editor::createPrimitive(Scene& scene, scene::MeshShape shape, const char* name)
{
    pushUndo(scene);
    GameObject* obj = scene.createObject(name);
    obj->transform().position = {0.0f, 0.5f, 0.0f};
    obj->transform().scale = {0.5f, 0.5f, 0.5f};
    auto mesh = std::make_unique<MeshRenderer>();
    mesh->shape = shape;
    mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
    obj->addComponent(std::move(mesh));
    selected_ = obj;
    std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", obj->name().c_str());
    markSceneDirty();
}

void Editor::drawHierarchyNode(Scene& scene, GameObject* obj)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool isLeaf = obj->children().empty();
    if (isLeaf || scene.isCamera(obj)) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    }
    if (selected_ == obj) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool nodeOpen = ImGui::TreeNodeEx(obj, flags, "%s", obj->name().c_str());

    // Click anywhere on the node selects it.
    if (ImGui::IsItemClicked()) {
        selected_ = obj;
        std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", obj->name().c_str());
    }

    // Drag source (the camera is not draggable).
    if (!scene.isCamera(obj) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        GameObject* payload = obj;
        ImGui::SetDragDropPayload("GAMEOBJECT", &payload, sizeof(payload));
        ImGui::Text("%s", obj->name().c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target: reparent the dragged object onto this one. Scene validates
    // cycles and the camera; invalid drops are simply ignored.
    if (!scene.isCamera(obj) && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("GAMEOBJECT")) {
            if (p->DataSize == sizeof(GameObject*)) {
                GameObject* dragged = *static_cast<GameObject**>(p->Data);
                if (dragged != obj) {
                    pushUndo(scene);
                    scene.setParent(dragged, obj);
                    markSceneDirty();
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (nodeOpen) {
        for (GameObject* child : obj->children()) {
            drawHierarchyNode(scene, child);
        }
        ImGui::TreePop();
    }
}

void Editor::drawInspector(Scene& scene)
{
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector", &showInspector_);

    if (!selected_) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", selected_->name().c_str());

    if (ImGui::InputText("Name", nameBuffer_, sizeof(nameBuffer_))) {
        selected_->setName(nameBuffer_);
        markSceneDirty();
    }

    // Read-only hierarchy readout (local authoring stays in Transform below).
    if (GameObject* p = selected_->parent()) {
        ImGui::Text("Parent: %s", p->name().c_str());
    } else {
        ImGui::TextDisabled("Parent: (root)");
    }
    const Mat4& wm = selected_->transform().worldMatrix();
    ImGui::Text("World Pos: %.2f, %.2f, %.2f", wm.columns[3][0], wm.columns[3][1], wm.columns[3][2]);

    ImGui::Separator();
    ImGui::Text("Transform");

    Vec3& position = selected_->transform().position;
    Vec3& rotation = selected_->transform().rotation;
    Vec3& scale = selected_->transform().scale;

    if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
        markSceneDirty();
    }
    coalesceEdit(scene);
    if (ImGui::DragFloat3("Rotation", &rotation.x, 0.05f)) {
        markSceneDirty();
    }
    coalesceEdit(scene);
    if (ImGui::DragFloat3("Scale", &scale.x, 0.05f)) {
        markSceneDirty();
    }
    coalesceEdit(scene);

    ImGui::Separator();
    ImGui::Text("Components");

    if (MeshRenderer* mesh = selected_->getComponent<MeshRenderer>()) {
        ImGui::Text("MeshRenderer");

        // shapeNames order must match MeshShape enum order (Cube=0, Sphere=1, Plane=2).
        const char* shapeNames[] = {"Cube", "Sphere", "Plane"};
        int current = static_cast<int>(mesh->shape);
        if (ImGui::Combo("Shape", &current, shapeNames, IM_ARRAYSIZE(shapeNames))) {
            pushUndo(scene);
            mesh->shape = static_cast<scene::MeshShape>(current);
            markSceneDirty();
        }

        float color[4] = {mesh->color.x, mesh->color.y, mesh->color.z, mesh->color.w};
        if (ImGui::ColorEdit4("Color", color)) {
            mesh->color = {color[0], color[1], color[2], color[3]};
            markSceneDirty();
        }
        coalesceEdit(scene);
    }

    if (scene::RotateComponent* rot = selected_->getComponent<scene::RotateComponent>()) {
        ImGui::Text("RotateComponent");
        if (ImGui::DragFloat3("Angular Velocity", &rot->angularVelocityEuler.x, 1.0f)) {
            markSceneDirty();
        }
        coalesceEdit(scene);
    }

    ImGui::Separator();

    ImGui::BeginDisabled(selected_->hasComponent<MeshRenderer>());
    if (ImGui::Button("Add MeshRenderer")) {
        pushUndo(scene);
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
        selected_->addComponent(std::move(mesh));
        markSceneDirty();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(selected_->hasComponent<scene::RotateComponent>());
    if (ImGui::Button("Add RotateComponent")) {
        pushUndo(scene);
        selected_->addComponent(std::make_unique<scene::RotateComponent>());
        markSceneDirty();
    }
    ImGui::EndDisabled();

    ImGui::End();
}

void Editor::drawLightingPanel(Scene& scene)
{
    if (!lightSettings_) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(280, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lighting", &showLighting_);

    // Store the raw direction; normalization happens where it's consumed
    // (RenderContext::setLight and the sky shader). Normalizing here would
    // rescale all three components on every edit, so dragging one field would
    // visibly move the others.
    float dir[3] = {lightSettings_->direction.x, lightSettings_->direction.y, lightSettings_->direction.z};
    if (ImGui::DragFloat3("Direction", dir, 0.01f)) {
        lightSettings_->direction = {dir[0], dir[1], dir[2]};
        markSceneDirty();
    }
    coalesceEdit(scene);

    Vec3 n = normalize({lightSettings_->direction.x, lightSettings_->direction.y, lightSettings_->direction.z});
    ImGui::Text("Normalized: %.3f, %.3f, %.3f", n.x, n.y, n.z);

    if (ImGui::SliderFloat("Ambient", &lightSettings_->ambient, 0.0f, 1.0f)) {
        markSceneDirty();
    }
    coalesceEdit(scene);
    if (ImGui::SliderFloat("Diffuse", &lightSettings_->diffuse, 0.0f, 2.0f)) {
        markSceneDirty();
    }
    coalesceEdit(scene);

    if (skySettings_) {
        if (ImGui::CollapsingHeader("Sky")) {
            float horizon[3] = {skySettings_->horizonColor.x, skySettings_->horizonColor.y, skySettings_->horizonColor.z};
            if (ImGui::ColorEdit3("Horizon", horizon)) {
                skySettings_->horizonColor = {horizon[0], horizon[1], horizon[2]};
                markSceneDirty();
            }
            coalesceEdit(scene);

            float zenith[3] = {skySettings_->zenithColor.x, skySettings_->zenithColor.y, skySettings_->zenithColor.z};
            if (ImGui::ColorEdit3("Zenith", zenith)) {
                skySettings_->zenithColor = {zenith[0], zenith[1], zenith[2]};
                markSceneDirty();
            }
            coalesceEdit(scene);

            float sun[3] = {skySettings_->sunColor.x, skySettings_->sunColor.y, skySettings_->sunColor.z};
            if (ImGui::ColorEdit3("Sun", sun)) {
                skySettings_->sunColor = {sun[0], sun[1], sun[2]};
                markSceneDirty();
            }
            coalesceEdit(scene);

            // sunSize is the sun's angular radius in radians (consumed directly
            // by sky.metal's acos comparison). Log scale + 4 decimals give usable
            // resolution at the small end of the 0.001..0.05 rad range.
            if (ImGui::SliderFloat("Sun Radius (rad)", &skySettings_->sunSize, 0.001f, 0.05f,
                               "%.4f", ImGuiSliderFlags_Logarithmic)) {
                markSceneDirty();
            }
            coalesceEdit(scene);
            if (ImGui::SliderFloat("Sun Intensity", &skySettings_->sunIntensity, 0.0f, 5.0f)) {
                markSceneDirty();
            }
            coalesceEdit(scene);

            if (lightSettings_) {
                ImGui::SeparatorText("Presets");
                const auto& presets = skyPresets();
                for (size_t i = 0; i < presets.size(); ++i) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(presets[i].name)) {
                        pushUndo(scene);
                        *lightSettings_ = presets[i].light;
                        *skySettings_ = presets[i].sky;
                        markSceneDirty();
                    }
                }
            }
        }
    }

    ImGui::End();
}

void Editor::drawShadowMapPanel()
{
    ImGui::SetNextWindowPos(ImVec2(650, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 340), ImGuiCond_FirstUseEver);
    ImGui::Begin("Shadow Map", &showShadowMap_);

    void* tex = renderer_ ? renderer_->shadowMapTexture() : nullptr;
    if (tex) {
        ImGui::TextUnformatted("Directional-light depth (reversed-Z:");
        ImGui::TextUnformatted("brighter = closer to the light)");
        float side = ImGui::GetContentRegionAvail().x;
        ImGui::Image((ImTextureID)tex, ImVec2(side, side));
    } else {
        ImGui::TextUnformatted("Shadow map unavailable.");
    }

    ImGui::End();
}

void Editor::drawAgentConsole(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount)
{
    ImGui::SetNextWindowPos(ImVec2(280, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Agent Console", &showAgentConsole_);

    ImGui::InputText("Command", commandBuffer_, sizeof(commandBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Execute")) {
        AgentCommandContext ctx{scene, selected_, frame, fps, frameTimeMs, renderCommandCount, lastScriptPath_, renderer_, lastCapturePath_, lastBundlePath_, &lastAssertionFailure_};
        AgentCommandResult result = executeCommand(commandBuffer_, ctx);

        consoleOutput_ += "> " + std::string(commandBuffer_) + "\n";
        consoleOutput_ += result.output;
        if (!consoleOutput_.empty() && consoleOutput_.back() != '\n') {
            consoleOutput_ += "\n";
        }
        commandBuffer_[0] = '\0';
        lastScriptPath_ = ctx.lastScriptPath;
        lastCapturePath_ = ctx.lastCapturePath;
        lastBundlePath_ = ctx.lastBundlePath;
    }

    ImGui::Separator();

    ImVec2 outputSize = ImGui::GetContentRegionAvail();
    if (ImGui::BeginChild("##output", outputSize, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::TextUnformatted(consoleOutput_.data(),
                               consoleOutput_.data() + consoleOutput_.size());
    }
    ImGui::EndChild();

    ImGui::End();
}

void Editor::drawScriptRunner(Scene& scene)
{
    ImGui::SetNextWindowPos(ImVec2(650, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Script Runner", &showScriptRunner_);

    ImGui::InputText("Script File", scriptBuffer_, sizeof(scriptBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Run Script")) {
        AgentCommandContext ctx{scene, selected_, 0, 0.0f, 0.0f, 0, lastScriptPath_, renderer_, lastCapturePath_, lastBundlePath_, &lastAssertionFailure_};
        AgentCommandResult result = executeCommand(std::string("script.run ") + scriptBuffer_, ctx);

        scriptOutput_ = result.output;
        if (!result.success && !result.output.empty()) {
            scriptOutput_ += result.output;
        }
        lastScriptPath_ = ctx.lastScriptPath;
        lastCapturePath_ = ctx.lastCapturePath;
        lastBundlePath_ = ctx.lastBundlePath;
    }

    ImGui::Separator();

    ImVec2 outputSize = ImGui::GetContentRegionAvail();
    if (ImGui::BeginChild("##script_output", outputSize, false, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::TextUnformatted(scriptOutput_.data(),
                               scriptOutput_.data() + scriptOutput_.size());
    }
    ImGui::EndChild();

    ImGui::End();
}

void Editor::drawScreenshotPanel(Scene& scene)
{
    ImGui::SetNextWindowPos(ImVec2(650, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Screenshot", &showScreenshot_);

    ImGui::InputText("Filename", screenshotBuffer_, sizeof(screenshotBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Capture Screenshot")) {
        AgentCommandContext ctx{scene, selected_, 0, 0.0f, 0.0f, 0, lastScriptPath_, renderer_, lastCapturePath_, lastBundlePath_, &lastAssertionFailure_};
        std::string command = std::string("render.capture ") + screenshotBuffer_;
        AgentCommandResult result = executeCommand(command, ctx);
        screenshotOutput_ = result.output;
        lastCapturePath_ = ctx.lastCapturePath;
        lastBundlePath_ = ctx.lastBundlePath;
    }

    if (!screenshotOutput_.empty()) {
        ImGui::TextUnformatted(screenshotOutput_.data(),
                               screenshotOutput_.data() + screenshotOutput_.size());
    }

    ImGui::End();
}

void Editor::drawReproBundlePanel(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount)
{
    ImGui::SetNextWindowPos(ImVec2(650, 450), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Repro Bundle", &showReproBundle_);

    ImGui::InputText("Bundle Name", bundleBuffer_, sizeof(bundleBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Create Bundle")) {
        AgentCommandContext ctx{scene, selected_, frame, fps, frameTimeMs, renderCommandCount, lastScriptPath_, renderer_, lastCapturePath_, lastBundlePath_, &lastAssertionFailure_};
        std::string command = std::string("debug.bundle ") + bundleBuffer_;
        AgentCommandResult result = executeCommand(command, ctx);
        bundleOutput_ = result.output;
        lastScriptPath_ = ctx.lastScriptPath;
        lastCapturePath_ = ctx.lastCapturePath;
        lastBundlePath_ = ctx.lastBundlePath;
    }

    if (!bundleOutput_.empty()) {
        ImGui::TextUnformatted(bundleOutput_.data(),
                               bundleOutput_.data() + bundleOutput_.size());
    }

    ImGui::End();
}

void Editor::drawDiagnostics(uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount, Scene& scene)
{
    ImGui::SetNextWindowPos(ImVec2(10, 490), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Diagnostics", &showDiagnostics_);

    if (ImGui::Button("Write Debug State")) {
        std::string text = DebugState::build(frame, fps, frameTimeMs, renderCommandCount, scene, selected_, lastScriptPath_, lastCapturePath_, lastBundlePath_, &lastAssertionFailure_);
        DebugState::writeToFile(text);
    }

    if (ImGui::Button("Copy Debug State")) {
        std::string text = DebugState::build(frame, fps, frameTimeMs, renderCommandCount, scene, selected_, lastScriptPath_, lastCapturePath_, lastBundlePath_, &lastAssertionFailure_);
        ImGui::SetClipboardText(text.c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("Reset Camera")) {
        scene.camera().reset();
        markSceneDirty();
    }

    ImGui::End();
}

void Editor::loadWindowStates()
{
    std::unordered_map<std::string, bool> states;
    Settings::loadEditorWindowStates(states);

    struct Mapping {
        const char* key;
        bool* visible;
    };

    Mapping mappings[] = {
        {"Hierarchy", &showHierarchy_},
        {"Inspector", &showInspector_},
        {"Lighting", &showLighting_},
        {"Diagnostics", &showDiagnostics_},
        {"AgentConsole", &showAgentConsole_},
        {"ScriptRunner", &showScriptRunner_},
        {"Screenshot", &showScreenshot_},
        {"ReproBundle", &showReproBundle_},
        {"ShadowMap", &showShadowMap_},
    };

    for (const auto& mapping : mappings) {
        auto it = states.find(mapping.key);
        if (it != states.end()) {
            *mapping.visible = it->second;
        }
    }
}

void Editor::saveWindowStates()
{
    std::unordered_map<std::string, bool> states = {
        {"Hierarchy", showHierarchy_},
        {"Inspector", showInspector_},
        {"Lighting", showLighting_},
        {"Diagnostics", showDiagnostics_},
        {"AgentConsole", showAgentConsole_},
        {"ScriptRunner", showScriptRunner_},
        {"Screenshot", showScreenshot_},
        {"ReproBundle", showReproBundle_},
        {"ShadowMap", showShadowMap_},
    };

    Settings::saveEditorWindowStates(states);
}
