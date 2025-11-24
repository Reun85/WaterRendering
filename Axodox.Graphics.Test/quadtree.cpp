#include "pch.h"
#include "CppUnitTest.h"
#include "../Axodox.Graphics.Desktop/QuadTree.cpp"
#include "../Axodox.Graphics.Desktop/Frustum.hpp"
#include "../Axodox.Graphics.Desktop/Camera.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Reun;
using namespace DirectX;

// mock imgui functions
bool ImGui::Begin(char const *, bool *, int) { return true; }
bool ImGui::Checkbox(const char *, bool *) { return false; }
bool ImGui::SliderFloat(const char *, float *, float, float, const char *,
                        int) {
  return false;
}
bool ImGui::InputFloat3(const char *, float *, char const *, int) {
  return true;
}
void ImGui::End() {}
namespace ReunTests {
XMMATRIX GetIdentityMatrix() { return XMMatrixIdentity(); }

Reun::Frustum mockFrustum() {
  Reun::Frustum f;
  f.bottomFace = Reun::Plane(XMVectorSet(0.0, 1.0, 0.0, 1.0),
                             XMVectorSet(0.0, -1000.0, 0.0, 0.0));
  f.farFace = Reun::Plane(XMVectorSet(1.0, 0.0, 0.0, 1.0),
                          XMVectorSet(-1000.0, 0.0, 0.0, 0.0));
  f.leftFace = Reun::Plane(XMVectorSet(0.0, 0.0, 1.0, 1.0),
                           XMVectorSet(0.0, 0.0, -1000.0, 0.0));
  f.nearFace = Reun::Plane(XMVectorSet(-1.0, 0.0, 0.0, 1.0),
                           XMVectorSet(1000.0, 0.0, 0.0, 0.0));
  f.rightFace = Reun::Plane(XMVectorSet(0.0, 0.0, -1.0, 1.0),
                            XMVectorSet(0.0, 0.0, 1000.0, 0.0));
  f.topFace = Reun::Plane(XMVectorSet(0.0, -1.0, 0.0, 1.0),
                          XMVectorSet(0.0, 1000.0, 0.0, 0.0));
  return f;
}

TEST_CLASS(QuadTreeTests){
  public :

      TEST_METHOD(Node_Initialization_Defaults){Node node;
Assert::AreEqual(0.0f, node.center.x);
Assert::AreEqual(0.0f, node.center.y);
Assert::AreEqual(0.0f, node.size.x);
Assert::AreEqual((NodeID)0, node.parent);
Assert::IsFalse(node.HasChildren(), L"New node should not have children");
} // namespace ReunTests

TEST_METHOD(Node_HasChildren_Logic) {
  Node node;
  node.children[0] = 0;
  Assert::IsFalse(node.HasChildren());

  node.children[0] = 5;
  Assert::IsTrue(node.HasChildren(),
                 L"Node should report children if index 0 is non-zero");
}

TEST_METHOD(Node_GetCenter_3D_Projection) {
  Node node;
  node.center = {10.5f, -5.5f};
  float height = 100.0f;

  float3 result = node.GetCenter(height);

  Assert::AreEqual(10.5f, result.x);
  Assert::AreEqual(100.0f, result.y,
                   L"Y component should equal the input height");
  Assert::AreEqual(-5.5f, result.z);
}

TEST_METHOD(Node_ChildDirections_Constants) {
  // Verify the static direction multipliers verify standard QuadTree quadrant
  // logic
  // (-1, -1), (-1, 1), (1, -1), (1, 1)
  auto &dirs = Node::childDirections;

  Assert::AreEqual(-1.0f, dirs[0].x);
  Assert::AreEqual(-1.0f, dirs[0].y);
  Assert::AreEqual(-1.0f, dirs[1].x);
  Assert::AreEqual(1.0f, dirs[1].y);
  Assert::AreEqual(1.0f, dirs[2].x);
  Assert::AreEqual(-1.0f, dirs[2].y);
  Assert::AreEqual(1.0f, dirs[3].x);
  Assert::AreEqual(1.0f, dirs[3].y);
}
TEST_METHOD(QuadTree_Construction_Empty) {
  QuadTree qt;
  Assert::AreEqual((NodeID)0, qt.GetSize());
}

TEST_METHOD(QuadTree_Build_PopulatesNodes) {
  QuadTree qt;
  float3 center = {0, 0, 0};
  float2 size = {100, 100};
  float3 camEye = {0, 50, 0};
  float3 camDir = {0, -1, 0};
  Reun::Frustum frustum = mockFrustum();
  XMMATRIX viewProj = GetIdentityMatrix();

  qt.Build(center, size, camEye, camDir, frustum, viewProj, 10.0f, 3);

  // Assert
  Assert::IsTrue(qt.GetSize() > 0, L"Tree should contain nodes after build");

  const Node &root = qt.GetRoot();
  Assert::AreEqual(center.x, root.center.x, 0.01f);
  Assert::AreEqual(center.z, root.center.y, 0.01f);
}

TEST_METHOD(Iterator_Begin_End) {
  QuadTree qt;

  float3 center = {0, 0, 0};
  float2 size = {10, 10};

  float3 cam_dir = {0, -1, 0};
  Reun::Frustum f = mockFrustum();

  qt.Build(center, size, {0.1, 3, 0}, cam_dir, f, GetIdentityMatrix(), 200.0f,
           0);
  Assert::AreEqual(5, (int)qt.GetSize());
  auto it = qt.begin();
  Assert::IsTrue(
      it != qt.end(),
      L"Iterator should not be at end immediately if tree has nodes");

  for (int i = 0; i < 4; ++i) {

    const Node &n = *it;
    ++it;
    // div per 2 because the first 4 are leaf nodes.
    Assert::AreEqual(size.x / 2.f, n.size.x);
  }
  Assert::IsTrue(it == qt.end(),
                 L"After visiting all leaves, iterator should be at end");
}

TEST_METHOD(Iterator_Traversal) {
  QuadTree qt;
  Reun::Frustum f = mockFrustum();

  // This will split twice 16 leaf nodes.
  qt.Build({0, 0, 0}, {100, 100}, {0, 100, 0}, {0, -1, 0}, f,
           GetIdentityMatrix(), 100.0f, 1);

  int leafCount = 0;
  for (auto it = qt.begin(); it != qt.end(); ++it) {
    leafCount++;
    // Sanity check, iterated elements should not have children.
    Assert::IsFalse(it->HasChildren());
  }

  Assert::AreEqual(16, leafCount);
}

TEST_METHOD(TravelOrder_Initialization) {
  float3 camDir = {1, 0, 0};
  TravelOrder order(camDir, GetIdentityMatrix());

  bool already_found_end = false;

  for (auto order : order.directions) {
    bool isValid = (order >= 0 && order <= 3);
    bool is_end = order == 9;
    Assert::IsFalse(is_end && already_found_end);
    Assert::IsTrue(isValid || is_end,
                   L"TravelOrder directions must be valid child indices");
    already_found_end |= is_end;
  }
}

TEST_METHOD(Neighbor_Ratio_Calculation) {

  QuadTree qt;
  Reun::Frustum f = mockFrustum();

  qt.Build({0, 0, 0}, {100, 100}, {50, 5, 50}, {0, -1, 0}, f,
           GetIdentityMatrix(), 10.0f, 3);

  bool foundDiff = false;

  for (auto it = qt.begin(); it != qt.end(); ++it) {
    auto ratios = it.GetSmallerNeighbor();

    auto validate = [](float r) {
      return r == 0.0f || r == 0.5f || r == 1.0f || r == 2.0f;
    };

    Assert::IsTrue(validate(ratios.xpos));
    Assert::IsTrue(validate(ratios.xneg));
    Assert::IsTrue(validate(ratios.zpos));
    Assert::IsTrue(validate(ratios.zneg));
  }
}
}
;
} // namespace ReunTests
