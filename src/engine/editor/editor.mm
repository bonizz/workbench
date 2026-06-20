#include "editor/editor.h"
#include "agent/command.h"
#include "agent/script_runner.h"
#include "capture/capture.h"
#include "debug/debug_state.h"
#include "renderer/metal_renderer.h"
#include "scene/mesh_renderer.h"
#include "scene/scene.h"
#include "scene/game_object.h"

using scene::MeshRenderer;

#include <cstdio>

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

    if (!ImGui_ImplOSX_Init(view)) return false;
    if (!ImGui_ImplMetal_Init(mtlDevice)) return false;

    initialized_ = true;
    return true;
}

void Editor::shutdown()
{
    if (!initialized_) return;
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
    drawHierarchy(scene, fps, frameTimeMs);
    drawInspector();
    drawDiagnostics(frame, fps, frameTimeMs, renderCommandCount, scene);
    drawAgentConsole(scene, frame, fps, frameTimeMs, renderCommandCount);
    drawScriptRunner(scene);
    drawScreenshotPanel(scene);
}

void Editor::drawHierarchy(Scene& scene, float fps, float frameTimeMs)
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Hierarchy");

    ImGui::Text("%.1f FPS  %.2f ms", fps, frameTimeMs);
    ImGui::Separator();

    for (const auto& obj : scene.objects()) {
        ImGui::PushID(obj.get());
        bool isSelected = (selected_ == obj.get());
        const std::string& name = obj->name();
        if (ImGui::Selectable(name.c_str(), isSelected)) {
            selected_ = obj.get();
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", name.c_str());
        }
        ImGui::PopID();
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

    ImGui::End();
}

void Editor::drawInspector()
{
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    if (!selected_) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", selected_->name().c_str());

    if (ImGui::InputText("Name", nameBuffer_, sizeof(nameBuffer_))) {
        selected_->setName(nameBuffer_);
    }

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
        float color[4] = {mesh->color.x, mesh->color.y, mesh->color.z, mesh->color.w};
        if (ImGui::ColorEdit4("Color", color)) {
            mesh->color = {color[0], color[1], color[2], color[3]};
        }
    }

    ImGui::Separator();

    ImGui::BeginDisabled(selected_->hasComponent<MeshRenderer>());
    if (ImGui::Button("Add MeshRenderer")) {
        auto mesh = std::make_unique<MeshRenderer>();
        mesh->color = {0.95f, 0.55f, 0.20f, 1.0f};
        selected_->addComponent(std::move(mesh));
    }
    ImGui::EndDisabled();

    ImGui::End();
}

void Editor::drawAgentConsole(Scene& scene, uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount)
{
    ImGui::SetNextWindowPos(ImVec2(280, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Agent Console");

    ImGui::InputText("Command", commandBuffer_, sizeof(commandBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Execute")) {
        AgentCommandContext ctx{scene, selected_, frame, fps, frameTimeMs, renderCommandCount, lastScriptPath_, renderer_, lastCapturePath_};
        AgentCommandResult result = executeCommand(commandBuffer_, ctx);

        consoleOutput_ += "> " + std::string(commandBuffer_) + "\n";
        consoleOutput_ += result.output;
        if (!consoleOutput_.empty() && consoleOutput_.back() != '\n') {
            consoleOutput_ += "\n";
        }
        commandBuffer_[0] = '\0';
        lastScriptPath_ = ctx.lastScriptPath;
        lastCapturePath_ = ctx.lastCapturePath;
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
    ImGui::Begin("Script Runner");

    ImGui::InputText("Script File", scriptBuffer_, sizeof(scriptBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Run Script")) {
        AgentCommandContext ctx{scene, selected_, 0, 0.0f, 0.0f, 0, lastScriptPath_, renderer_, lastCapturePath_};
        AgentCommandResult result = executeCommand(std::string("script.run ") + scriptBuffer_, ctx);

        scriptOutput_ = result.output;
        if (!result.success && !result.output.empty()) {
            scriptOutput_ += result.output;
        }
        lastScriptPath_ = ctx.lastScriptPath;
        lastCapturePath_ = ctx.lastCapturePath;
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
    ImGui::Begin("Screenshot");

    ImGui::InputText("Filename", screenshotBuffer_, sizeof(screenshotBuffer_));
    ImGui::SameLine();
    if (ImGui::Button("Capture Screenshot")) {
        AgentCommandContext ctx{scene, selected_, 0, 0.0f, 0.0f, 0, lastScriptPath_, renderer_, lastCapturePath_};
        std::string command = std::string("render.capture ") + screenshotBuffer_;
        AgentCommandResult result = executeCommand(command, ctx);
        screenshotOutput_ = result.output;
        lastCapturePath_ = ctx.lastCapturePath;
    }

    if (!screenshotOutput_.empty()) {
        ImGui::TextUnformatted(screenshotOutput_.data(),
                               screenshotOutput_.data() + screenshotOutput_.size());
    }

    ImGui::End();
}

void Editor::drawDiagnostics(uint64_t frame, float fps, float frameTimeMs, size_t renderCommandCount, const Scene& scene)
{
    ImGui::SetNextWindowPos(ImVec2(10, 490), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Diagnostics");

    if (ImGui::Button("Write Debug State")) {
        std::string text = DebugState::build(frame, fps, frameTimeMs, renderCommandCount, scene, selected_, lastScriptPath_, lastCapturePath_);
        DebugState::writeToFile(text);
    }

    if (ImGui::Button("Copy Debug State")) {
        std::string text = DebugState::build(frame, fps, frameTimeMs, renderCommandCount, scene, selected_, lastScriptPath_, lastCapturePath_);
        ImGui::SetClipboardText(text.c_str());
    }

    ImGui::End();
}
