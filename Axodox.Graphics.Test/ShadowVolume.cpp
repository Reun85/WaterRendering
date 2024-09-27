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

SilhouetteDetector::Buffers::Buffers(ResourceAllocationContext &context,
                                     u32 IndexCount)
    : EdgeBuffer(context.ResourceAllocator->CreateBuffer(BufferDefinition(
          IndexCount * sizeof(EdgeBufferType), BufferFlags::UnorderedAccess))),
      EdgeCountBuffer(context.ResourceAllocator->CreateBuffer(
          BufferDefinition(IndexCount * sizeof(EdgeCountBufferType),
                           BufferFlags::UnorderedAccess))),

      Edge(context, &*EdgeBuffer,
           BufferViewDefinitions{
               .ShaderResource =
                   D3D12_SHADER_RESOURCE_VIEW_DESC{
                       .Format = DXGI_FORMAT_UNKNOWN,
                       .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                       .Shader4ComponentMapping =
                           D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                       .Buffer = {.FirstElement = 0,
                                  .NumElements = IndexCount,
                                  .StructureByteStride =
                                      sizeof(EdgeBufferType)}},
               .UnorderedAccess =
                   D3D12_UNORDERED_ACCESS_VIEW_DESC{
                       .Format = DXGI_FORMAT_UNKNOWN,
                       .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                       .Buffer = {.FirstElement = 0,
                                  .NumElements = IndexCount,
                                  .StructureByteStride =
                                      sizeof(EdgeBufferType)}}}),
      EdgeCount(context, &*EdgeCountBuffer,
                BufferViewDefinitions{
                    .ShaderResource =
                        D3D12_SHADER_RESOURCE_VIEW_DESC{
                            .Format = DXGI_FORMAT_UNKNOWN,
                            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                            .Shader4ComponentMapping =
                                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                            .Buffer = {.FirstElement = 0,
                                       .NumElements = 1,
                                       .StructureByteStride =
                                           sizeof(EdgeCountBufferType)}},
                    .UnorderedAccess =
                        D3D12_UNORDERED_ACCESS_VIEW_DESC{
                            .Format = DXGI_FORMAT_UNKNOWN,
                            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
                            .Buffer = {.FirstElement = 0,
                                       .NumElements = 1,
                                       .StructureByteStride =
                                           sizeof(EdgeCountBufferType)}}})

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
  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);
  mask.buff = *inp.buffers.EdgeCount.UnorderedAccess();

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
  assert(inp.mesh.GetTopology() == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  u32 faceCount = inp.mesh.GetIndexCount() / 3;

  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);

  mask.Vertex = *inp.meshBuffers.Vertex.ShaderResource();
  mask.Index = *inp.meshBuffers.Index.ShaderResource();
  mask.EdgeCount = *inp.buffers.EdgeCount.UnorderedAccess();
  mask.Edges = *inp.buffers.Edge.UnorderedAccess();
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
              .get()) {
  // Create command signature for ExecuteIndirect
  D3D12_INDIRECT_ARGUMENT_DESC indirectArgDesc = {};
  indirectArgDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

  D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
  commandSignatureDesc.ByteStride =
      sizeof(SilhouetteDetector::Buffers::EdgeCountBufferType);
  commandSignatureDesc.NumArgumentDescs = 1;
  commandSignatureDesc.pArgumentDescs = &indirectArgDesc;

  device->CreateCommandSignature(&commandSignatureDesc, Signature.get(),
                                 IID_PPV_ARGS(&indirectCommandSignature));
}

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

  mask.camera = inp.camera;
  mask.model = inp.modelTransform;
  if (inp.texture)
    mask.texture = *inp.texture;
  mask.Vertex =
      inp.mesh.GetVertexBuffer().get()->get().get()->GetGPUVirtualAddress();

  mask.Edges =
      inp.buffers.EdgeBuffer.get()->get().get()->GetGPUVirtualAddress();

  allocator->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Set descriptor tables done automatically
  /*
  allocator->SetGraphicsRootDescriptorTable(0, vertexBufferSrvHandle);
  allocator->SetGraphicsRootDescriptorTable(1, edgeBufferSrvHandle);
 // Set constant buffer
  allocator->SetGraphicsRootConstantBufferView(
      2, worldViewProjectionBufferGPUAddress);

  */

  // Execute indirect draw
  // allocator->ExecuteIndirect(indirectCommandSignature.get(),
  //                           1, // Max command count
  //                           inp.buffers.EdgeCountBuffer.get()->get().get(),
  //                           0,       // Buffer offset
  //                           nullptr, // Count buffer (optional)
  //                           0        // Count buffer offset
  //);

  allocator->IASetVertexBuffers(0, 0, nullptr);
  allocator->IASetIndexBuffer(nullptr);
  allocator->DrawInstanced(3, 1, 0, 0);
}

SilhouetteDetector::MeshSpecificBuffers::MeshSpecificBuffers(
    ResourceAllocationContext &context, const ImmutableMesh &mesh,
    u32 VertexByteSize, u32 IndexByteSize)

    : Vertex(context, &*mesh.GetVertexBuffer(),
             BufferViewDefinitions{
                 .ShaderResource =
                     D3D12_SHADER_RESOURCE_VIEW_DESC{
                         .Format = DXGI_FORMAT_UNKNOWN,
                         .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                         .Shader4ComponentMapping =
                             D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                         .Buffer = {.FirstElement = 0,
                                    .NumElements = mesh.GetVertexCount(),
                                    .StructureByteStride = VertexByteSize}},
             }),
      Index(context, &*mesh.GetIndexBuffer(),
            BufferViewDefinitions{
                .ShaderResource =
                    D3D12_SHADER_RESOURCE_VIEW_DESC{
                        .Format = DXGI_FORMAT_UNKNOWN,
                        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                        .Shader4ComponentMapping =
                            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                        .Buffer = {.FirstElement = 0,
                                   .NumElements = mesh.GetIndexCount(),
                                   .StructureByteStride = IndexByteSize}},
            })

{}

SilhouetteDetector::MeshSpecificBuffers::MeshSpecificBuffers(
    ResourceAllocationContext &context, const ImmutableMesh &mesh)

    : Vertex(context, &*mesh.GetVertexBuffer(),
             BufferViewDefinitions{
                 .ShaderResource =
                     D3D12_SHADER_RESOURCE_VIEW_DESC{
                         .Format = DXGI_FORMAT_UNKNOWN,
                         .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                         .Shader4ComponentMapping =
                             D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                         .Buffer =
                             {.FirstElement = 0,
                              .NumElements = mesh.GetVertexCount(),
                              .StructureByteStride =
                                  // Length of the buffer divided by the count

                              (u32)(mesh.GetVertexBuffer()
                                        .get()
                                        ->Definition()
                                        .Length /
                                    (u64)mesh.GetVertexCount())}},
             }),
      Index(context, &*mesh.GetIndexBuffer(),
            BufferViewDefinitions{
                .ShaderResource =
                    D3D12_SHADER_RESOURCE_VIEW_DESC{
                        .Format = DXGI_FORMAT_UNKNOWN,
                        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                        .Shader4ComponentMapping =
                            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                        .Buffer = {.FirstElement = 0,
                                   .NumElements = mesh.GetIndexCount(),
                                   .StructureByteStride =
                                       (u32)(mesh.GetIndexBuffer()
                                                 .get()
                                                 ->Definition()
                                                 .Length /
                                             (u64)mesh.GetIndexCount())}},
            })

{}