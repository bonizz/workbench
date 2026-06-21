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

struct MetalRenderer::Impl
{
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLRenderPipelineState> meshPipeline = nil;
    id<MTLRenderPipelineState> gridPipeline = nil;
    id<MTLDepthStencilState> depthStencilState = nil;
    id<MTLTexture> sceneColorTexture = nil;
    id<MTLTexture> depthTexture = nil;
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
    id<MTLFunction> frag = [library newFunctionWithName:[NSString stringWithUTF8String:fragmentFunction]];
    if (!vert || !frag) {
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
    desc.colorAttachments[0].pixelFormat = colorFormat;
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

    impl_->meshPipeline = loadPipeline(impl_->device,
                                       "assets/shaders/basic.metal",
                                       "vertex_main",
                                       "fragment_main",
                                       MTLPixelFormatBGRA8Unorm,
                                       MTLPixelFormatDepth32Float,
                                       false);

    impl_->gridPipeline = loadPipeline(impl_->device,
                                       "assets/shaders/grid.metal",
                                       "grid_vertex",
                                       "grid_fragment",
                                       MTLPixelFormatBGRA8Unorm,
                                       MTLPixelFormatDepth32Float,
                                       true);

    MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthDesc.depthCompareFunction = MTLCompareFunctionGreater;
    depthDesc.depthWriteEnabled = YES;
    impl_->depthStencilState = [impl_->device newDepthStencilStateWithDescriptor:depthDesc];
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
    [impl_->depthStencilState release];
    [impl_->gridPipeline release];
    [impl_->meshPipeline release];
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
        v[0] = {{-sz, 0.0f, -sz}, {0, 0, 0, 0}};
        v[1] = {{ sz, 0.0f, -sz}, {0, 0, 0, 0}};
        v[2] = {{ sz, 0.0f,  sz}, {0, 0, 0, 0}};
        v[3] = {{-sz, 0.0f,  sz}, {0, 0, 0, 0}};
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
                ShaderUniforms uniforms = {cmd.transform, view, projection};
                [sceneEncoder setVertexBytes:&uniforms length:sizeof(uniforms)
                    atIndex:static_cast<NSUInteger>(VertexBufferIndex::Uniforms)];
                [sceneEncoder setFragmentBytes:&cmd.color length:sizeof(cmd.color)
                    atIndex:static_cast<NSUInteger>(MeshFragmentBufferIndex::Color)];
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
                [sceneEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                    indexCount:6 indexType:MTLIndexTypeUInt16
                    indexBuffer:impl_->groundIndexBuffer indexBufferOffset:0];
                break;
            }
        }
    }

    [sceneEncoder endEncoding];

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
