#include <pch.h>
#include "QuadTree.h"
#include <array>

ConstQuadTreeLeafIteratorDepthFirst::ConstQuadTreeLeafIteratorDepthFirst(
    const NodeID node, const Depth maxDepth, const QuadTree &_tree)
    : node(node), path(maxDepth), tree(_tree) {
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
  IterateTillLeaf();
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

  const auto curr_id = node;

  if (curr_id + 1 == tree.GetAt(node).children[0]) {
    // Child node
    path.push_back(0);
  } else if (path.back() != 3) {
    // Sibling node
    path.back()++;
  } else {
    while (path.back() == 3) {
      path.pop_back();
    }
    path.back()++;
  }
  node++;
}

void ConstQuadTreeLeafIteratorDepthFirst::IterateTillLeaf() {
  while (tree.GetAt(node).HasChildren()) {
    AdjustNode();
  }
}

void QuadTree::Build(const float3 center, const float2 fullSizeXZ,
                     const float3 camEye, const XMMATRIX &mvpMatrix,
                     const float distanceThreshold) {

  // zero it out, especially the children part
  memset(&nodes[0], 0, sizeof(Node) * nodes.size());

  nodes[0] = {{center.x, center.z}, {fullSizeXZ.x, fullSizeXZ.y}};
  count = 1;
  height = 0;
  BuildRecursively(0, center.y, camEye, distanceThreshold, 0, mvpMatrix);
}
bool AreAABBsIntersecting(const XMFLOAT3 &min1, const XMFLOAT3 &max1,
                          const XMFLOAT3 &min2, const XMFLOAT3 &max2) {
  if (max1.x < min2.x || max2.x < min1.x)
    return false;
  // Check if one box is above the other
  if (max1.y < min2.y || max2.y < min1.y)
    return false;
  // Check if one box is in front of the other
  if (max1.z < min2.z || max2.z < min1.z)
    return false;

  // If none of the above, the boxes intersect
  return true;
}
bool IsAABBContained(const XMFLOAT3 &outerMin, const XMFLOAT3 &outerMax,
                     const XMFLOAT3 &innerMin, const XMFLOAT3 &innerMax) {
  // Check if the inner box is within the outer box
  return (outerMin.x <= innerMin.x && outerMax.x >= innerMax.x &&
          outerMin.y <= innerMin.y && outerMax.y >= innerMax.y/* &&
          outerMin.z <= innerMin.z && outerMax.z >= innerMax.z*/);
}
inline bool IsInViewFrustum(const Node &node, const float &yCoordinate,
                            const XMMATRIX &mvpMatrix) {
  return true;

  XMVECTOR centerWorldSpace =
      XMVectorSet(node.center.x, yCoordinate, node.center.y, 1.0f);

  float sizex = node.size.x / 2.0f;
  float sizey = node.size.y / 2.0f;

  XMVECTOR p0 =
      XMVectorAdd(centerWorldSpace, XMVectorSet(sizex, 0.0f, sizey, 0.0f));
  XMVECTOR p1 =
      XMVectorAdd(centerWorldSpace, XMVectorSet(-sizex, 0.0f, sizey, 0.0f));
  XMVECTOR p2 =
      XMVectorAdd(centerWorldSpace, XMVectorSet(-sizex, 0.0f, -sizey, 0.0f));
  XMVECTOR p3 =
      XMVectorAdd(centerWorldSpace, XMVectorSet(sizex, 0.0f, -sizey, 0.0f));

  p0 = XMVector3TransformCoord(p0, mvpMatrix);
  p1 = XMVector3TransformCoord(p1, mvpMatrix);
  p2 = XMVector3TransformCoord(p2, mvpMatrix);
  p3 = XMVector3TransformCoord(p3, mvpMatrix);

  std::array<XMFLOAT3, 4> corners;
  XMStoreFloat3(&corners[0], p0);
  XMStoreFloat3(&corners[1], p1);
  XMStoreFloat3(&corners[2], p2);
  XMStoreFloat3(&corners[3], p3);

  // Calculate bounding box of the quad
  XMFLOAT3 min = {std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
  XMFLOAT3 max = {std::numeric_limits<float>::min(),
                  std::numeric_limits<float>::min(),
                  std::numeric_limits<float>::min()};

  XMFLOAT3 min2 = {-1.0f, -1.0f, 0.0f};
  XMFLOAT3 max2 = {1.0f, 1.0f, 1.0f};

  for (const auto &corner : corners) {
    min.x = std::min(min.x, corner.x);
    max.x = std::max(max.x, corner.x);
    min.y = std::min(min.y, corner.y);
    max.y = std::max(max.y, corner.y);
    min.z = std::min(min.z, corner.z);
    max.z = std::max(max.z, corner.z);
  }
  if (AreAABBsIntersecting(min, max, min2, max2)) {
    return true;
  }

  if (IsAABBContained(min, max, min2, max2) ||
      IsAABBContained(min2, max2, min, max)) {
    return true;
  }

  return false;
}
void QuadTree::BuildRecursively(const uint index, const float yCoordinate,
                                const float3 camEye,
                                const float distanceThreshold,
                                const Depth depth, const XMMATRIX &mvpMatrix) {
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

  bool cont = dist / (node.size.x * node.size.y) < distanceThreshold;
  // bool cont = dist / (std::max(node.size.x, node.size.y)) <
  // distanceThreshold;
  //  bool cont = dist < distanceThreshold;
  if ((cont || depth < minDepth) &&
      IsInViewFrustum(node, yCoordinate, mvpMatrix)) {
    for (uint i = 0; i < 4; i++) {
      uint id = count;
      auto &node2 = nodes[index];
      ++count;
      node2.children[i] = id;
      auto &curr = nodes[id];
      curr.parent = index;
      curr.size = node2.size / 2;
      curr.center = node2.center + Node::childDirections[i] * (curr.size / 2);
      BuildRecursively(id, yCoordinate, camEye, distanceThreshold, depth + 1,
                       mvpMatrix);
    }
  }
}
