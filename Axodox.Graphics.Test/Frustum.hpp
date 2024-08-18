#pragma once
#include "pch.h"
#include <DirectXMath.h>

struct Plane {
  // unit vector
  XMVECTOR normal = {0.f, 1.f, 0.f};

  // distance from origin to the nearest point in the plane
  float distance = 0.f;

  Plane() = default;
  Plane(const XMVECTOR &p1, const XMVECTOR &norm)
      : normal(XMVector3Normalize(norm)),
        distance(XMVectorGetX(XMVector3Dot(normal, p1))) {}

  float getSignedDistanceToPlane(const XMVECTOR &point) const {
    return XMVectorGetX(XMVector3Dot(normal, point)) - distance;
  }
};

struct Frustum {
  Plane topFace;
  Plane bottomFace;

  Plane rightFace;
  Plane leftFace;

  Plane farFace;
  Plane nearFace;
};

struct Volume {
  virtual bool isOnFrustum(const Frustum &camFrustum,
                           const XMMATRIX &mMatrix) const = 0;
};

struct AABB : public Volume {
  // This is padded due to vtable. Cool
  XMVECTOR center{0.f, 0.f, 0.f, 1};
  XMVECTOR extents{0.f, 0.f, 0.f, 1};

  AABB(const XMVECTOR &min, const XMVECTOR &max)
      : Volume{}, center{(max + min) * 0.5f},
        extents{XMVectorGetX(max) - XMVectorGetX(center),
                XMVectorGetY(max) - XMVectorGetZ(center),
                XMVectorGetZ(max) - XMVectorGetZ(center)} {}

  AABB(const XMVECTOR &inCenter, float iI, float iJ, float iK)
      : Volume{}, center{inCenter}, extents{iI, iJ, iK} {}

  bool isOnOrForwardPlane(const Plane &plane) const {
    // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
    const float r =
        XMVectorGetX(extents) * std::abs(XMVectorGetX(plane.normal)) +
        XMVectorGetY(extents) * std::abs(XMVectorGetY(plane.normal)) +
        XMVectorGetZ(extents) * std::abs(XMVectorGetZ(plane.normal));

    return -r <= plane.getSignedDistanceToPlane(center);
  }

  bool isOnFrustum(const Frustum &camFrustum,
                   const XMMATRIX &mMatrix) const final {
    // Get global scale thanks to our transform

    const XMVECTOR globalCenter = XMVector3TransformCoord(center, mMatrix);

    // Scaled orientation

    const XMVECTOR right =
        XMVector3Normalize(mMatrix.r[0]) * XMVectorGetX(extents);
    const XMVECTOR up =
        XMVector3Normalize(mMatrix.r[1]) * XMVectorGetY(extents);
    const XMVECTOR forward =
        XMVector3Normalize(mMatrix.r[2]) * XMVectorGetZ(extents);

    const float newIi =
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{1.f, 0.f, 0.f}, right))) +
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{1.f, 0.f, 0.f}, up))) +
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{1.f, 0.f, 0.f}, forward)));

    const float newIj =
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{0.f, 1.f, 0.f}, right))) +
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{0.f, 1.f, 0.f}, up))) +
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{0.f, 1.f, 0.f}, forward)));

    const float newIk =
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{0.f, 0.f, 1.f}, right))) +
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{0.f, 0.f, 1.f}, up))) +
        std::abs(XMVectorGetX(XMVector3Dot(XMVECTOR{0.f, 0.f, 1.f}, forward)));

    // We not need to divise scale because it's based on the half extention of
    // the AABB
    const AABB globalAABB(globalCenter, newIi, newIj, newIk);

    return (globalAABB.isOnOrForwardPlane(camFrustum.leftFace) &&
            globalAABB.isOnOrForwardPlane(camFrustum.rightFace) &&
            globalAABB.isOnOrForwardPlane(camFrustum.topFace) &&
            globalAABB.isOnOrForwardPlane(camFrustum.bottomFace) &&
            globalAABB.isOnOrForwardPlane(camFrustum.nearFace) &&
            globalAABB.isOnOrForwardPlane(camFrustum.farFace));
  };
};
