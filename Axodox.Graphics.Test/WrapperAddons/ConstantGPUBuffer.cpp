#include "pch.h"
#include "ConstantGPUBuffer.h"

ConstantGPUBuffer::ConstantGPUBuffer(const ResourceAllocationContext &context,
                                     BufferData data)
    :

      bufferRef(
          context.ResourceAllocator->CreateBuffer(BufferDefinition(data))) {
  _allocatedEvent = bufferRef->Allocated(
      [this, buffer = std::move(data), context](Resource *resource) {
        context.ResourceUploader->EnqueueUploadTask(resource, &buffer);
        view = (*resource)->GetGPUVirtualAddress();
      });
}

// void ConstantGPUBuffer::Update(DynamicBufferManager &buffManager,
//                                CommandAllocator &allocator,
//                                std::span<const uint8_t> buffer) {
//     assert(bufferRef.get()->Definition().Length = buffer.size_bytes();
//     GpuVirtualAddress tmpBuff = buffManager.AddBuffer(buffer);
//     [&block,offset] = buffManager.GetBlockAndOffsetForAddress(tmpBuff);
//     winrt::com_ptr<ID3D12Resource> res = bufferRef.get()->get();
//     // Copy tmpBuff to res using allocator bytes: sizeof(T)
//
//   allocator.operator->()->CopyBufferRegion(
//     res.get(),
//     0,
//    block.get()->(),
//     offset,
//     sizeof(T)
//   );
// }