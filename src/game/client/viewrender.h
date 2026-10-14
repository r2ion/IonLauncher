#pragma once

#include "game/client/iviewrender.h"

#include <cstddef>
#include <cstdint>

struct CPitchDrift
{
	float pitchvel;
	bool nodrift;
	std::uint8_t m_Pad0005[3];
	float driftmove;
	std::uint32_t m_Pad000C;
	double laststop;
};


class CViewRender : public IViewRender
{
public:
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
	Vector3D m_Pad12EF04;
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
