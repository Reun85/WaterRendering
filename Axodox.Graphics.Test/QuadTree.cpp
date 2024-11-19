#include <pch.h>
#include "QuadTree.h"
#include <array>

ConstQuadTreeLeafIteratorDepthFirst::ConstQuadTreeLeafIteratorDepthFirst(
    const NodeID node, const Depth maxDepth, const QuadTree &_tree,
    const TravelOrder &_order)
    : node(node), path(maxDepth + 2), tree(_tree), order(_order) {
  path.clear();
  path.push_back(-1);
  IterateTillLeaf();
}

const Node &ConstQuadTreeLeafIteratorDepthFirst::operator*() const {
  return tree.GetAt(node);
}
const Node *ConstQuadTreeLeafIteratorDepthFirst::operator->() const {
  return &tree.GetAt(node);
}
ConstQuadTreeLeafIteratorDepthFirst &
ConstQuadTreeLeafIteratorDepthFirst::operator++() noexcept {

  AdjustNode();
  return *this;
}

using NeedToGoUp = bool;
using NeighborDirection =
    const std::array<const std::pair<const ChildrenID, const NeedToGoUp>, 4>;

// NodeID must be a leaf.
constexpr float IsSmaller(NeighborDirection &directions, const QuadTree &tree,
                          const std::vector<int> &path,
                          std::vector<ChildrenID> &buff, const Node *node) {
  buff.clear();

  // Traverse the path in reverse to backtrack
  bool stop = false;
  for (auto backitr = path.rbegin(); backitr != path.rend() && !stop;
       backitr++) {
    if (*backitr == -1) {
      // No neighbor
      return 0;
    }
    auto [child, goUp] = directions[*backitr];
    stop = !goUp;
    buff.push_back(child);
    node = &tree.GetAt(node->parent);
  }
  for (auto backitr = buff.rbegin(); backitr != buff.rend(); backitr++) {
    node = &tree.GetAt(node->children[*backitr]);
    if (!node->HasChildren()) {
      return 1;
    }
  }
  float ret = 1.f;
  while (node->HasChildren()) {
    node = &tree.GetAt(node->children[*buff.begin()]);
    ret *= 0.5f;
  }
  return ret;
}
template <const NeighborDirection &directions>
constexpr float
IsSmallerTemplated(const QuadTree &tree, const std::vector<int> &path,
                   std::vector<ChildrenID> &buff, const Node *node) {
  buff.clear();

  // Traverse the path in reverse to backtrack
  bool stop = false;
  for (auto backitr = path.rbegin(); backitr != path.rend() && !stop;
       backitr++) {
    if (*backitr == -1) {
      // No neighbor
      return 0;
    }
    auto [child, goUp] = directions[*backitr];
    stop = !goUp;
    buff.push_back(child);
    node = &tree.GetAt(node->parent);
  }
  for (auto backitr = buff.rbegin(); backitr != buff.rend(); backitr++) {
    node = &tree.GetAt(node->children[*backitr]);
    if (!node->HasChildren()) {
      return 1;
    }
  }
  float ret = 1.f;
  while (node->HasChildren()) {
    node = &tree.GetAt(node->children[*buff.begin()]);
    ret *= 2.f;
  }
  return ret;
}

/*
  constexpr static std::array<float2, 4> childDirections = {
      {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};

  /*

     ^   2 3
     |   0 1
    xpos
     0 zpos ->

  */

ConstQuadTreeLeafIteratorDepthFirst::SmallerNeighborRatio
ConstQuadTreeLeafIteratorDepthFirst::GetSmallerNeighbor() const {
  static constexpr NeighborDirection xpos = {
      {{2, false}, {3, false}, {0, true}, {1, true}}};
  static constexpr NeighborDirection xneg = {
      {{2, true}, {3, true}, {0, false}, {1, false}}};
  static constexpr NeighborDirection zneg = {
      {{1, true}, {0, false}, {3, true}, {2, false}}};
  static constexpr NeighborDirection zpos = {
      {{1, false}, {0, true}, {3, false}, {2, true}}};

  std::vector<ChildrenID> buff(path.size());
  SmallerNeighborRatio res{};

  const Node *const id = &tree.GetAt(node);
  // res.xpos = IsSmaller(xpos, tree, path, buff, id);
  // res.xneg = IsSmaller(xneg, tree, path, buff, id);
  // res.zneg = IsSmaller(zneg, tree, path, buff, id);
  // res.zpos = IsSmaller(zpos, tree, path, buff, id);
  res.xpos = IsSmallerTemplated<xpos>(tree, path, buff, id);
  res.xneg = IsSmallerTemplated<xneg>(tree, path, buff, id);
  res.zneg = IsSmallerTemplated<zneg>(tree, path, buff, id);
  res.zpos = IsSmallerTemplated<zpos>(tree, path, buff, id);
  return res;
}
inline void ConstQuadTreeLeafIteratorDepthFirst::AdjustNode() {

  /// if the id+1 is the first child of the parent,
  /// then its has children: recurse!
  if (tree.GetAt(node).HasChildren()) {
    // Child node

    ChildrenID dir = order.directionStart;
    node = tree.GetAt(node).children[dir];
    path.push_back(dir);
  } else {
    if (order.directions[path.back()] != 9) {
      // Sibling node
    } else {
      while (path.back() != -1 && order.directions[path.back()] == 9) {
        path.pop_back();
        node = tree.GetAt(node).parent;
      }
    }
    // we are empty
    if (path.back() == -1) {
      node = tree.GetSize();
      return;
    } else {
      ChildrenID dir = order.directions[path.back()];
      node = tree.GetAt(tree.GetAt(node).parent).children[dir];
      path.back() = dir;
    }
  }
  IterateTillLeaf();
}

void ConstQuadTreeLeafIteratorDepthFirst::IterateTillLeaf() {
  while (tree.GetAt(node).HasChildren()) {
    ChildrenID dir = order.directionStart;
    node = tree.GetAt(node).children[dir];
    path.push_back(dir);
  }
}

void QuadTree::Build(const float3 &center, const float2 &fullSizeXZ,
                     const float3 &camEye, const float3 &camDir,
                     const Frustum &f, const XMMATRIX &mMatrix,
                     const float &quadTreeDistanceThreshold, Depth MaxDepth) {

  // zero it out, especially the children part
  memset(&nodes[0], 0, sizeof(Node) * nodes.size());

  nodes[0] = {{center.x, center.z}, {fullSizeXZ.x, fullSizeXZ.y}};
  count = 1;
  height = 0;
  maxDepth = MaxDepth;
  order = TravelOrder(camDir, mMatrix);
  BuildRecursively(0, center.y, camEye, quadTreeDistanceThreshold, 0, f,
                   mMatrix);
}
inline bool IsInViewFrustum(const Node &node, const float &yCoordinate,
                            const Frustum &f, const XMMATRIX &mMatrix) {

  const AABB aabb{XMVECTOR{node.center.x, yCoordinate, node.center.y},
                  node.size.x / 2, 0, node.size.y / 2};

  return aabb.isOnFrustum(f, mMatrix);
}
void QuadTree::BuildRecursively(const uint index, const float yCoordinate,
                                const float3 camEye,
                                const float quadTreeDistanceThreshold,
                                const Depth depth, const Frustum &f,
                                const XMMATRIX &mMatrix) {
  if (depth > height)
    height = depth;
  if (depth > maxDepth) {
    return;
  }

  if (count + 4 > nodes.size()) {
    nodes.emplace_back();
  }

  const auto &node = nodes[index];
  float3 diff = {node.center.x - camEye.x, yCoordinate - camEye.y,
                 node.center.y - camEye.z};
  double dist = sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
  dist *= dist;

  dist = dist / (node.size.x * node.size.y);
  bool cont = dist < quadTreeDistanceThreshold;
  // float x = std::max(node.size.x, node.size.y);
  // x *= x;
  // bool cont = dist / x < quadTreeDistanceThreshold;
  //  bool cont = dist < quadTreeDistanceThreshold;
  if ((cont || depth < minDepth) &&
      IsInViewFrustum(node, yCoordinate, f, mMatrix)) {
    for (uint i = 0; i < 4; i++) {
      uint id = count;
      auto &node2 = nodes[index];
      ++count;
      node2.children[i] = id;
      auto &curr = nodes[id];
      curr.parent = index;
      curr.size = node2.size / 2;
      curr.center = node2.center + Node::childDirections[i] * (curr.size / 2);
      BuildRecursively(id, yCoordinate, camEye, quadTreeDistanceThreshold,
                       depth + 1, f, mMatrix);
    }
  }
}

TravelOrder::TravelOrder(const float3 &camForward, const XMMATRIX &mMatrix) {
  XMVECTOR forwardXZ = XMVectorSet(camForward.x, 0.0f, camForward.z, 0.0f);
  forwardXZ = XMVector3Normalize(forwardXZ);

  XMVECTOR camBasedOffset = XMVECTOR{camForward.x >= 0 ? 1.f : -1.f, 0.f,
                                     camForward.z >= 0 ? 1.f : -1.f, 0.f};
  std::array<float, 4> values;
  for (int i = 0; i < 4; ++i) {
    // World space coordinates of the plane centers
    XMVECTOR curr = XMVector3TransformCoord(
        XMVectorSet(Node::childDirections[i].x / 2.f, 0.0f,
                    Node::childDirections[i].y / 2.f, 1.0f),
        mMatrix);
    curr = XMVectorAdd(curr, camBasedOffset);
    // Cam is at 0,0,0

    // we want high dot and low length
    // we can play around with dot^n and length^(-m)
    float length = XMVectorGetX(XMVector3Length(curr));
    float dot1 = XMVectorGetX(XMVector3Dot(curr, forwardXZ));

    values[i] = dot1 / (length * length);
  }

  std::array<ChildrenID, 4> indices = {0, 1, 2, 3};

  std::ranges::sort(indices,
                    [&values](int a, int b) { return values[a] > values[b]; });

  directionStart = indices[0];
  for (int i = 0; i < 3; ++i) {
    directions[indices[i]] = indices[i + 1];
  }
  directions[indices[3]] = 9;
}
