#pragma once

#include "pch.h"
using namespace winrt::Windows::UI::Core;
using namespace DirectX;

class Camera {
public:
  Camera();
  Camera(XMVECTOR _eye, XMVECTOR _at, XMVECTOR _worldup);

  ~Camera();

  inline XMVECTOR GetEye() const { return m_eye; }
  inline XMVECTOR GetAt() const { return m_at; }
  inline XMVECTOR GetWorldUp() const { return m_worldUp; }

  inline XMMATRIX GetViewMatrix() const { return m_viewMatrix; }
  inline XMMATRIX GetProj() const { return m_matProj; }
  inline XMMATRIX GetViewProj() const { return m_matViewProj; }

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

  void SetProj(float _angle, float _aspect, float _zn, float _zf);

  void SetSpeed(float _val);
  void Resize(int _w, int _h);

  void SetFirstPerson(bool _firstperson);
  bool GetFirstPerson() const { return firstperson; }

  void KeyboardDown(const KeyEventArgs &key);
  void KeyboardUp(const KeyEventArgs &key);
  void MouseMove(const PointerEventArgs &mouse);
  void MouseWheel(const PointerEventArgs &wheel);

private:
  bool firstperson = false;
  // Updates the UV.
  void UpdateUV(float du, float dv);

  // Updates the distance.
  void UpdateDistance(float dDistance);

  // Updates the underlying parameters.
  void UpdateParams();

  //  The traversal speed of the camera
  float m_speed = 2.0f;

  bool m_slow = false;
  bool m_slow2 = false;

  // The camera position.
  XMVECTOR m_eye;

  // The vector pointing upwards
  XMVECTOR m_worldUp;

  // The camera look at point.
  XMVECTOR m_at;

  // The u spherical coordinate of the spherical coordinate pair (u,v) denoting
  // the current viewing direction from the view position m_eye.
  float m_u;

  // The v spherical coordinate of the spherical coordinate pair (u,v) denoting
  // the current viewing direction from the view position m_eye.
  float m_v;

  // The distance of the look at point from the camera.
  float m_distance;

  // The unit vector pointing towards the viewing direction.
  XMVECTOR m_forward;

  // The unit vector pointing to the 'right'
  XMVECTOR m_right;

  // The vector pointing upwards
  XMVECTOR m_up;

  float m_goForward = 0.0f;
  float m_goRight = 0.0f;
  float m_goUp = 0.0f;

  // view matrix needs recomputation
  bool m_viewDirty = true;

  // The view matrix of the camera
  XMMATRIX m_viewMatrix;

  // projection parameters
  float m_zNear = 0.01f;
  float m_zFar = 1000.0f;

  float m_angle = XMConvertToRadians(27.0f);
  float m_aspect = 640.0f / 480.0f;

  // projection matrix needs recomputation
  bool m_projectionDirty = true;

  // projection matrix
  XMMATRIX m_matProj;

  // Combined view-projection matrix of the camera
  XMMATRIX m_matViewProj;
};
