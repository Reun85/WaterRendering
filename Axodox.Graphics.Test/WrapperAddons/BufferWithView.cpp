#include "pch.h"
#include "BufferWithView.h"

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
