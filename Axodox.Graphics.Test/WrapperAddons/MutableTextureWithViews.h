#pragma once
#include "../pch.h"

namespace Axodox::Graphics::D3D12 {

struct TextureViewDefinitions {
  std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> ShaderResource = std::nullopt;
  std::optional<D3D12_RENDER_TARGET_VIEW_DESC> RenderTarget = std::nullopt;
  std::optional<D3D12_DEPTH_STENCIL_VIEW_DESC> DepthStencil = std::nullopt;
  std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> UnorderedAccess =
      std::nullopt;
  static TextureViewDefinitions
  GetDepthStencilWithShaderView(const Format &DSFormat,
                                const Format &SRVFormat);
};
class MutableTextureWithViews : public MutableTexture {
public:
  MutableTextureWithViews(const ResourceAllocationContext &context);
  MutableTextureWithViews(
      const ResourceAllocationContext &context,
      const TextureDefinition &definition,
      const std::optional<TextureViewDefinitions> &viewDefinitions);
  MutableTextureWithViews(
      const ResourceAllocationContext &context, const TextureData &startingData,
      const std::optional<TextureViewDefinitions> &viewDefinitions);

  void Allocate(const TextureDefinition &definition,
                const std::optional<TextureViewDefinitions> &viewDefinitions);

private:
  void OnAllocated(Resource *resource,
                   std::optional<TextureViewDefinitions> viewDefs);
};
} // namespace Axodox::Graphics::D3D12