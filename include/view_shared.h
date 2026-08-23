#pragma once

#include "mathlib/vector.h"
#include "mathlib/vmatrix.h"

#include <cstddef>
#include <cstdint>

enum CViewSetupFlags_t : std::uint8_t
{
	ViewSetupOffCenter = 1 << 0,
	ViewSetupRenderToSubrect = 1 << 1,
	ViewSetupBloomAndToneMapping = 1 << 2,
	ViewSetupDepthOfField = 1 << 3,
	ViewSetupHdrTarget = 1 << 4,
	ViewSetupDrawWorldNormal = 1 << 5,
	ViewSetupCullFrontFaces = 1 << 6,
	ViewSetupCacheFullSceneState = 1 << 7,
};

struct CViewSetup
{
	int m_X;
	int m_Y;
	int m_Width;
	int m_Height;
	bool m_Ortho;
	bool m_UseCustomViewMatrix;
	bool m_ViewToProjectionOverride;
	bool m_RenderToSubrectOfLargerScreen;
	float m_OrthoLeft;
	float m_OrthoTop;
	float m_OrthoRight;
	float m_OrthoBottom;
	float m_CustomViewMatrix[3][4];
	VMatrix m_CustomProjectionMatrix;
	VMatrix m_ViewToProjection;
	float m_TanHalfFovX;
	float m_TanHalfFovY;
	float m_TanHalfViewModelFovX;
	float m_TanHalfViewModelFovY;
	Vector3 m_Origin;
	QAngle m_Angles;
	float m_ZNear;
	float m_ZFar;
	float m_ZNearViewmodel;
	float m_ZFarViewmodel;
	float m_AspectRatio;
	float m_NearBlurDepth;
	float m_NearFocusDepth;
	float m_FarFocusDepth;
	float m_FarBlurDepth;
	float m_NearBlurRadius;
	float m_FarBlurRadius;
	int m_DepthOfFieldQuality;
	Vector3 m_ShutterOpenPosition;
	QAngle m_ShutterOpenAngles;
	Vector3 m_ShutterClosePosition;
	QAngle m_ShutterCloseAngles;
	float m_OffCenterTop;
	float m_OffCenterBottom;
	float m_OffCenterLeft;
	float m_OffCenterRight;
	int m_EdgeBlur;
	CViewSetupFlags_t m_Flags;
	std::uint8_t m_ExtendedFlags[3];
	Vector3 m_UnreflectedOrigin;
	QAngle m_UnreflectedAngles;
	bool m_Unknown018C;
	std::uint8_t m_Pad018D[3];
};

static_assert(sizeof(CViewSetup) == 0x190);
static_assert(offsetof(CViewSetup, m_X) == 0x0);
static_assert(offsetof(CViewSetup, m_Ortho) == 0x10);
static_assert(offsetof(CViewSetup, m_OrthoLeft) == 0x14);
static_assert(offsetof(CViewSetup, m_CustomViewMatrix) == 0x24);
static_assert(offsetof(CViewSetup, m_CustomProjectionMatrix) == 0x54);
static_assert(offsetof(CViewSetup, m_ViewToProjection) == 0x94);
static_assert(offsetof(CViewSetup, m_TanHalfFovX) == 0xD4);
static_assert(offsetof(CViewSetup, m_TanHalfFovY) == 0xD8);
static_assert(offsetof(CViewSetup, m_TanHalfViewModelFovX) == 0xDC);
static_assert(offsetof(CViewSetup, m_TanHalfViewModelFovY) == 0xE0);
static_assert(offsetof(CViewSetup, m_Origin) == 0xE4);
static_assert(offsetof(CViewSetup, m_Angles) == 0xF0);
static_assert(offsetof(CViewSetup, m_ZNear) == 0xFC);
static_assert(offsetof(CViewSetup, m_AspectRatio) == 0x10C);
static_assert(offsetof(CViewSetup, m_NearBlurDepth) == 0x110);
static_assert(offsetof(CViewSetup, m_NearFocusDepth) == 0x114);
static_assert(offsetof(CViewSetup, m_FarFocusDepth) == 0x118);
static_assert(offsetof(CViewSetup, m_FarBlurDepth) == 0x11C);
static_assert(offsetof(CViewSetup, m_NearBlurRadius) == 0x120);
static_assert(offsetof(CViewSetup, m_FarBlurRadius) == 0x124);
static_assert(offsetof(CViewSetup, m_DepthOfFieldQuality) == 0x128);
static_assert(offsetof(CViewSetup, m_ShutterOpenPosition) == 0x12C);
static_assert(offsetof(CViewSetup, m_ShutterOpenAngles) == 0x138);
static_assert(offsetof(CViewSetup, m_ShutterClosePosition) == 0x144);
static_assert(offsetof(CViewSetup, m_ShutterCloseAngles) == 0x150);
static_assert(offsetof(CViewSetup, m_OffCenterTop) == 0x15C);
static_assert(offsetof(CViewSetup, m_EdgeBlur) == 0x16C);
static_assert(offsetof(CViewSetup, m_Flags) == 0x170);
static_assert(offsetof(CViewSetup, m_UnreflectedOrigin) == 0x174);
static_assert(offsetof(CViewSetup, m_UnreflectedAngles) == 0x180);
static_assert(offsetof(CViewSetup, m_Unknown018C) == 0x18C);

struct CViewRenderView
{
	float m_ProjectionParameters[16];
	VMatrix m_ViewMatrix;
	VMatrix m_ProjectionMatrix;
	VMatrix m_ViewProjectionMatrix;
	VMatrix m_UnjitteredViewMatrix;
	VMatrix m_UnjitteredProjectionMatrix;
	float m_TanHalfFovX;
	float m_TanHalfFovY;
	float m_ZNear;
	float m_ProjectionOffset;
	int m_ViewportX;
	int m_ViewportY;
	int m_ViewportWidth;
	int m_ViewportHeight;
	int m_UnscaledViewportX;
	int m_UnscaledViewportY;
	int m_UnscaledViewportWidth;
	float m_ViewportScale;
	std::uint8_t m_Pad01B0[8];
	int m_Orthographic;
	std::uint8_t m_Pad01BC[4];
	float m_ZFar;
	float m_TanHalfFovYSecondary;
	float m_TanHalfViewModelFovX;
	float m_ZFarSecondary;
	float m_TanHalfViewModelFovY;
	Vector3 m_UnreflectedOrigin;
	QAngle m_UnreflectedAngles;
	bool m_DoBloomAndToneMapping;
	std::uint8_t m_Pad01ED[3];
	int m_OutputViewportX;
	int m_OutputViewportY;
	int m_OutputViewportWidth;
	int m_OutputViewportHeight;
};

static_assert(sizeof(CViewRenderView) == 0x200);
static_assert(offsetof(CViewRenderView, m_ViewMatrix) == 0x40);
static_assert(offsetof(CViewRenderView, m_ProjectionMatrix) == 0x80);
static_assert(offsetof(CViewRenderView, m_ViewProjectionMatrix) == 0xC0);
static_assert(offsetof(CViewRenderView, m_UnjitteredViewMatrix) == 0x100);
static_assert(offsetof(CViewRenderView, m_UnjitteredProjectionMatrix) == 0x140);
static_assert(offsetof(CViewRenderView, m_TanHalfFovX) == 0x180);
static_assert(offsetof(CViewRenderView, m_ViewportX) == 0x190);
static_assert(offsetof(CViewRenderView, m_UnscaledViewportX) == 0x1A0);
static_assert(offsetof(CViewRenderView, m_Orthographic) == 0x1B8);
static_assert(offsetof(CViewRenderView, m_ZFar) == 0x1C0);
static_assert(offsetof(CViewRenderView, m_UnreflectedOrigin) == 0x1D4);
static_assert(offsetof(CViewRenderView, m_UnreflectedAngles) == 0x1E0);
static_assert(offsetof(CViewRenderView, m_DoBloomAndToneMapping) == 0x1EC);
static_assert(offsetof(CViewRenderView, m_OutputViewportX) == 0x1F0);
static_assert(offsetof(CViewRenderView, m_OutputViewportHeight) == 0x1FC);
