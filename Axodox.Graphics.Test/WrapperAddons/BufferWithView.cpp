#include "pch.h"
#include "BufferWithView.h"

Axodox::Graphics::D3D12::StructuredObjectViews::StructuredObjectViews(
    ResourceAllocationContext &context, const Buffer *br,
    const BufferViewDefinitions &def) {
  if (def.ShaderResource) {
    srv = context.CommonDescriptorHeap->CreateDescriptor<ShaderResourceView>(
        br, *def.ShaderResource);
  }
  if (def.UnorderedAccess) {
    uav = context.CommonDescriptorHeap->CreateDescriptor<UnorderedAccessView>(
        br, *def.UnorderedAccess);
  }
}