#include <pch.h>
#include <winrt/Windows.UI.Core.h>
#include <array>
#include "Defaults.h"
using namespace winrt::Windows::Foundation::Numerics;

using uint = uint32_t;
using NodeID = uint;
using NodeCenter = float2;
using NodeSize = float2;
// -1 means root
using ChildrenID = int;
using Depth = uint;

#define USEDIRECTPOINTERS false

struct Node {
  NodeCenter center = {0, 0};
  NodeSize size = {0, 0};
#if USEDIRECTPOINTERS
  Node *children[4] = {nullptr, nullptr, nullptr, nullptr};
  Node *parent = nullptr;
#else
  NodeID children[4] = {0, 0, 0, 0};
  NodeID parent = 0;
#endif
#if USEDIRECTPOINTERS
  constexpr bool HasChildren() const { return children[0] != nullptr; }
#else
  constexpr bool HasChildren() const { return children[0] != 0; }
#endif

  constexpr static std::array<NodeSize, 4> childDirections = {
      {{-1.f, -1.f}, {-1.f, 1.f}, {1.f, -1.f}, {1.f, 1.f}}};

  /*

     ^   2 3
     |   0 1
    xpos
     0 zpos ->

  */

  const float3 GetCenter(const float height) const {
    return float3(center.x, height, center.y);
  }
};

class QuadTree;

class ConstQuadTreeLeafIteratorDepthFirst {
public:
  ConstQuadTreeLeafIteratorDepthFirst(const NodeID node, const Depth maxDepth,
                                      const QuadTree &_tree);

  ConstQuadTreeLeafIteratorDepthFirst &operator++() noexcept;
  const Node &operator*() const;
  const Node *operator->() const;
  constexpr bool
  operator!=(const ConstQuadTreeLeafIteratorDepthFirst &other) const noexcept {
    return node != other.node;
  }
  constexpr bool
  operator==(const ConstQuadTreeLeafIteratorDepthFirst &other) const noexcept {
    return node == other.node;
  }
  constexpr bool operator!=(const NodeID other) const noexcept {
    return node != other;
  }
  constexpr bool operator==(const NodeID other) const noexcept {
    return node == other;
  }

  // If the neighbor is bigger or same size, its 1
  // No neighbor = 0
  // Else ratio from this to that <==> if its twice the size of this, its 0.5
  struct SmallerNeighborRatio {
    /*
    xpos = xpos
    xneg = xneg
    zneg = zneg
    right = zpos
    */
    float xpos;
    float xneg;
    float zneg;
    float zpos;
  };

  SmallerNeighborRatio GetSmallerNeighbor() const;

private:
  void AdjustNode();
  void IterateTillLeaf();

  NodeID node;
  // Path to get current leaf
  std::vector<ChildrenID> path;
  const QuadTree &tree;
};
// The plane must be perpendicular to the Y-axis.
class QuadTree {
  using ValueType = Node;
  using ConstIterator = ConstQuadTreeLeafIteratorDepthFirst;

public:
  QuadTree(const uint allocation = Defaults::QuadTree::allocation)
      : nodes(allocation), count(0), height(0),
        maxDepth(Defaults::QuadTree::maxDepth),
        minDepth(Defaults::QuadTree::minDepth) {}
  void
  Build(const float3 center, const float2 fullSizeXZ, const float3 camEye,
        const float distanceThreshold = Defaults::QuadTree::distanceThreshold);
  const ValueType &GetRoot() const { return nodes[0]; }
  ConstIterator begin() const {
    return ConstQuadTreeLeafIteratorDepthFirst(0, height, *this);
  }
  const ValueType &GetAt(NodeID id) const { return nodes[id]; }
  // ConstIterator end() const {
  //   return ConstQuadTreeLeafIteratorDepthFirst(&*nodes.end());
  // }
  const NodeID end() const { return GetSize(); }
  const NodeID &GetSize() const { return count; }

private:
  friend class ConstIterator;
  void BuildRecursively(const uint index, const float height,
                        const float3 camEye, const float distanceThreshold,
                        const Depth depth);

  std::vector<ValueType> nodes;
  NodeID count;
  Depth height;
  Depth maxDepth;
  Depth minDepth;
};
