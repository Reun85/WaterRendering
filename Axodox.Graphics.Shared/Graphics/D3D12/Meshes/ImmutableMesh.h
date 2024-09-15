#pragma once
#include "../Resources/ResourceAllocationContext.h"
#include "MeshDescriptions.h"

namespace Axodox::Graphics::D3D12 {
class AXODOX_GRAPHICS_API ImmutableMesh {

public:
  ImmutableMesh(const ResourceAllocationContext &context,
                MeshDescription &&description);

  void Draw(CommandAllocator &allocator, uint32_t instanceCount = 1) const;

  const BufferRef &GetVertexBuffer() const { return _vertexBuffer; }
  const BufferRef &GetIndexBuffer() const { return _indexBuffer; }
  const uint32_t GetIndexCount() const { return _indexCount; }

  const D3D12_PRIMITIVE_TOPOLOGY &GetTopology() const { return _topology; }

private:
  BufferRef _vertexBuffer;
  BufferRef _indexBuffer;

  Infrastructure::event_subscription _verticesAllocatedSubscription,
      _indicesAllocatedSubscription;

  D3D12_PRIMITIVE_TOPOLOGY _topology;
  uint32_t _vertexCount, _indexCount;
  std::optional<D3D12_VERTEX_BUFFER_VIEW> _vertexBufferView;
  std::optional<D3D12_INDEX_BUFFER_VIEW> _indexBufferView;

  static DXGI_FORMAT GetIndexFormat(uint32_t size);
};
} // namespace Axodox::Graphics::D3D12