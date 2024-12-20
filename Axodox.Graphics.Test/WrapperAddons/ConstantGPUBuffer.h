#pragma once
#include "pch.h"
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
/// <summary>
/// Wrapper for basic Uniform like buffers that should rarely be updated. Assumes that this will be uploaded at startup. If this has to change, upload
/// the replacement values via DynamicBufferManager, then queue a copy command
/// from the dynamic buffer to this buffer.
/// </summary>
class ConstantGPUBuffer {
public:
  ConstantGPUBuffer(const ResourceAllocationContext &context, BufferData data);

  D3D12_GPU_VIRTUAL_ADDRESS GetView() const { return view; };

 /* [[nodiscard]] void Update(DynamicBufferManager &buffManager,
                            CommandAllocator &allocator,
                            std::span<const uint8_t> buffer);

  template <typename T>
  [[nodiscard]] void Update(DynamicBufferManager &buffManager,
                            CommandAllocator &allocator, const T &value) {
     return AddBuffer(Infrastructure::to_span(buffManager, allocator, value));
   }*/

private:
  BufferRef bufferRef;

  Axodox::Infrastructure::event_subscription _allocatedEvent;

  D3D12_GPU_VIRTUAL_ADDRESS view;
};
