#pragma once
#include "pch.h"

inline ResourceStates GetResourceStateFromFlags(const TextureFlags &flags) {
  if (has_flag(flags, TextureFlags::RenderTarget)) {
    return ResourceStates::RenderTarget;
  } else if (has_flag(flags, TextureFlags::DepthStencil)) {
    return ResourceStates::DepthWrite;
  } else if (has_flag(flags, TextureFlags::ShaderResourceDepthStencil)) {
    return ResourceStates::DepthWrite;
  } else if (has_flag(flags, TextureFlags::UnorderedAccess)) {
    return ResourceStates::UnorderedAccess;
  } else {
    return ResourceStates::Common;
  }
}

// A class meant to simplify keeping track of a GPU resource's state.
// If multiple threads (cpu or gpu command queues) use this in parallel, this
// abstraction will cause more issues than it solves.
class MutableTextureWithState : public Axodox::Graphics::D3D12::MutableTexture {
  ResourceStates _state;

public:
  MutableTextureWithState(const ResourceAllocationContext &context,
                          const TextureDefinition &definition)
      : MutableTexture(context, definition),
        _state(GetResourceStateFromFlags(definition.Flags)) {}

  // On the GPU the transition happen when the queue reaches this command,
  // on the CPU the "transition" will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this resource this
  // abstraction will cause more issues than it solves.
  void Transition(CommandAllocator &allocator, const ResourceStates &newState) {
    if (_state == newState)
      return;
    allocator.TransitionResource(
        this->operator Axodox::Graphics::D3D12::ResourceArgument(), _state,
        newState);
    _state = newState;
  }

  // On the GPU the transition happen when the queue reaches this command,
  // on the CPU the "transition" will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this resource this
  // abstraction will cause more issues than it solves.
  ShaderResourceView *ShaderResource(CommandAllocator &allocator) {
    Transition(allocator, ResourceStates::AllShaderResource);
    return MutableTexture::ShaderResource();
  };

  // On the GPU the transition happen when the queue reaches this command,
  // on the CPU the "transition" will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this resource this
  // abstraction will cause more issues than it solves.
  UnorderedAccessView *UnorderedAccess(CommandAllocator &allocator) {
    Transition(allocator, ResourceStates::UnorderedAccess);
    return MutableTexture::UnorderedAccess();
  };

  MutableTexture &getInnerUnsafe() { return *this; }

  TextureRef &getTexture() { return _texture; };
};
