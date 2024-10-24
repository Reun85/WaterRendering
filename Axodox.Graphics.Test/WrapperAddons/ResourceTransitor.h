#pragma once
#include "pch.h"

class ResourceTransitorTooManyValuesError : public std::exception {};

/// Holds a reference to command allocator and the required resource
/// transitions. When this value is dropped, it will automatically queue the
/// transitions. This can also be achieved by calling the `Queue` method.
/// If you hold onto this value the transitions will NOT be queued.
template <u64 N> class ResourceTransitor {
  struct Data {
    union {
      ResourceTransition x;
    };
    Data(){};
  };
  CommandAllocator &allocator;
  std::array<Data, N> arr;
  u64 n = 0;

public:
  ResourceTransitor(CommandAllocator &all) : allocator(all) {}
  ResourceTransitor(CommandAllocator &all, const ResourceTransitor &other) {
    allocator = all;
    Add(other);
  }
  ResourceTransitor(CommandAllocator &all,
                    const std::array<ResourceTransition, N> &inp = {})
      : allocator(all) {
    for (auto x : inp) {
      if (n > N)
        throw new std::runtime_error("ResourceTransitor: Too many transitions");
      arr[n++].x = x;
    }
  }
  ResourceTransitor(CommandAllocator &all,
                    const std::initializer_list<ResourceTransition> &inp = {})
      : allocator(all) {
    for (auto x : inp) {
      if (n > N)
        throw new std::runtime_error("ResourceTransitor: Too many transitions");
      arr[n++].x = x;
    }
  }
  ResourceTransitor &Add(const ResourceTransitor &other) {
    for (auto &tr : other.arr) {
      if (n > N)
        throw new std::runtime_error("ResourceTransitor: Too many transitions");
      arr[n++].x = tr;
    }
    other.Invalidate();
  }
  ResourceTransitor &
  Add(const std::initializer_list<std::pair<ResourceStates, ResourceStates>>
          &inp) {
    for (auto &tr : inp) {
      if (n > N)
        throw new std::runtime_error("ResourceTransitor: Too many transitions");
      arr[n++].x = tr;
    }
  }
  void Queue() {

    std::array<D3D12_RESOURCE_BARRIER, N> barriers;

    for (int i = 0; i < n; ++i) {
      auto resource = arr[i].x;
      barriers[i] = {
          .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
          .Transition = {.pResource = resource.Resource.Pointer,
                         .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                         .StateBefore = D3D12_RESOURCE_STATES(resource.From),
                         .StateAfter = D3D12_RESOURCE_STATES(resource.To)}};
    }

    allocator->ResourceBarrier(uint32_t(n), barriers.data());

    Invalidate();
  }
  void Invalidate() { n = 0; }

  static template <u64 ArrN, u64 N2>
  ResourceTransitor<N2 * ArrN>
  Combine(const std::array<ResourceTransitor<N2>, ArrN> &inp) {
    ResourceTransitor<N2 * ArrN> ret(all);
    for (auto &x : inp) {

      ret.Add(x);
    }
    return ret;
  }

  template <u64 N2>
  ResourceTransitor<N2 + N> operator+(const ResourceTransitor &other) {
    ResourceTransitor<N2 + N> ret(all);
    ret.Add(*this);
    ret.Add(other);
  }
  ~ResourceTransitor() { Queue(); }
};
