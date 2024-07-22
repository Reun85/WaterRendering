#include <pch.h>
#include "QuadTree.h"
#include <array>

ConstQuadTreeLeafIteratorDepthFirst::ConstQuadTreeLeafIteratorDepthFirst(
    const Node *node, const Depth maxDepth, const QuadTree &_tree)
    : node(node), path(maxDepth), tree(_tree) {
  path.clear();
  path.push_back(-1);
  IterateTillLeaf();
}

const Node &ConstQuadTreeLeafIteratorDepthFirst::operator*() const {
  return *node;
}
const Node *ConstQuadTreeLeafIteratorDepthFirst::operator->() const {
  return node;
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
constexpr float IsSmaller(NeighborDirection &directions,
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
    auto &[matchingIndex, shouldGoUp] = directions[*backitr];
    stop = !shouldGoUp;
    buff.push_back(matchingIndex);
    node = node->parent;
  }
  for (auto backitr = buff.rbegin(); backitr != buff.rend(); backitr++) {
    node = node->children[*backitr];
    if (!node->HasChildren()) {
      return 1;
    }
  }
  if (node->HasChildren()) {
    return 0.5;
  } else {
    return 1;
  }
}

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

  const Node *const id = node;
  res.xpos = IsSmaller(xpos, path, buff, id);
  res.xneg = IsSmaller(xneg, path, buff, id);
  res.zneg = IsSmaller(zneg, path, buff, id);
  res.zpos = IsSmaller(zpos, path, buff, id);
  return res;
}
void ConstQuadTreeLeafIteratorDepthFirst::AdjustNode() {

  if (node + 1 == node->children[0]) {
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
  while (node->HasChildren()) {
    AdjustNode();
  }
}

void QuadTree::Build(const float3 center, const float2 fullSizeXZ,
                     const float3 camEye, const float distanceThreshold) {

  // zero it out, especially the children part
  memset(&nodes[0], 0, sizeof(Node) * nodes.size());

  nodes[0] = {{center.x, center.z}, {fullSizeXZ.x, fullSizeXZ.y}};
  count = 1;
  height = 0;
  BuildRecursively(0, center.y, camEye, distanceThreshold, 0);
}

void QuadTree::BuildRecursively(const uint index, const float yCoordinate,
                                const float3 camEye,
                                const float distanceThreshold,
                                const Depth depth) {
  if (depth > height)
    height = depth;
  if (depth > maxDepth) {
    return;
  }
  auto &node = nodes[index];
  float3 diff = {node.center.x - camEye.x, yCoordinate - camEye.y,
                 node.center.y - camEye.z};
  double dist = sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
  // if (dist / (node.size.x * node.size.y) < distanceThreshold) {
  if (dist / (std::max(node.size.x, node.size.y)) < distanceThreshold ||
      depth < minDepth) {
    for (uint i = 0; i < 4; i++) {
      uint id = count;
      count++;
      if (id >= nodes.size() - 1) {
        nodes.emplace_back();
      }
      auto &curr = nodes[id];
      node.children[i] = &nodes[id];
      curr.parent = &nodes[index];
      curr.size = node.size / 2;
      curr.center = node.center + Node::childDirections[i] * (curr.size / 2);
      BuildRecursively(id, yCoordinate, camEye, distanceThreshold, depth + 1);
    }
  }
}
