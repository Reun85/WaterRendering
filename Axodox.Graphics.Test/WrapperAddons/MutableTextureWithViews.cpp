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
        MutableTextureWithViews::OnAllocated(resource, viewDefinitions);
      });
}
void MutableTextureWithViews::OnAllocated(
    Resource *resource, std::optional<TextureViewDefinitions> viewDefs) {
  _definition = make_unique<TextureDefinition>(resource->Description());
  auto flags = D3D12_RESOURCE_FLAGS(_definition->Flags);
  auto texture = static_cast<Texture *>(resource);

  if (!has_flag(flags, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
    const D3D12_SHADER_RESOURCE_VIEW_DESC *const desc =
        viewDefs.has_value() && viewDefs->ShaderResource.has_value()
            ? &viewDefs->ShaderResource.value()
            : nullptr;
    _shaderResourceView =
        _context.CommonDescriptorHeap->CreateDescriptor<ShaderResourceView>(
            resource->get(), desc);
  }

  if (has_flag(flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
    const D3D12_RENDER_TARGET_VIEW_DESC *const desc =
        viewDefs.has_value() && viewDefs->RenderTarget.has_value()
            ? &viewDefs->RenderTarget.value()
            : nullptr;

    _renderTargetView =
        _context.RenderTargetDescriptorHeap->CreateDescriptor<RenderTargetView>(
            texture->get(), desc);
  }

  if (has_flag(flags, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
    const D3D12_DEPTH_STENCIL_VIEW_DESC *const desc =
        viewDefs.has_value() && viewDefs->DepthStencil.has_value()
            ? &viewDefs->DepthStencil.value()
            : nullptr;
    _depthStencilView =
        _context.DepthStencilDescriptorHeap->CreateDescriptor<DepthStencilView>(
            texture->get(), desc);
  }

  if (has_flag(flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
    const D3D12_UNORDERED_ACCESS_VIEW_DESC *const desc =
        viewDefs.has_value() && viewDefs->UnorderedAccess.has_value()
            ? &viewDefs->UnorderedAccess.value()
            : nullptr;
    _unorderedAccessView =
        _context.CommonDescriptorHeap->CreateDescriptor<UnorderedAccessView>(
            resource->get(), desc);
  }
}

TextureViewDefinitions
TextureViewDefinitions::GetDepthStencilWithShaderView(const Format &DSFormat,
                                                      const Format &SRVFormat) {
  TextureViewDefinitions depthViews{
      .ShaderResource = D3D12_SHADER_RESOURCE_VIEW_DESC{},
      .RenderTarget = std::nullopt,
      .DepthStencil = D3D12_DEPTH_STENCIL_VIEW_DESC{},
      .UnorderedAccess = std::nullopt};
  depthViews.DepthStencil->Format = static_cast<DXGI_FORMAT>(DSFormat);
  depthViews.DepthStencil->Flags = D3D12_DSV_FLAG_NONE;
  depthViews.DepthStencil->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  depthViews.DepthStencil->Texture2D.MipSlice = 0;

  depthViews.ShaderResource->Format = static_cast<DXGI_FORMAT>(SRVFormat);
  depthViews.ShaderResource->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  depthViews.ShaderResource->Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  depthViews.ShaderResource->Texture2D.MipLevels = 1;
  depthViews.ShaderResource->Texture2D.MostDetailedMip = 0;

  return depthViews;
}

} // namespace Axodox::Graphics::D3D12
