#pragma once

#include "pch.h"
using namespace winrt::Windows::UI::Core;
using namespace DirectX;

struct Frustum;

struct ViewFrustumCoordinates {
  constexpr static std::pair<f32, f32> xRange = {-1.f, 1.f};
  constexpr static std::pair<f32, f32> yRange = {-1.f, 1.f};
  constexpr static std::pair<f32, f32> zRange = {0.f, 1.f};
};

class Camera {
public:
  Camera();
  Camera(XMVECTOR _eye, XMVECTOR _at, XMVECTOR _worldup);

  ~Camera();

  inline XMVECTOR GetEye() const { return m_eye; }
  inline XMVECTOR GetAt() const { return m_at; }
  inline XMVECTOR GetWorldUp() const { return m_worldUp; }
  inline XMVECTOR GetForward() const { return m_forward; }

  inline XMMATRIX GetViewMatrix() const { return m_viewMatrix; }
  inline XMMATRIX GetProj() const { return m_matProj; }
  inline XMMATRIX GetViewProj() const { return m_matViewProj; }

  inline XMMATRIX GetINVView() const { return m_INVview; }
  inline XMMATRIX GetINVProj() const { return m_INVproj; }
  inline XMMATRIX GetINVViewProj() const { return m_INVviewProj; }

  std::array<XMVECTOR, 8> GetFrustumCorners() const;

  Frustum GetFrustum() const;
  // Returns true if the view has changed.
  bool Update(float _deltaTime);

  void SetView(XMVECTOR _eye, XMVECTOR _at, XMVECTOR _up);
  void LookAt(XMVECTOR _at);

  inline float GetAngle() const { return m_angle; }
  void SetAngle(const float _angle) noexcept;
  inline float GetAspect() const { return m_aspect; }
  void SetAspect(const float _aspect) noexcept;
  inline float GetZNear() const { return m_zNear; }
  void SetZNear(const float _zn) noexcept;
  inline float GetZFar() const { return m_zFar; }
  void SetZFar(const float _zf) noexcept;

  void SetEye(XMVECTOR eye);
  void SetLookAt(XMVECTOR at);

  void SetProj(float _angle, float _aspect, float _zn, float _zf);

  float GetBaseSpeed() const { return m_speed; }
  void SetBaseSpeed(float _val);

  float GetDistance() const { return m_distance; }
  void SetDistanceFromAt(float);

  void Resize(int _w, int _h);

  void SetFirstPerson(bool _firstperson);
  bool GetFirstPerson() const { return firstperson; }

  void KeyboardDown(const KeyEventArgs &key);
  void KeyboardUp(const KeyEventArgs &key);
  void MouseMove(const PointerEventArgs &mouse);
  void MouseWheel(const PointerEventArgs &wheel);

  void DrawImGui(bool exclusiveWindow = true);

private:
  // Updates the UV.
  void UpdateUV(float du, float dv);

  // Updates the distance.
  void UpdateDistance(float dDistance);

  // Updates the underlying parameters.
  void UpdateParams();

  inline float GetSpeed() const;

  // projection matrix
  XMMATRIX m_matProj;

  // Combined view-projection matrix of the camera
  XMMATRIX m_matViewProj;
  // The view matrix of the camera
  XMMATRIX m_viewMatrix;

  XMMATRIX m_INVview;
  XMMATRIX m_INVproj;
  XMMATRIX m_INVviewProj;

  // The camera position.
  XMVECTOR m_eye;

  // The vector pointing upwards
  XMVECTOR m_worldUp;

  // The camera look at point.
  XMVECTOR m_at;

  // The unit vector pointing towards the viewing direction.
  XMVECTOR m_forward;

  // The unit vector pointing to the 'right'
  XMVECTOR m_right;

  // The vector pointing upwards
  XMVECTOR m_up;

  //  The traversal speed of the camera
  float m_speed = 16.0f;
  bool firstperson = false;

  bool m_slow = false;
  bool m_accelerated = false;

  // The u spherical coordinate of the spherical coordinate pair (u,v) denoting
  // the current viewing direction from the view position m_eye.
  float m_u;

  // The v spherical coordinate of the spherical coordinate pair (u,v) denoting
  // the current viewing direction from the view position m_eye.
  float m_v;

  // The distance of the look at point from the camera.
  float m_distance;

  float m_goForward = 0.0f;
  float m_goRight = 0.0f;
  float m_goUp = 0.0f;

  // view matrix needs recomputation
  bool m_viewDirty = true;

  // projection parameters
  float m_zNear = 0.1f;
  float m_zFar = 1000.0f;

  float m_angle = XMConvertToRadians(27.0f);
  float m_aspect = 640.0f / 480.0f;

  // projection matrix needs recomputation
  bool m_projectionDirty = true;
};
