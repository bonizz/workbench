#include "renderer/metal_renderer.h"
#include "renderer/mesh_geometry.h"
#include "renderer/render_types.h"
#include "capture/capture.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Fixed shadow-map resolution for v1. Independent of the window size, so it is
// allocated once in the constructor rather than in resize().
static constexpr NSUInteger kShadowMapSize = 1024;
static constexpr float kShadowBias = 0.0015f;

struct MetalRenderer::Impl
{
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLRenderPipelineState> meshPipeline = nil;
    id<MTLRenderPipelineState> gridPipeline = nil;
    id<MTLRenderPipelineState> skyPipeline = nil;
    id<MTLRenderPipelineState> shadowPipeline = nil;
    id<MTLRenderPipelineState> shadowDebugPipeline = nil;
    id<MTLDepthStencilState> depthStencilState = nil;
    id<MTLDepthStencilState> skyDepthState = nil;
    id<MTLTexture> sceneColorTexture = nil;
    id<MTLTexture> depthTexture = nil;
    id<MTLTexture> shadowMapTexture = nil;
    id<MTLTexture> shadowDebugTexture = nil;
    id<MTLBuffer> groundVertexBuffer = nil;
    id<MTLBuffer> groundIndexBuffer = nil;
    id<MTLBuffer> cubeVertexBuffer = nil;
    id<MTLBuffer> cubeIndexBuffer = nil;
    id<MTLBuffer> sphereVertexBuffer = nil;
    id<MTLBuffer> sphereIndexBuffer = nil;
    id<MTLBuffer> planeVertexBuffer = nil;
    id<MTLBuffer> planeIndexBuffer = nil;
    NSUInteger cubeIndexCount = 0;
    NSUInteger sphereIndexCount = 0;
    NSUInteger planeIndexCount = 0;
    float aspectRatio = 1.0f;

    std::string pendingScreenshotPath;
};

static void uploadMesh(id<MTLDevice> device, const MeshData& data,
                       id<MTLBuffer>& vbuf, id<MTLBuffer>& ibuf, NSUInteger& indexCount)
{
    NSUInteger vlen = sizeof(Vertex) * data.vertices.size();
    NSUInteger ilen = sizeof(uint16_t) * data.indices.size();

    vbuf = [device newBufferWithBytes:data.vertices.data() length:vlen options:MTLResourceStorageModeShared];
    ibuf = [device newBufferWithBytes:data.indices.data() length:ilen options:MTLResourceStorageModeShared];
    indexCount = data.indices.size();
}

static id<MTLRenderPipelineState> loadPipeline(id<MTLDevice> device,
                                               const char* shaderPath,
                                               const char* vertexFunction,
                                               const char* fragmentFunction,
                                               MTLPixelFormat colorFormat,
                                               MTLPixelFormat depthFormat,
                                               bool blend)
{
    NSError* error = nil;
    NSString* path = [NSString stringWithUTF8String:shaderPath];
    NSString* source = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&error];
    if (!source) {
        NSLog(@"Failed to load shader %s: %@", shaderPath, error);
        return nil;
    }

    id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
    if (!library) {
        NSLog(@"Failed to compile shader %s: %@", shaderPath, error);
        return nil;
    }

    id<MTLFunction> vert = [library newFunctionWithName:[NSString stringWithUTF8String:vertexFunction]];
    // A null fragmentFunction is valid for a depth-only pass (the shadow map).
    id<MTLFunction> frag = fragmentFunction
        ? [library newFunctionWithName:[NSString stringWithUTF8String:fragmentFunction]]
        : nil;
    if (!vert || (fragmentFunction && !frag)) {
        NSLog(@"Missing shader function in %s", shaderPath);
        [vert release];
        [frag release];
        [library release];
        return nil;
    }

    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vert;
    desc.fragmentFunction = frag;
    desc.rasterSampleCount = 1;
    if (colorFormat != MTLPixelFormatInvalid) {
        desc.colorAttachments[0].pixelFormat = colorFormat;
    }
    if (blend) {
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    }
    if (depthFormat != MTLPixelFormatInvalid) {
        desc.depthAttachmentPixelFormat = depthFormat;
    }

    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline) {
        NSLog(@"Failed to create pipeline for %s: %@", shaderPath, error);
    }

    [desc release];
    [vert release];
    [frag release];
    [library release];
    return pipeline;
}

MetalRenderer::MetalRenderer(void* nativeMetalLayer)
    : impl_(new Impl())
{
    impl_->device = [MTLCreateSystemDefaultDevice() retain];
    if (!impl_->device) {
        NSLog(@"No Metal device found");
        return;
    }

    impl_->commandQueue = [impl_->device newCommandQueue];
    impl_->layer = [(CAMetalLayer*)nativeMetalLayer retain];
    impl_->layer.device = impl_->device;
    impl_->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    impl_->layer.framebufferOnly = NO;

    // Blending is enabled so the translucent selection-highlight overlay reads
    // over the base mesh. Opaque meshes use alpha 1.0, for which blending is a
    // no-op (src*1 + dst*0 = src).
    impl_->meshPipeline = loadPipeline(impl_->device,
                                       "assets/shaders/basic.metal",
                                       "vertex_main",
                                       "fragment_main",
                                       MTLPixelFormatBGRA8Unorm,
                                       MTLPixelFormatDepth32Float,
                                       true);

    impl_->gridPipeline = loadPipeline(impl_->device,
                                       "assets/shaders/grid.metal",
                                       "grid_vertex",
                                       "grid_fragment",
                                       MTLPixelFormatBGRA8Unorm,
                                       MTLPixelFormatDepth32Float,
                                       true);

    impl_->skyPipeline = loadPipeline(impl_->device,
                                      "assets/shaders/sky.metal",
                                      "sky_vertex",
                                      "sky_fragment",
                                      MTLPixelFormatBGRA8Unorm,
                                      MTLPixelFormatDepth32Float,
                                      false);

    // Depth-only shadow pass: no color attachment, no fragment function.
    impl_->shadowPipeline = loadPipeline(impl_->device,
                                         "assets/shaders/shadow.metal",
                                         "shadow_vertex",
                                         nullptr,
                                         MTLPixelFormatInvalid,
                                         MTLPixelFormatDepth32Float,
                                         false);

    // Debug copy: shadow depth -> grayscale color for ImGui display.
    impl_->shadowDebugPipeline = loadPipeline(impl_->device,
                                              "assets/shaders/shadow_debug.metal",
                                              "shadow_debug_vertex",
                                              "shadow_debug_fragment",
                                              MTLPixelFormatBGRA8Unorm,
                                              MTLPixelFormatInvalid,
                                              false);

    // Shadow map (depth) and its debug color copy. Fixed size, allocated once.
    MTLTextureDescriptor* shadowDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                          width:kShadowMapSize
                                                                                         height:kShadowMapSize
                                                                                      mipmapped:NO];
    shadowDesc.storageMode = MTLStorageModePrivate;
    shadowDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    impl_->shadowMapTexture = [impl_->device newTextureWithDescriptor:shadowDesc];

    MTLTextureDescriptor* shadowDebugDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                               width:kShadowMapSize
                                                                                              height:kShadowMapSize
                                                                                           mipmapped:NO];
    shadowDebugDesc.storageMode = MTLStorageModePrivate;
    shadowDebugDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    impl_->shadowDebugTexture = [impl_->device newTextureWithDescriptor:shadowDebugDesc];

    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionGreater;
    depthDesc.depthWriteEnabled = YES;
    impl_->depthStencilState = [impl_->device newDepthStencilStateWithDescriptor:depthDesc];

    depthDesc.depthCompareFunction = MTLCompareFunctionAlways;
    depthDesc.depthWriteEnabled = NO;
    impl_->skyDepthState = [impl_->device newDepthStencilStateWithDescriptor:depthDesc];
    [depthDesc release];
}

MetalRenderer::~MetalRenderer()
{
    [impl_->planeIndexBuffer release];
    [impl_->planeVertexBuffer release];
    [impl_->sphereIndexBuffer release];
    [impl_->sphereVertexBuffer release];
    [impl_->cubeIndexBuffer release];
    [impl_->cubeVertexBuffer release];
    [impl_->groundIndexBuffer release];
    [impl_->groundVertexBuffer release];
    [impl_->sceneColorTexture release];
    [impl_->depthTexture release];
    [impl_->shadowMapTexture release];
    [impl_->shadowDebugTexture release];
    [impl_->depthStencilState release];
    [impl_->skyDepthState release];
    [impl_->gridPipeline release];
    [impl_->meshPipeline release];
    [impl_->skyPipeline release];
    [impl_->shadowPipeline release];
    [impl_->shadowDebugPipeline release];
    [impl_->commandQueue release];
    [impl_->device release];
    [impl_->layer release];
    delete impl_;
}

void MetalRenderer::resize(float width, float height, float scale)
{
    CGSize drawableSize = CGSizeMake(width * scale, height * scale);
    impl_->layer.drawableSize = drawableSize;
    impl_->aspectRatio = (height > 0.0f) ? (width / height) : 1.0f;

    MTLTextureDescriptor* colorDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                            width:(NSUInteger)drawableSize.width
                                                                                           height:(NSUInteger)drawableSize.height
                                                                                        mipmapped:NO];
    colorDesc.sampleCount = 1;
    colorDesc.storageMode = MTLStorageModePrivate;
    colorDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    [impl_->sceneColorTexture release];
    impl_->sceneColorTexture = [impl_->device newTextureWithDescriptor:colorDesc];

    MTLTextureDescriptor* depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                            width:(NSUInteger)drawableSize.width
                                                                                           height:(NSUInteger)drawableSize.height
                                                                                        mipmapped:NO];
    depthDesc.sampleCount = 1;
    depthDesc.storageMode = MTLStorageModePrivate;
    depthDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    [impl_->depthTexture release];
    impl_->depthTexture = [impl_->device newTextureWithDescriptor:depthDesc];
}

float MetalRenderer::aspectRatio() const
{
    return impl_->aspectRatio;
}

void* MetalRenderer::device() const
{
    return (void*)impl_->device;
}

void* MetalRenderer::shadowMapTexture() const
{
    return (void*)impl_->shadowDebugTexture;
}

void MetalRenderer::requestScreenshot(const std::string& path)
{
    impl_->pendingScreenshotPath = path;
}

void MetalRenderer::draw(const RenderContext& rc, const float clearColor[4], const UIRenderCallback& uiCallback)
{
    if (!impl_->device || !impl_->commandQueue || !impl_->meshPipeline) {
        return;
    }

    if (!impl_->sceneColorTexture) {
        CGSize size = impl_->layer.drawableSize;
        MTLTextureDescriptor* colorDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                                width:(NSUInteger)size.width
                                                                                               height:(NSUInteger)size.height
                                                                                            mipmapped:NO];
        colorDesc.storageMode = MTLStorageModePrivate;
        colorDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        impl_->sceneColorTexture = [impl_->device newTextureWithDescriptor:colorDesc];
    }

    if (!impl_->cubeVertexBuffer) {
        uploadMesh(impl_->device, makeCube(),
                   impl_->cubeVertexBuffer, impl_->cubeIndexBuffer, impl_->cubeIndexCount);
    }

    if (!impl_->sphereVertexBuffer) {
        uploadMesh(impl_->device, makeSphere(),
                   impl_->sphereVertexBuffer, impl_->sphereIndexBuffer, impl_->sphereIndexCount);
    }

    if (!impl_->planeVertexBuffer) {
        uploadMesh(impl_->device, makePlane(),
                   impl_->planeVertexBuffer, impl_->planeIndexBuffer, impl_->planeIndexCount);
    }

    if (!impl_->groundVertexBuffer) {
        NSUInteger len = sizeof(Vertex) * 4;
        impl_->groundVertexBuffer = [impl_->device newBufferWithLength:len options:MTLResourceStorageModeShared];
        Vertex* v = static_cast<Vertex*>([impl_->groundVertexBuffer contents]);
        float sz = 50.0f;
        v[0] = {{-sz, 0.0f, -sz}, {0.0f, 1.0f, 0.0f}};
        v[1] = {{ sz, 0.0f, -sz}, {0.0f, 1.0f, 0.0f}};
        v[2] = {{ sz, 0.0f,  sz}, {0.0f, 1.0f, 0.0f}};
        v[3] = {{-sz, 0.0f,  sz}, {0.0f, 1.0f, 0.0f}};
    }

    if (!impl_->groundIndexBuffer) {
        NSUInteger len = sizeof(uint16_t) * 6;
        impl_->groundIndexBuffer = [impl_->device newBufferWithLength:len options:MTLResourceStorageModeShared];
        uint16_t indices[6] = {0, 1, 2, 0, 2, 3};
        memcpy([impl_->groundIndexBuffer contents], indices, len);
    }

    id<CAMetalDrawable> drawable = [impl_->layer nextDrawable];
    if (!drawable) {
        return;
    }

    id<MTLCommandBuffer> commandBuffer = [impl_->commandQueue commandBuffer];

    // Per-frame shadow data shared by the mesh and grid fragment shaders.
    ShadowData shadowData;
    shadowData.lightViewProjection = rc.lightViewProjection();
    shadowData.bias = kShadowBias;
    shadowData.enabled = rc.shadowsEnabled() ? 1.0f : 0.0f;

    // Shadow depth pass: render occluder meshes from the light's point of view
    // into the depth-only shadow map. Reuses the reversed-Z depth state (clear
    // 0.0 = far, compare Greater). Runs before the scene pass; Metal resolves
    // the render-target -> shader-read hazard between encoders automatically.
    if (impl_->shadowPipeline && impl_->shadowMapTexture) {
        MTLRenderPassDescriptor* shadowPass = [MTLRenderPassDescriptor renderPassDescriptor];
        shadowPass.depthAttachment.texture = impl_->shadowMapTexture;
        shadowPass.depthAttachment.loadAction = MTLLoadActionClear;
        shadowPass.depthAttachment.storeAction = MTLStoreActionStore;
        shadowPass.depthAttachment.clearDepth = 0.0;

        id<MTLRenderCommandEncoder> shadowEncoder = [commandBuffer renderCommandEncoderWithDescriptor:shadowPass];
        [shadowEncoder setDepthStencilState:impl_->depthStencilState];
        [shadowEncoder setRenderPipelineState:impl_->shadowPipeline];

        for (const auto& cmd : rc.commands()) {
            id<MTLBuffer> vbuf = nil;
            id<MTLBuffer> ibuf = nil;
            NSUInteger indexCount = 0;
            switch (cmd.type) {
                case ShapeType::Cube:
                    vbuf = impl_->cubeVertexBuffer; ibuf = impl_->cubeIndexBuffer; indexCount = impl_->cubeIndexCount; break;
                case ShapeType::Sphere:
                    vbuf = impl_->sphereVertexBuffer; ibuf = impl_->sphereIndexBuffer; indexCount = impl_->sphereIndexCount; break;
                case ShapeType::Plane:
                    vbuf = impl_->planeVertexBuffer; ibuf = impl_->planeIndexBuffer; indexCount = impl_->planeIndexCount; break;
                default:
                    break; // Ground and other shapes do not cast shadows.
            }
            if (!vbuf || !ibuf) continue;
            [shadowEncoder setVertexBuffer:vbuf offset:0
                atIndex:static_cast<NSUInteger>(VertexBufferIndex::Vertices)];
            ShadowVertexUniforms su = {cmd.transform, rc.lightViewProjection()};
            [shadowEncoder setVertexBytes:&su length:sizeof(su)
                atIndex:static_cast<NSUInteger>(VertexBufferIndex::Uniforms)];
            [shadowEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                indexCount:indexCount indexType:MTLIndexTypeUInt16
                indexBuffer:ibuf indexBufferOffset:0];
        }

        [shadowEncoder endEncoding];
    }

    // Scene pass: render 3D content to offscreen color + depth.
    MTLRenderPassDescriptor* scenePass = [MTLRenderPassDescriptor renderPassDescriptor];
    scenePass.defaultRasterSampleCount = 1;
    scenePass.colorAttachments[0].texture = impl_->sceneColorTexture;
    scenePass.colorAttachments[0].loadAction = MTLLoadActionClear;
    scenePass.colorAttachments[0].storeAction = MTLStoreActionStore;
    scenePass.colorAttachments[0].clearColor = MTLClearColorMake(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    scenePass.depthAttachment.texture = impl_->depthTexture;
    scenePass.depthAttachment.loadAction = MTLLoadActionClear;
    scenePass.depthAttachment.storeAction = MTLStoreActionStore;
    scenePass.depthAttachment.clearDepth = 0.0;

    id<MTLRenderCommandEncoder> sceneEncoder = [commandBuffer renderCommandEncoderWithDescriptor:scenePass];
    [sceneEncoder setDepthStencilState:impl_->depthStencilState];

    Mat4 view = rc.view();
    Mat4 projection = rc.projection();

    // Draw the procedural sky first so it fills the background without
    // interfering with depth-tested geometry.
    if (impl_->skyPipeline && impl_->skyDepthState) {
        [sceneEncoder setRenderPipelineState:impl_->skyPipeline];
        [sceneEncoder setDepthStencilState:impl_->skyDepthState];

        Mat4 invViewProj = inverseMatrix(multiply(projection, view));
        Mat4 invView = inverseMatrix(view);
        simd::float4 cameraCol = invView.columns[3];
        simd::float3 cameraPos = {cameraCol.x, cameraCol.y, cameraCol.z};
        simd::float3 sunDir = simd::normalize(-rc.light().direction);

        [sceneEncoder setFragmentBytes:&rc.sky() length:sizeof(SkySettings)
            atIndex:static_cast<NSUInteger>(SkyFragmentBufferIndex::SkySettings)];
        [sceneEncoder setFragmentBytes:&invViewProj length:sizeof(simd::float4x4)
            atIndex:static_cast<NSUInteger>(SkyFragmentBufferIndex::InvViewProj)];
        [sceneEncoder setFragmentBytes:&cameraPos length:sizeof(simd::float3)
            atIndex:static_cast<NSUInteger>(SkyFragmentBufferIndex::CameraPos)];
        [sceneEncoder setFragmentBytes:&sunDir length:sizeof(simd::float3)
            atIndex:static_cast<NSUInteger>(SkyFragmentBufferIndex::SunDir)];
        [sceneEncoder drawPrimitives:MTLPrimitiveTypeTriangle
            vertexStart:0 vertexCount:3];

        [sceneEncoder setDepthStencilState:impl_->depthStencilState];
    }

    for (const auto& cmd : rc.commands()) {
        switch (cmd.type) {
            case ShapeType::Cube:
            case ShapeType::Sphere:
            case ShapeType::Plane: {
                id<MTLBuffer> vbuf = nil;
                id<MTLBuffer> ibuf = nil;
                NSUInteger indexCount = 0;
                switch (cmd.type) {
                    case ShapeType::Cube:
                        vbuf = impl_->cubeVertexBuffer;
                        ibuf = impl_->cubeIndexBuffer;
                        indexCount = impl_->cubeIndexCount;
                        break;
                    case ShapeType::Sphere:
                        vbuf = impl_->sphereVertexBuffer;
                        ibuf = impl_->sphereIndexBuffer;
                        indexCount = impl_->sphereIndexCount;
                        break;
                    case ShapeType::Plane:
                        vbuf = impl_->planeVertexBuffer;
                        ibuf = impl_->planeIndexBuffer;
                        indexCount = impl_->planeIndexCount;
                        break;
                    default:
                        break;
                }
                if (!vbuf || !ibuf) break;
                [sceneEncoder setRenderPipelineState:impl_->meshPipeline];
                [sceneEncoder setVertexBuffer:vbuf offset:0
                    atIndex:static_cast<NSUInteger>(VertexBufferIndex::Vertices)];
                ShaderUniforms uniforms = {cmd.transform, view, projection, normalMatrix(cmd.transform)};
                [sceneEncoder setVertexBytes:&uniforms length:sizeof(uniforms)
                    atIndex:static_cast<NSUInteger>(VertexBufferIndex::Uniforms)];
                [sceneEncoder setFragmentBytes:&cmd.color length:sizeof(cmd.color)
                    atIndex:static_cast<NSUInteger>(MeshFragmentBufferIndex::Color)];
                [sceneEncoder setFragmentBytes:&rc.light() length:sizeof(rc.light())
                    atIndex:static_cast<NSUInteger>(MeshFragmentBufferIndex::Light)];
                [sceneEncoder setFragmentBytes:&shadowData length:sizeof(shadowData)
                    atIndex:static_cast<NSUInteger>(MeshFragmentBufferIndex::Shadow)];
                [sceneEncoder setFragmentTexture:impl_->shadowMapTexture
                    atIndex:static_cast<NSUInteger>(FragmentTextureIndex::ShadowMap)];
                [sceneEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                    indexCount:indexCount indexType:MTLIndexTypeUInt16
                    indexBuffer:ibuf indexBufferOffset:0];
                break;
            }
            case ShapeType::Ground: {
                if (!impl_->gridPipeline) break;
                [sceneEncoder setRenderPipelineState:impl_->gridPipeline];
                [sceneEncoder setVertexBuffer:impl_->groundVertexBuffer offset:0
                    atIndex:static_cast<NSUInteger>(VertexBufferIndex::Vertices)];
                ShaderUniforms gridUniforms = {cmd.transform, view, projection};
                [sceneEncoder setVertexBytes:&gridUniforms length:sizeof(gridUniforms)
                    atIndex:static_cast<NSUInteger>(VertexBufferIndex::Uniforms)];
                [sceneEncoder setFragmentBytes:&cmd.gridScale length:sizeof(cmd.gridScale)
                    atIndex:static_cast<NSUInteger>(FragmentBufferIndex::GridScale)];
                [sceneEncoder setFragmentBytes:&cmd.gridMajorDiv length:sizeof(cmd.gridMajorDiv)
                    atIndex:static_cast<NSUInteger>(FragmentBufferIndex::GridMajorDiv)];
                simd::float3 camSimd = {cmd.cameraPos[0], cmd.cameraPos[1], cmd.cameraPos[2]};
                [sceneEncoder setFragmentBytes:&camSimd length:sizeof(camSimd)
                    atIndex:static_cast<NSUInteger>(FragmentBufferIndex::CameraPos)];
                [sceneEncoder setFragmentBytes:&shadowData length:sizeof(shadowData)
                    atIndex:static_cast<NSUInteger>(FragmentBufferIndex::Shadow)];
                [sceneEncoder setFragmentTexture:impl_->shadowMapTexture
                    atIndex:static_cast<NSUInteger>(FragmentTextureIndex::ShadowMap)];
                [sceneEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                    indexCount:6 indexType:MTLIndexTypeUInt16
                    indexBuffer:impl_->groundIndexBuffer indexBufferOffset:0];
                break;
            }
        }
    }

    [sceneEncoder endEncoding];

    // Shadow-map debug copy: render the depth map as grayscale into a color
    // texture so the editor can display it with ImGui::Image.
    if (impl_->shadowDebugPipeline && impl_->shadowDebugTexture && impl_->shadowMapTexture) {
        MTLRenderPassDescriptor* debugPass = [MTLRenderPassDescriptor renderPassDescriptor];
        debugPass.colorAttachments[0].texture = impl_->shadowDebugTexture;
        debugPass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        debugPass.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> debugEncoder = [commandBuffer renderCommandEncoderWithDescriptor:debugPass];
        [debugEncoder setRenderPipelineState:impl_->shadowDebugPipeline];
        [debugEncoder setFragmentTexture:impl_->shadowMapTexture
            atIndex:static_cast<NSUInteger>(FragmentTextureIndex::ShadowMap)];
        [debugEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [debugEncoder endEncoding];
    }

    // Blit offscreen scene color to the window drawable.
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    MTLSize copySize = MTLSizeMake(impl_->sceneColorTexture.width, impl_->sceneColorTexture.height, 1);
    [blitEncoder copyFromTexture:impl_->sceneColorTexture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:copySize
                       toTexture:drawable.texture
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blitEncoder endEncoding];

    // UI pass: render ImGui on top of the drawable.
    if (uiCallback) {
        MTLRenderPassDescriptor* uiPass = [MTLRenderPassDescriptor renderPassDescriptor];
        uiPass.defaultRasterSampleCount = 1;
        uiPass.colorAttachments[0].texture = drawable.texture;
        uiPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
        uiPass.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> uiEncoder = [commandBuffer renderCommandEncoderWithDescriptor:uiPass];
        uiCallback((void*)commandBuffer, (void*)uiEncoder, (void*)uiPass);
        [uiEncoder endEncoding];
    }

    // Screenshot capture reads back the final drawable after the UI pass.
    if (!impl_->pendingScreenshotPath.empty()) {
        NSUInteger captureWidth = drawable.texture.width;
        NSUInteger captureHeight = drawable.texture.height;

        MTLTextureDescriptor* stagingDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:captureWidth
            height:captureHeight
            mipmapped:NO];
        stagingDesc.storageMode = MTLStorageModeShared;
        stagingDesc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> staging = [impl_->device newTextureWithDescriptor:stagingDesc];

        id<MTLBlitCommandEncoder> captureBlit = [commandBuffer blitCommandEncoder];
        [captureBlit copyFromTexture:drawable.texture
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:MTLOriginMake(0, 0, 0)
                          sourceSize:MTLSizeMake(captureWidth, captureHeight, 1)
                           toTexture:staging
                    destinationSlice:0
                    destinationLevel:0
                   destinationOrigin:MTLOriginMake(0, 0, 0)];
        [captureBlit endEncoding];

        std::string capturePath = std::move(impl_->pendingScreenshotPath);
        impl_->pendingScreenshotPath.clear();

        __block id<MTLTexture> blockStaging = [staging retain];
        [staging release];
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
            NSUInteger w = blockStaging.width;
            NSUInteger h = blockStaging.height;
            NSUInteger bytesPerRow = w * 4;
            std::vector<uint8_t> bgra(bytesPerRow * h);
            MTLRegion region = MTLRegionMake2D(0, 0, w, h);
            [blockStaging getBytes:bgra.data()
                       bytesPerRow:bytesPerRow
                        fromRegion:region
                       mipmapLevel:0];

            std::vector<uint8_t> rgba(bgra.size());
            for (size_t i = 0; i < w * h; ++i) {
                rgba[i * 4 + 0] = bgra[i * 4 + 2];
                rgba[i * 4 + 1] = bgra[i * 4 + 1];
                rgba[i * 4 + 2] = bgra[i * 4 + 0];
                rgba[i * 4 + 3] = bgra[i * 4 + 3];
            }

            Capture::writePNG(capturePath, rgba.data(), static_cast<int>(w), static_cast<int>(h));
            [blockStaging release];
        }];
    }

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}
