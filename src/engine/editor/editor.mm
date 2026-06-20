#include "editor/editor.h"
#include "scene/scene.h"
#include "scene/game_object.h"

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

void Editor::drawUI(Scene& scene, float fps, float frameTimeMs)
{
    drawHierarchy(scene, fps, frameTimeMs);
    drawInspector();
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
        if (ImGui::Selectable(obj->name().c_str(), isSelected)) {
            selected_ = obj.get();
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", obj->name().c_str());
        }
        ImGui::PopID();
    }

    ImGui::End();
}

void Editor::drawInspector()
{
    ImGui::SetNextWindowPos(ImVec2(10, 220), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    if (!selected_) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

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

    ImGui::End();
}
