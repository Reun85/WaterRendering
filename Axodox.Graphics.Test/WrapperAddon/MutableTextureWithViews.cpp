#include "pch.h"
#include "MutableTextureWithViews.h"
#include "Infrastructure/BitwiseOperations.h"

using namespace Axodox::Infrastructure;
using namespace std;

namespace Axodox::Graphics::D3D12 {
MutableTextureWithViews::MutableTextureWithViews(
    const ResourceAllocationContext &context)
    : MutableTexture(context) {}

MutableTextureWithViews::MutableTextureWithViews(
    const ResourceAllocationContext &context,
    const TextureDefinition &definition,
    const std::optional<TextureViewDefinitions> &viewDefinitions)
    : MutableTextureWithViews(context) {
  Allocate(definition, viewDefinitions);
}

MutableTextureWithViews::MutableTextureWithViews(
    const ResourceAllocationContext &context, const TextureData &textureData,
    const std::optional<TextureViewDefinitions> &viewDefinitions)
    : MutableTexture(context) {

  Reset();
  TextureData x = textureData;
  _texture = context.ResourceAllocator->CreateTexture(x.Definition());

  _allocatedSubscription = _texture->Allocated(
      [this, context, viewDefinitions, data = move(x)](Resource *resource) {
        context.ResourceUploader->EnqueueUploadTask(resource, &data);
        OnAllocated(resource, viewDefinitions);
      });
}

void MutableTextureWithViews::Allocate(
    const TextureDefinition &definition,
    const std::optional<TextureViewDefinitions> &viewDefinitions) {
  Reset();

  _texture = _context.ResourceAllocator->CreateTexture(definition);
  _allocatedSubscription =
      _texture->Allocated([this, viewDefinitions](Resource *resource) {
        OnAllocated(resource, viewDefinitions);
      });
}
void MutableTextureWithViews::OnAllocated(
    Resource *resource, std::optional<TextureViewDefinitions> viewDefs) {
  _definition = make_unique<TextureDefinition>(resource->Description());
  auto flags = D3D12_RESOURCE_FLAGS(_definition->Flags);
  auto texture = static_cast<Texture *>(resource);

  if (!has_flag(flags, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
    const D3D12_SHADER_RESOURCE_VIEW_DESC *const desc =
        viewDefs.has_value() ? &viewDefs->ShaderResource.value_or(nullptr)
                             : nullptr;
    _shaderResourceView =
        _context.CommonDescriptorHeap->CreateDescriptor<ShaderResourceView>(
            resource->get(), desc);
  }

  if (has_flag(flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
    const D3D12_RENDER_TARGET_VIEW_DESC *const desc =
        viewDefs.has_value() ? &viewDefs->RenderTarget.value_or(nullptr)
                             : nullptr;
    _renderTargetView =
        _context.RenderTargetDescriptorHeap->CreateDescriptor<RenderTargetView>(
            texture->get(), desc);
  }

  if (has_flag(flags, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
    const D3D12_DEPTH_STENCIL_VIEW_DESC *const desc =
        viewDefs.has_value() ? &viewDefs->DepthStencil.value_or(nullptr)
                             : nullptr;
    _depthStencilView =
        _context.DepthStencilDescriptorHeap->CreateDescriptor<DepthStencilView>(
            texture->get(), desc);
  }

  if (has_flag(flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
    const D3D12_UNORDERED_ACCESS_VIEW_DESC *const desc =
        viewDefs.has_value() ? &viewDefs->UnorderedAccess.value_or(nullptr)
                             : nullptr;
    _unorderedAccessView =
        _context.CommonDescriptorHeap->CreateDescriptor<UnorderedAccessView>(
            resource->get(), desc);
  }
}

} // namespace Axodox::Graphics::D3D12
