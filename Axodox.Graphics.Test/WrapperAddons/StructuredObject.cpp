#include "pch.h"
#include "BufferWithView.h"
#include "StructuredObject.h"

Axodox::Graphics::D3D12::StructuredObjectViews::StructuredObjectViews(
    ResourceAllocationContext &context, Buffer *const br,
    const BufferViewDefinitions &def) {
  allocatedSubscription = br->Allocated([this, def,
                                         &context](Resource *resource) {
    if (def.ShaderResource) {
      const D3D12_SHADER_RESOURCE_VIEW_DESC &desc = *def.ShaderResource;

      srv = context.CommonDescriptorHeap->CreateDescriptor<ShaderResourceView>(
          resource->get(), &desc);
    }
    if (def.UnorderedAccess) {
      const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc = *def.UnorderedAccess;
      uav = context.CommonDescriptorHeap->CreateDescriptor<UnorderedAccessView>(
          resource->get(), &desc);
    }
  });
}

Axodox::Graphics::D3D12::StructuredObjectViews::StructuredObjectViews(
    ResourceAllocationContext &context, Buffer *const br,
    std::function<BufferViewDefinitions(Resource *)> fn) {
  allocatedSubscription = br->Allocated([this, fn,
                                         &context](Resource *resource) {
    auto def = fn(resource);
    if (def.ShaderResource) {
      const D3D12_SHADER_RESOURCE_VIEW_DESC &desc = *def.ShaderResource;

      srv = context.CommonDescriptorHeap->CreateDescriptor<ShaderResourceView>(
          resource->get(), &desc);
    }
    if (def.UnorderedAccess) {
      const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc = *def.UnorderedAccess;
      uav = context.CommonDescriptorHeap->CreateDescriptor<UnorderedAccessView>(
          resource->get(), &desc);
    }
  });
}

MeshSpecificBuffers::MeshSpecificBuffers(
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

MeshSpecificBuffers::MeshSpecificBuffers(
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

StructuredObject::StructuredObject(
    ResourceAllocationContext &context,
    const BufferDefinition &bufferDefinitions,
                                   const BufferViewDefinitions &viewDefinitions)
    : _buff(context.ResourceAllocator->CreateBuffer(bufferDefinitions)),
      StructuredObjectViews(context, &*_buff, viewDefinitions)

{}
