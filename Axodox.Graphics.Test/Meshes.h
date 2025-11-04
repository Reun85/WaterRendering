#pragma once
#include "pch.h"
namespace Reun {

namespace Meshes {
/// <summary>
/// For deferred shading the culling should be set to CullCounterClockwise
/// therefore shown mesh should be facing backwards
/// </summary>
/// <param name="size"></param>
/// <param name="subdivisions"></param>
/// <returns></returns>
MeshDescription CreateBackwardsPlane(float size, DirectX::XMUINT2 subdivisions);

MeshDescription CreateCubeWithoutBottom(float size,
                                        XMFLOAT3 offset = XMFLOAT3{0, 0, 0});

MeshDescription CreateBoxInVSMesh();

MeshDescription CreateQuadPatch();
}; // namespace Meshes

namespace Detail {
struct Meshes {};
}; // namespace Detail
}; // namespace Reun
