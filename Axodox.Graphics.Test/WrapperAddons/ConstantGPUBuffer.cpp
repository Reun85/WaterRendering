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
