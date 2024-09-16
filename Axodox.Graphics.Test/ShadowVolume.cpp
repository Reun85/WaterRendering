#include "pch.h"
#include "ShadowVolume.h"
#include "Camera.h"
#include "Helpers.h"
#include "GraphicsPipeline.h"

using namespace std;
using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace Axodox::Threading;
using namespace DirectX;
using namespace DirectX::PackedVector;

SilhouetteDetector::Buffers::Buffers(const ResourceAllocationContext &context,
                                     u32 IndexCount)
    : EdgeBuffer(context.ResourceAllocator->CreateBuffer(BufferDefinition(
          IndexCount * sizeof(EdgeBufferType), BufferFlags::UnorderedAccess))),
      EdgeCountBuffer(context.ResourceAllocator->CreateBuffer(
          BufferDefinition(IndexCount * sizeof(EdgeCountBufferType),
                           BufferFlags::UnorderedAccess)))

{}

constexpr D3D12_DEPTH_STENCIL_DESC
ShadowVolume::ShaderMask::GetDepthStencilDesc() {
  D3D12_DEPTH_STENCIL_DESC result{
      .DepthEnable = true,                           // Depth testing enabled
      .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO, // Do not write to depth
      // buffer, why though?
      .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
      .StencilEnable = true,
      .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
      .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
      // Front-facing polygons
      .FrontFace =
          {
              .StencilFailOp = D3D12_STENCIL_OP_KEEP,
              .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
              .StencilPassOp =
                  D3D12_STENCIL_OP_INCR, // Increment stencil on pass
              // and wrap if necessary

              .StencilFunc =
                  D3D12_COMPARISON_FUNC_ALWAYS, // Always pass stencil test ?
          },

      // Back-facing polygons
      .BackFace =
          {
              .StencilFailOp = D3D12_STENCIL_OP_KEEP,
              .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
              .StencilPassOp =
                  D3D12_STENCIL_OP_DECR, // Decrement stencil on pass
              // and wrap if necessary
              .StencilFunc =
                  D3D12_COMPARISON_FUNC_ALWAYS, // Always pass stencil test ?
          },
  };

  return result;
}

SilhouetteClearTask::SilhouetteClearTask(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
    ComputeShader *cs)
    : Signature(device),
      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature,
                       .ComputeShader = cs,
                   })
                   .get()) {}

SilhouetteClearTask SilhouetteClearTask ::WithDefaultShaders(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device) {
  ComputeShader cs(app_folder() / L"SilhouetteClear.cso");

  return SilhouetteClearTask(pipelineProvider, device, &cs);
}

void SilhouetteClearTask::Run(CommandAllocator &allocator,
                              DynamicBufferManager &buffermanager,
                              const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Graphics);
  mask.buff =
      inp.buffer.EdgeCountBuffer.get()->get().get()->GetGPUVirtualAddress();
  ;
  allocator.Dispatch(1, 1, 1);
}

SilhouetteDetector::SilhouetteDetector(PipelineStateProvider &pipelineProvider,
                                       GraphicsDevice &device,
                                       ComputeShader *cs)
    : Signature(device),
      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature,
                       .ComputeShader = cs,
                   })
                   .get()) {}

SilhouetteDetector
SilhouetteDetector::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                       GraphicsDevice &device) {
  ComputeShader cs(app_folder() / L"SilhouetteDetector.cso");

  return SilhouetteDetector(pipelineProvider, device, &cs);
}

void SilhouetteDetector::Run(CommandAllocator &allocator,
                             DynamicBufferManager &buffermanager,
                             const Inp &inp) const {

  assert(inp.mesh.GetTopology() == D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
  u32 faceCount = inp.mesh.GetIndexCount() / 3;

  auto mask = Signature.Set(allocator, RootSignatureUsage::Graphics);

  mask.Vertex =
      inp.mesh.GetVertexBuffer().get()->get().get()->GetGPUVirtualAddress();
  mask.Index =
      inp.mesh.GetIndexBuffer().get()->get().get()->GetGPUVirtualAddress();
  mask.EdgeCount =
      inp.buffers.EdgeCountBuffer.get()->get().get()->GetGPUVirtualAddress();
  mask.Edges =
      inp.buffers.EdgeBuffer.get()->get().get()->GetGPUVirtualAddress();
  mask.lights = inp.lights;
  mask.constants = buffermanager.AddBuffer(Constants{.faceCount = faceCount});

  u32 totalFaces = faceCount;
  u32 threadsPerGroup = 256; // THREADS_PER_GROUP from the shader
  u32 numThreadGroups = (totalFaces + threadsPerGroup - 1) / threadsPerGroup;

  // Dispatch the compute shader
  allocator.Dispatch(numThreadGroups, 1, 1);
}

consteval D3D12_INPUT_ELEMENT_DESC InputElement(const char *semanticName,
                                                uint32_t semanticIndex,
                                                DXGI_FORMAT format) {
  return {.SemanticName = semanticName,
          .SemanticIndex = semanticIndex,
          .Format = format,
          .InputSlot = 0,
          .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
          .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
          .InstanceDataStepRate = 0};
}

SilhouetteDetectorTester::SilhouetteDetectorTester(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
    VertexShader *vs, PixelShader *ps)
    : Signature(device),
      pipeline(
          pipelineProvider
              .CreatePipelineStateAsync(GraphicsPipelineStateDefinition{
                  .RootSignature = &Signature,
                  .VertexShader = vs,
                  .PixelShader = ps,
                  .RasterizerState = RasterizerFlags::CullClockwise,
                  .DepthStencilState = DepthStencilMode::WriteDepth,
                  .TopologyType = PrimitiveTopologyType::Line,
                  .RenderTargetFormats = std::initializer_list(
                      std::to_address(
                          DeferredShading::GBuffer::GetGBufferFormats()
                              .begin()),
                      std::to_address(
                          DeferredShading::GBuffer::GetGBufferFormats().end())),
                  .DepthStencilFormat = Format::D32_Float})
              .get()) {}

SilhouetteDetectorTester SilhouetteDetectorTester::WithDefaultShaders(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device) {
  VertexShader vs(app_folder() / L"SilhouetteTestVS.cso");
  PixelShader ps(app_folder() / L"SilhouetteTestPS.cso");

  return SilhouetteDetectorTester(pipelineProvider, device, &vs, &ps);
}

void SilhouetteDetectorTester::Run(CommandAllocator &allocator,
                                   DynamicBufferManager &buffermanager,
                                   const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Graphics);

  // Indirect rendering??
  // TODO: do this
}
