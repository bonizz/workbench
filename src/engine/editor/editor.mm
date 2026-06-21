#include "editor/editor.h"
#include "agent/command.h"
#include "agent/script_runner.h"
#include "capture/capture.h"
#include "core/math.h"
#include "core/settings.h"
#include "debug/debug_state.h"
#include "renderer/metal_renderer.h"
#include "scene/mesh_renderer.h"
#include "scene/rotate_component.h"
#include "scene/scene.h"
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
    drawMainMenuBar();

    if (showHierarchy_) {
        drawHierarchy(scene, fps, frameTimeMs);
    }
    if (showInspector_) {
        drawInspector();
    }
    if (showLighting_) {
        drawLightingPanel();
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
}

void Editor::drawMainMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
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
        };

        for (const auto& toggle : toggles) {
            ImGui::MenuItem(toggle.label, nullptr, toggle.visible);
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
                    scene.setParent(dragged, nullptr);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Create Cube")) {
        GameObject* obj = scene.createObject("Cube");
        obj->transform().position = {0.0f, 0.5f, 0.0f};
        obj->transform().scale = {0.5f, 0.5f, 0.5f};
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
        obj->addComponent(std::move(mesh));
        selected_ = obj;
        std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", obj->name().c_str());
    }

    bool canModifySelection = selected_ && !scene.isCamera(selected_);

    ImGui::SameLine();
    ImGui::BeginDisabled(!canModifySelection);
    if (ImGui::Button("Duplicate")) {
        GameObject* dup = scene.duplicateObject(selected_);
        if (dup) {
            selected_ = dup;
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", dup->name().c_str());
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        scene.deleteObject(selected_);
        selected_ = nullptr;
        nameBuffer_[0] = '\0';
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!(canModifySelection && selected_->parent() != nullptr));
    if (ImGui::Button("Detach")) {
        scene.setParent(selected_, nullptr);
    }
    ImGui::EndDisabled();

    ImGui::End();
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
                    scene.setParent(dragged, obj);
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

void Editor::drawInspector()
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

    ImGui::DragFloat3("Position", &position.x, 0.1f);
    ImGui::DragFloat3("Rotation", &rotation.x, 0.05f);
    ImGui::DragFloat3("Scale", &scale.x, 0.05f);

    ImGui::Separator();
    ImGui::Text("Components");

    if (MeshRenderer* mesh = selected_->getComponent<MeshRenderer>()) {
        ImGui::Text("MeshRenderer");

        // shapeNames order must match MeshShape enum order (Cube=0, Sphere=1, Plane=2).
        const char* shapeNames[] = {"Cube", "Sphere", "Plane"};
        int current = static_cast<int>(mesh->shape);
        if (ImGui::Combo("Shape", &current, shapeNames, IM_ARRAYSIZE(shapeNames))) {
            mesh->shape = static_cast<scene::MeshShape>(current);
        }

        float color[4] = {mesh->color.x, mesh->color.y, mesh->color.z, mesh->color.w};
        if (ImGui::ColorEdit4("Color", color)) {
            mesh->color = {color[0], color[1], color[2], color[3]};
        }
    }

    if (scene::RotateComponent* rot = selected_->getComponent<scene::RotateComponent>()) {
        ImGui::Text("RotateComponent");
        ImGui::DragFloat3("Angular Velocity", &rot->angularVelocityEuler.x, 1.0f);
    }

    ImGui::Separator();

    ImGui::BeginDisabled(selected_->hasComponent<MeshRenderer>());
    if (ImGui::Button("Add MeshRenderer")) {
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
        selected_->addComponent(std::move(mesh));
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(selected_->hasComponent<scene::RotateComponent>());
    if (ImGui::Button("Add RotateComponent")) {
        selected_->addComponent(std::make_unique<scene::RotateComponent>());
    }
    ImGui::EndDisabled();

    ImGui::End();
}

void Editor::drawLightingPanel()
{
    if (!lightSettings_) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(280, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 180), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lighting", &showLighting_);

    float dir[3] = {lightSettings_->direction.x, lightSettings_->direction.y, lightSettings_->direction.z};
    if (ImGui::DragFloat3("Direction", dir, 0.01f)) {
        Vec3 v = {dir[0], dir[1], dir[2]};
        v = normalize(v);
        lightSettings_->direction = {v.x, v.y, v.z};
    }

    Vec3 n = normalize({lightSettings_->direction.x, lightSettings_->direction.y, lightSettings_->direction.z});
    ImGui::Text("Normalized: %.3f, %.3f, %.3f", n.x, n.y, n.z);

    ImGui::SliderFloat("Ambient", &lightSettings_->ambient, 0.0f, 1.0f);
    ImGui::SliderFloat("Diffuse", &lightSettings_->diffuse, 0.0f, 2.0f);

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
    };

    Settings::saveEditorWindowStates(states);
}
