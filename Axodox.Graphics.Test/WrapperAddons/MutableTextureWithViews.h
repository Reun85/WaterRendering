#pragma once
#include "../pch.h"

namespace Axodox::Graphics::D3D12 {
/// The Views generated for a given texture
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
/// <summary>
/// When generating a texture with Axodox graphics, depending on the
/// TextureFlags it will assume its usage, and generate the views accordingly.
/// For example, it will not allow you to generate shaderResource /
/// UnorderedAccess for a depth buffer texture. With this class, you can specify
/// the views you want to generate for texture.
/// </summary>
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