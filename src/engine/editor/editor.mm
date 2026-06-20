#include "editor/editor.h"
#include "scene/scene.h"

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

void Editor::drawPanels(Scene& scene, float fps, float frameTimeMs)
{
    (void)scene;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Workbench");
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame time: %.3f ms", frameTimeMs);

    const Vec3& pos = scene.camera().position;
    ImGui::Text("Camera: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
    ImGui::End();
}
