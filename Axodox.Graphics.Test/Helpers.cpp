#pragma once
#include "pch.h"

std::string Utf16ToUtf8(const std::wstring_view &wstr) {
  if (wstr.empty())
    return {};
  // Calculat the size
  int size_needed = WideCharToMultiByte(
      CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
  if (size_needed == 0)
    return {}; // handle error as needed
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0],
                      size_needed, nullptr, nullptr);
  return str;
}
std::wstring Utf8ToUtf16(const std::string_view &str) {
  if (str.empty())
    return {};
  // Calculat the size
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
  if (size_needed == 0)
    return {}; // handle error as needed
  std::wstring wstr(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0],
                      size_needed);
  return wstr;
}
void set_flag(u32 &flag, u32 flagIndex, bool flagValue) {
  if (flagValue) {
    flag |= 1 << flagIndex;
  } else {
    flag &= ~(1 << flagIndex);
  }
}

MeshDescription CreateQuadPatch() {
  float size = 0.5f;
  MeshDescription result;

  result.Vertices = {VertexPosition{XMFLOAT3{-size, 0.f, size}},
                     VertexPosition{XMFLOAT3{size, 0, size}},
                     VertexPosition{XMFLOAT3{-size, 0.f, -size}},
                     VertexPosition{XMFLOAT3{size, 0.f, -size}}};

  result.Topology = static_cast<PrimitiveTopology>(
      D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

  return result;
}
std::string GetLocalFolder() {
  auto localFolder =
      winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
  std::wstring Wpath = localFolder.c_str();
  auto path = Utf16ToUtf8(Wpath);
  return path;
  // return std::filesystem::path(path);
}

void RuntimeResults::DrawImGui(bool exclusiveWindow) const {
  bool cont = true;
  if (exclusiveWindow)
    cont = ImGui::Begin("Results");
  if (cont) {
    ImGui::Text("QuadTree Nodes = %d", qtNodes);

    ImGui::Text("QuadTree buildtime %.3f ms/frame",
                GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                std::chrono::nanoseconds>(
                    QuadTreeBuildTime));
    ImGui::Text("Navigating QuadTree %.3f ms/frame",
                GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                std::chrono::nanoseconds>(
                    NavigatingTheQuadTree));
    ImGui::Text("Drawn Nodes: %d", drawnNodes);
    ImGui::Text(
        "CPU time %.3f ms/frame",
        GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                        std::chrono::nanoseconds>(CPUTime));
  }
  if (exclusiveWindow)
    ImGui::End();
}

float frac(float x) { return x - std::floor(x); }

MeshDescription CreateBackwardsPlane(float size,
                                     DirectX::XMUINT2 subdivisions) {
  if (subdivisions.x < 2 || subdivisions.y < 2)
    throw std::logic_error("Plane size must be at least 2!");
  if (subdivisions.x * subdivisions.y >
      (uint64_t)std::numeric_limits<uint32_t>::max() + 1)
    throw std::logic_error("Run out of indices!");

  MeshDescription result;

  // Vertices
  float xstep = size / (subdivisions.x - 1),
        xtexstep = 1.f / (subdivisions.x - 1), xstart = -size / 2.f;
  float ystep = size / (subdivisions.y - 1),
        ytexstep = 1.f / (subdivisions.y - 1), ystart = -size / 2.f;
  uint32_t vertexCount = subdivisions.x * subdivisions.y;

  VertexPositionNormalTexture *pVertex = nullptr;
  result.Vertices = BufferData(vertexCount, pVertex);

  for (uint32_t j = 0; j < subdivisions.y; j++) {
    for (uint32_t i = 0; i < subdivisions.x; i++) {
      *pVertex++ = {XMFLOAT3{xstart + i * xstep, ystart + j * ystep, 0.f},
                    XMBYTEN4{0.f, 0.f, 1.f, 1.f},
                    XMUSHORTN2{i * xtexstep, j * ytexstep}};
    }
  }

  // Indices
  uint32_t triangleWidth = subdivisions.x - 1,
           triangleHeight = subdivisions.y - 1;
  uint32_t indexCount = triangleWidth * triangleHeight * 6;

  uint32_t *pIndex = nullptr;
  result.Indices = BufferData(indexCount, pIndex);

  for (uint32_t j = 0; j < triangleHeight; j++) {
    for (uint32_t i = 0; i < triangleWidth; i++) {
      *pIndex++ = j * subdivisions.x + i;
      *pIndex++ = (j + 1) * subdivisions.x + i;
      *pIndex++ = j * subdivisions.x + i + 1;
      *pIndex++ = j * subdivisions.x + i + 1;
      *pIndex++ = (j + 1) * subdivisions.x + i;
      *pIndex++ = (j + 1) * subdivisions.x + i + 1;
    }
  }

  // Topology
  result.Topology = PrimitiveTopology::TriangleList;

  return result;
}

MeshDescription CreateBoxInVSMesh() {
  MeshDescription result;

  // Indices
  uint32_t *pIndex = nullptr;
  // 3 indices per 2 triangle per 3 face
  result.Indices = BufferData(3 * 2 * 3, pIndex);

  // xz plane
  *pIndex++ = 0;
  *pIndex++ = 1;
  *pIndex++ = 2; // Triangle 1
  *pIndex++ = 2;
  *pIndex++ = 1;
  *pIndex++ = 3; // Triangle 2

  // zy plane
  *pIndex++ = 2;
  *pIndex++ = 6;
  *pIndex++ = 3; // Triangle 1
  *pIndex++ = 3;
  *pIndex++ = 6;
  *pIndex++ = 7; // Triangle 2

  // xy plane
  *pIndex++ = 1;
  *pIndex++ = 5;
  *pIndex++ = 3; // Triangle 1
  *pIndex++ = 3;
  *pIndex++ = 5;
  *pIndex++ = 7; // Triangle 2

  // Topology
  result.Topology = PrimitiveTopology::TriangleList;

  return result;
}

MeshDescription CreateCubeWithoutBottom(float size, XMFLOAT3 offset) {
  MeshDescription result;
  size = size / 2;

  // Vertices
  VertexPosition *pVertex = nullptr;
  result.Vertices = BufferData(8, pVertex);

  *pVertex++ = VertexPosition{XMFLOAT3{size, size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{size, size, size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, size, size}};

  *pVertex++ = VertexPosition{XMFLOAT3{size, -size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{size, -size, size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, -size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, -size, size}};
  for (int i = -(i32)result.Vertices.ItemCount(); i < 0; i++) {
    pVertex[i].Position.x += offset.x;
    pVertex[i].Position.y += offset.y;
    pVertex[i].Position.z += offset.z;
  }

  // Indices
  uint32_t *pIndex = nullptr;
  result.Indices = BufferData(5 * 2 * 3, pIndex);

  // Top Face (1, 2, 3, 4)
  *pIndex++ = 0;
  *pIndex++ = 2;
  *pIndex++ = 1; // Triangle 1
  *pIndex++ = 1;
  *pIndex++ = 2;
  *pIndex++ = 3; // Triangle 2

  // Front Face (1, 3, 5, 7)
  *pIndex++ = 0;
  *pIndex++ = 4;
  *pIndex++ = 2; // Triangle 1
  *pIndex++ = 2;
  *pIndex++ = 4;
  *pIndex++ = 6; // Triangle 2

  // Back Face (2, 4, 6, 8)
  *pIndex++ = 1;
  *pIndex++ = 3;
  *pIndex++ = 5; // Triangle 1
  *pIndex++ = 5;
  *pIndex++ = 3;
  *pIndex++ = 7; // Triangle 2

  // Left Face (3, 4, 7, 8)
  *pIndex++ = 2;
  *pIndex++ = 6;
  *pIndex++ = 3; // Triangle 1
  *pIndex++ = 3;
  *pIndex++ = 6;
  *pIndex++ = 7; // Triangle 2

  // Right Face (1, 2, 5, 6)
  *pIndex++ = 0;
  *pIndex++ = 1;
  *pIndex++ = 4; // Triangle 1
  *pIndex++ = 4;
  *pIndex++ = 1;
  *pIndex++ = 5; // Triangle 2

  // Topology
  result.Topology = PrimitiveTopology::TriangleList;

  return result;
}
