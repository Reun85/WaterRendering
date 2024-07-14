#include "pch.h"
#include "Camera.h"
#include <DirectXMath.h>

Camera::Camera() {
  SetView(XMVectorSet(0.0f, 20.0f, 20.0f, 0), XMVectorSet(0.0f, 0.0f, 0.0f, 0),
          XMVectorSet(0.0f, 1.0f, 0.0f, 0));
}

Camera::Camera(XMVECTOR _eye, XMVECTOR _at, XMVECTOR _worldUp) {
  SetView(_eye, _at, _worldUp);
}

Camera::~Camera() {}

void Camera::SetView(XMVECTOR _eye, XMVECTOR _at, XMVECTOR _worldUp) {
  static const float eps = 0.001f;
  m_eye = _eye;
  m_at = _at;
  m_worldUp = _worldUp;

  XMVECTOR toAim = m_at - m_eye;
  m_distance = XMVectorGetX(XMVector3Length(toAim));

  m_u = atan2f(XMVectorGetZ(toAim), XMVectorGetX(toAim));
  m_v = acosf(XMVectorGetY(toAim) / m_distance);
  UpdateParams();

  m_viewDirty = true;
}

void Camera::SetProj(float _angle, float _aspect, float _zn, float _zf) {
  m_angle = _angle;
  m_aspect = _aspect;
  m_zNear = _zn;
  m_zFar = _zf;

  m_projectionDirty = true;
}

void Camera::LookAt(XMVECTOR _at) { SetView(m_eye, _at, m_up); }

void Camera::SetAngle(const float _angle) noexcept {
  m_angle = _angle;
  m_projectionDirty = true;
}

void Camera::SetAspect(const float _aspect) noexcept {
  m_aspect = _aspect;
  m_projectionDirty = true;
}

void Camera::SetZNear(const float _zn) noexcept {
  m_zNear = _zn;
  m_projectionDirty = true;
}

void Camera::SetZFar(const float _zf) noexcept {
  m_zFar = _zf;
  m_projectionDirty = true;
}

bool Camera::Update(float _deltaTime) {
  if (m_goForward != 0.0f || m_goRight != 0.0f || m_goUp != 0.0f) {
    XMVECTOR deltaPosition =
        (m_goForward * m_forward + m_goRight * m_right + m_goUp * m_up) *
        m_speed * _deltaTime;
    m_eye += deltaPosition;
    m_at += deltaPosition;
    m_viewDirty = true;
  }

  if (m_viewDirty) {
    m_viewMatrix = XMMatrixLookAtRH(m_eye, m_at, m_worldUp);
  }

  if (m_projectionDirty) {
    m_matProj = XMMatrixPerspectiveFovRH(m_angle, m_aspect, m_zNear, m_zFar);
  }
  if (m_viewDirty || m_projectionDirty) {
    // m_matViewProj = m_matProj*m_viewMatrix;
    m_matViewProj = m_viewMatrix * m_matProj;
    m_viewDirty = false;
    m_projectionDirty = false;
    return true;
  }
  return false;
}

void Camera::UpdateUV(float du, float dv) {
  m_u += du;
  m_v = XMMax(0.1f, XMMin(3.1f, m_v + dv));

  UpdateParams();
}

void Camera::UpdateDistance(float dDistance) {
  m_distance += dDistance;
  UpdateParams();
}

void Camera::SetFirstPerson(bool _firstperson) {
  firstperson = _firstperson;
  UpdateParams();
}

void Camera::UpdateParams() {
  XMVECTOR lookDirection =
      XMVectorSet(cosf(m_u) * sinf(m_v), cosf(m_v), sinf(m_u) * sinf(m_v), 0);

  if (firstperson) {
    m_forward = XMVector3Normalize(lookDirection);
    m_at = m_forward * m_distance + m_eye;
    m_right = XMVector3Normalize(XMVector3Cross(m_forward, m_worldUp));
    m_up = XMVector3Normalize(XMVector3Cross(m_right, m_forward));
  } else {
    m_eye = m_at - m_distance * lookDirection;

    m_up = m_worldUp;
    m_right = XMVector3Normalize(XMVector3Cross(lookDirection, m_worldUp));

    m_forward = XMVector3Cross(m_up, m_right);
  }
  m_viewDirty = true;
}

void Camera::SetSpeed(float _val) { m_speed = _val; }

void Camera::Resize(int _w, int _h) { SetAspect(_w / (float)_h); }

void Camera::KeyboardDown(const KeyEventArgs &key) {
  using namespace winrt::Windows::System;

  switch (key.VirtualKey()) {
  case VirtualKey::Shift:
  case VirtualKey::LeftShift:
  case VirtualKey::RightShift:
    if (!m_slow) {
      m_slow = true;
      m_speed /= 4.0f;
    }
    break;
  case VirtualKey::Control:
  case VirtualKey::RightControl:
  case VirtualKey::LeftControl:
    if (!m_slow2) {
      m_slow2 = true;
      m_speed /= 4.0f;
    }
    break;
  case VirtualKey::W:
    m_goForward = 1;
    break;
  case VirtualKey::S:
    m_goForward = -1;
    break;
  case VirtualKey::A:
    m_goRight = -1;
    break;
  case VirtualKey::D:
    m_goRight = 1;
    break;
  case VirtualKey::E:
    m_goUp = 1;
    break;
  case VirtualKey::Q:
    m_goUp = -1;
    break;
  }
}

void Camera::KeyboardUp(const KeyEventArgs &key) {
  using namespace winrt::Windows::System;

  switch (key.VirtualKey()) {
  case VirtualKey::Shift:
  case VirtualKey::LeftShift:
  case VirtualKey::RightShift:
    if (m_slow) {
      m_slow = false;
      m_speed *= 4.0f;
    }
    break;
  case VirtualKey::Control:
  case VirtualKey::RightControl:
  case VirtualKey::LeftControl:
    if (m_slow2) {
      m_slow2 = false;
      m_speed *= 4.0f;
    }
    break;

  case VirtualKey::W:
  case VirtualKey::S:
    m_goForward = 0;
    break;
  case VirtualKey::A:
  case VirtualKey::D:
    m_goRight = 0;
    break;
  case VirtualKey::E:
  case VirtualKey::Q:
    m_goUp = 0;
    break;
  }
}

void Camera::MouseMove(const PointerEventArgs &mouse) {
  using CursorPos = decltype(mouse.CurrentPoint().Position());
  auto point = mouse.CurrentPoint().Position();
  static CursorPos prev = point;
  if (mouse.CurrentPoint().Properties().IsLeftButtonPressed()) {
    UpdateUV((point.X - prev.X) / 600.0f, (point.Y - prev.Y) / 600.0f);
  }
  prev = point;
  if (!firstperson &&
      mouse.CurrentPoint().Properties().IsRightButtonPressed()) {
    UpdateDistance(mouse.CurrentPoint().Properties().YTilt() / 600.0f);
  }
}

void Camera::MouseWheel(const PointerEventArgs &wheel) {
  UpdateDistance(
      static_cast<float>(wheel.CurrentPoint().Properties().MouseWheelDelta()) *
      m_speed / -300.0f);
}