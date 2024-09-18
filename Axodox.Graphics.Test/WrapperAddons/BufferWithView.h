#pragma once
#include "../pch.h"

namespace Axodox::Graphics::D3D12 {
struct BufferViewDefinitions {
  std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> ShaderResource = std::nullopt;
  std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> UnorderedAccess =
      std::nullopt;
};
class StructuredObjectViews {
public:
  StructuredObjectViews(ResourceAllocationContext &context, Buffer *const br,
                        const BufferViewDefinitions &def);

  ShaderResourceView *ShaderResource() const { return srv.get(); }
  UnorderedAccessView *UnorderedAccess() const { return uav.get(); }

private:
  ShaderResourceViewRef srv;
  UnorderedAccessViewRef uav;
  Infrastructure::event_subscription allocatedSubscription;
};
} // namespace Axodox::Graphics::D3D12
