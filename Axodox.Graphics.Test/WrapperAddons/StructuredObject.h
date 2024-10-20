#pragma once
#include "../pch.h"

namespace Axodox::Graphics::D3D12 {
/// <summary>
///  The definitions for the views that will be created for a structured buffer.
/// std::nullopt means don't generate.
/// </summary>
struct BufferViewDefinitions {
  std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> ShaderResource = std::nullopt;
  std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> UnorderedAccess =
      std::nullopt;
};
/// <summary>
/// A way to create and manage views for buffer. Creating the Buffer must be handled elsewhere.
/// MUST BE CREATED BEFORE THE BUFFER IS ALLOCATED. IT WILL SUBSCRIBE TO THE ALLOCATION EVENT!
/// Useful for example, when you need to create a srv or uav for a vertexBuffer where the vertexBuffer is created and handled by a different part of the code.
/// This only creates the views for it, for (among many things) compute shaders. 
/// 
/// </summary>
class StructuredObjectViews {
public:
  StructuredObjectViews(ResourceAllocationContext &context, Buffer *const br,
                        const BufferViewDefinitions &def);
  StructuredObjectViews(ResourceAllocationContext &context, Buffer *const br,
                        std::function<BufferViewDefinitions(Resource *)> fn

  );

  ShaderResourceView *ShaderResource() const { return srv.get(); }
  UnorderedAccessView *UnorderedAccess() const { return uav.get(); }

private:
  ShaderResourceViewRef srv;
  UnorderedAccessViewRef uav;
  Infrastructure::event_subscription allocatedSubscription;
};
} // namespace Axodox::Graphics::D3D12


/// <summary>
///  An owning version of StructuredObjectViews
/// UNTESTED
/// </summary>
class StructuredObject : public StructuredObjectViews {


    StructuredObject(ResourceAllocationContext &context,
                   const BufferDefinition &bufferDefinitions,
                   const BufferViewDefinitions &viewDefinitions);
    BufferRef &Buffer() { return _buff; }
    private:
  BufferRef _buff;

};

struct MeshSpecificBuffers {

    StructuredObjectViews Vertex;
    StructuredObjectViews Index;

    /// UNTESTED
    explicit MeshSpecificBuffers(ResourceAllocationContext &context,
                                 const ImmutableMesh &mesh);
    explicit MeshSpecificBuffers(ResourceAllocationContext &context,
                                 const ImmutableMesh &mesh, u32 VertexByteSize,
                                 u32 IndexByteSize);
    ~MeshSpecificBuffers() = default;
  };
