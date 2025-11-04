#pragma once
#include "pch.h"
namespace Reun {
struct ShaderBuffers {
  // Allocates necessary buffers if they are not yet allocated. May use the
  // finalTarget size to determine the sizes of the buffers
  virtual void MakeCompatible(const RenderTargetView &finalTarget,
                              ResourceAllocationContext &allocationContext) = 0;

  // Get ready for next frame
  virtual void Clear(CommandAllocator &allocator) = 0;

  virtual ~ShaderBuffers() = default;
};
} // namespace Reun
