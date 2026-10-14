#pragma once

#include "view_shared.h"

enum class ViewRenderMatrixType_t
{
	ViewProjection,
	CameraOverrideViewProjection,
};

class IViewRender
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
};
