#pragma once

#include "view_shared.h"

#include <cstddef>
#include <cstdint>

enum class ViewRenderMatrixType_t
{
	ViewProjection,
	CameraOverrideViewProjection,
};

struct CPitchDrift
{
	float m_PitchVelocity;
	bool m_NoDrift;
	std::uint8_t m_Pad0005[3];
	float m_DriftMove;
	std::uint32_t m_Pad000C;
	double m_LastStopTime;
};


class CViewRender
{
public:
	virtual void Init() = 0;
	virtual void LevelInit() = 0;
	virtual void LevelShutdown() = 0;
	virtual void Shutdown() = 0;
	virtual void OnRenderStart() = 0;
	virtual void FinalizeVisibilityState() = 0;
	virtual void SetupViewMatrices(const CViewRenderView* pView) = 0;
	virtual void Render(void* pRect) = 0;
	virtual void EndViewRenderJobs() = 0;
	virtual void RenderView(const CViewRenderView* pView, int clearFlags, int whatToDraw) = 0;
	virtual void StartPitchDrift() = 0;
	virtual void StopPitchDrift() = 0;
	virtual CViewSetup* GetPlayerViewSetup(int splitScreenSlot) = 0;
	virtual void GetViewportSize(float* pWidth, float* pHeight) = 0;
	virtual VMatrix* GetViewProjectionMatrix(ViewRenderMatrixType_t type) = 0;
	virtual bool IsViewProjectionMatrixAvailable(ViewRenderMatrixType_t type) = 0;
	virtual void BuildViewProjectionMatrix(const float* pOrigin, const float* pAngles,
		float fieldOfView) = 0;
	virtual void SetCheapWaterStartDistance(float distance) = 0;
	virtual void SetCheapWaterEndDistance(float distance) = 0;
	virtual void SetCurrentView(void* pEntity, const float* pOrigin, const float* pAngles) = 0;
	virtual void ClearCurrentView() = 0;
	virtual void GetWaterLODParams(float* pStartDistance, float* pEndDistance) = 0;
	virtual void DriftPitch() = 0;
	virtual void WriteSaveGameScreenshot(const char* pFilename) = 0;
	virtual void WriteSaveGameScreenshotOfSize(const char* pFilename, int width, int height) = 0;
	virtual float GetZNear() = 0;
	virtual void WaitForViewRenderJobsAndDraw(void* pViewRecord, int* pRenderFlags) = 0;
	virtual void BuildViewRenderLists(void* pViewRecord, const void* pAuxiliaryViewData) = 0;
	virtual void DrawViewRenderListsToRenderTarget(void* pRenderTarget, void* pViewRecord,
		float scale) = 0;
	virtual void FreezeFrame(float duration) = 0;
	virtual void SetRenderContextStateEnabled(bool enabled) = 0;
	virtual ~CViewRender() = default;

	CViewSetup m_PlayerViewSetup;
	bool m_IsRendering;
	std::uint8_t m_Pad0199[7];
	CPitchDrift m_PitchDrift;
	std::uint64_t m_ActiveViewState;
	std::uint8_t m_Pad01C0[0xA1200];
	CViewRenderView m_AuxiliaryView0;
	std::uint8_t m_PadA15C0[0x14000];
	CViewRenderView m_AuxiliaryView1;
	std::uint8_t m_PadB57C0[0x79500];
	CViewRenderView m_MainView;
	int m_CurrentDrawingEntityIndex;
	VMatrix m_CurrentViewTransform;
	Vector3 m_Pad12EF04;
	VMatrix m_ViewProjectionMatrix;
	VMatrix m_ViewProjectionMatrixWithCameraOverride;
	VMatrix* m_ViewProjectionMatrices[2];
	float m_ViewportWidth;
	float m_ViewportHeight;
	void* m_ViewStateMemory0;
	void* m_ViewStateMemory1;
	float m_CheapWaterStartDistance;
	float m_CheapWaterEndDistance;
	std::uint32_t m_Unknown12EFC0;
	bool m_RenderContextStateEnabled;
	std::uint8_t m_Pad12EFC5[3];
	float m_Unknown12EFC8;
	bool m_CameraOverrideActive;
	std::uint8_t m_Pad12EFCD[3];
	std::uint8_t m_CameraOverrideState[0x70];
	bool m_FreezeFrameActive;
	std::uint8_t m_Pad12F041[3];
	float m_FreezeFrameUntil;
	void* m_ViewStateMemory2;
};

static_assert(sizeof(ViewRenderMatrixType_t) == 0x4);
static_assert(sizeof(CPitchDrift) == 0x18);
static_assert(offsetof(CPitchDrift, m_PitchVelocity) == 0x0);
static_assert(offsetof(CPitchDrift, m_NoDrift) == 0x4);
static_assert(offsetof(CPitchDrift, m_DriftMove) == 0x8);
static_assert(offsetof(CPitchDrift, m_LastStopTime) == 0x10);
static_assert(sizeof(CViewRenderView) == 0x200);
static_assert(offsetof(CViewRenderView, m_ViewMatrix) == 0x40);
static_assert(offsetof(CViewRenderView, m_ProjectionMatrix) == 0x80);
static_assert(offsetof(CViewRenderView, m_ViewProjectionMatrix) == 0xC0);
static_assert(offsetof(CViewRenderView, m_UnjitteredViewMatrix) == 0x100);
static_assert(offsetof(CViewRenderView, m_UnjitteredProjectionMatrix) == 0x140);
static_assert(offsetof(CViewRenderView, m_TanHalfFovX) == 0x180);
static_assert(offsetof(CViewRenderView, m_ViewportX) == 0x190);
static_assert(offsetof(CViewRenderView, m_ViewportScale) == 0x1AC);
static_assert(offsetof(CViewRenderView, m_Orthographic) == 0x1B8);
static_assert(offsetof(CViewRenderView, m_ZFar) == 0x1C0);
static_assert(offsetof(CViewRenderView, m_TanHalfViewModelFovX) == 0x1C8);
static_assert(offsetof(CViewRenderView, m_TanHalfViewModelFovY) == 0x1D0);
static_assert(offsetof(CViewRenderView, m_UnreflectedOrigin) == 0x1D4);
static_assert(offsetof(CViewRenderView, m_DoBloomAndToneMapping) == 0x1EC);
static_assert(offsetof(CViewRenderView, m_OutputViewportX) == 0x1F0);
static_assert(offsetof(CViewRenderView, m_OutputViewportHeight) == 0x1FC);
static_assert(offsetof(CViewRender, m_PlayerViewSetup) == 0x8);
static_assert(offsetof(CViewRender, m_IsRendering) == 0x198);
static_assert(offsetof(CViewRender, m_PitchDrift) == 0x1A0);
static_assert(offsetof(CViewRender, m_AuxiliaryView0) == 0xA13C0);
static_assert(offsetof(CViewRender, m_AuxiliaryView1) == 0xB55C0);
static_assert(offsetof(CViewRender, m_MainView) == 0x12ECC0);
static_assert(offsetof(CViewRender, m_CurrentDrawingEntityIndex) == 0x12EEC0);
static_assert(offsetof(CViewRender, m_CurrentViewTransform) == 0x12EEC4);
static_assert(offsetof(CViewRender, m_ViewProjectionMatrix) == 0x12EF10);
static_assert(offsetof(CViewRender, m_ViewProjectionMatrixWithCameraOverride) == 0x12EF50);
static_assert(offsetof(CViewRender, m_ViewProjectionMatrices) == 0x12EF90);
static_assert(offsetof(CViewRender, m_ViewportWidth) == 0x12EFA0);
static_assert(offsetof(CViewRender, m_ViewStateMemory0) == 0x12EFA8);
static_assert(offsetof(CViewRender, m_CheapWaterStartDistance) == 0x12EFB8);
static_assert(offsetof(CViewRender, m_RenderContextStateEnabled) == 0x12EFC4);
static_assert(offsetof(CViewRender, m_CameraOverrideActive) == 0x12EFCC);
static_assert(offsetof(CViewRender, m_CameraOverrideState) == 0x12EFD0);
static_assert(offsetof(CViewRender, m_FreezeFrameActive) == 0x12F040);
static_assert(offsetof(CViewRender, m_FreezeFrameUntil) == 0x12F044);
static_assert(sizeof(CViewRender) == 0x12F050);
