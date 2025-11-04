#pragma once
#include "pch.h"

namespace Reun {
struct ShaderJob {
  virtual void Pre(CommandAllocator &allocator) const = 0;
  virtual ~ShaderJob() = default;
};
} // namespace Reun
