#pragma once
#include "pch.h"
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
class ConstantGPUBuffer {
public:
  ConstantGPUBuffer(const ResourceAllocationContext &context, BufferData data);

  D3D12_GPU_VIRTUAL_ADDRESS GetView() const { return view; };

private:
  BufferRef bufferRef;

  Axodox::Infrastructure::event_subscription _allocatedEvent;

  D3D12_GPU_VIRTUAL_ADDRESS view;
};
