#pragma once

#include <cstdint>

struct OverlayText_t;
class QAngle;
struct Vector3;
struct matrix3x4_t;

inline constexpr char VDEBUG_OVERLAY_INTERFACE_VERSION[] = "VDebugOverlay004";

class IVDebugOverlay
{
public:
	virtual void AddEntityTextOverlay(int entityIndex, int lineOffset, float duration, int r, int g, int b, int a,
		const char* pFormat, ...) = 0;
	virtual void AddBoxOverlay(const Vector3& origin, const Vector3& mins, const Vector3& maxs, const QAngle& angles,
		int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void AddSphereOverlay(const Vector3& origin, float radius, int thetaSegments, int phiSegments,
		int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void AddTriangleOverlay(const Vector3& point1, const Vector3& point2, const Vector3& point3,
		int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void AddLineOverlay(const Vector3& start, const Vector3& end, int r, int g, int b,
		bool noDepthTest, float duration) = 0;
	virtual void AddSplineOverlay(const Vector3& start, const Vector3& end, int r, int g, int b,
		bool noDepthTest, float duration) = 0;
	virtual void AddTextOverlay(const Vector3& origin, int lineOffset, float duration, const char* pFormat, ...) = 0;
	virtual void AddTextOverlay(const Vector3& origin, float duration, const char* pFormat, ...) = 0;
	virtual void AddScreenTextOverlay(float x, float y, int lineOffset, float duration, int r, int g, int b, int a,
		const char* pText) = 0;
	virtual void AddScreenTextOverlay(float x, float y, float duration, int r, int g, int b, int a,
		const char* pText) = 0;
	virtual void AddSweptBoxOverlay(const Vector3& start, const Vector3& end, const Vector3& mins,
		const Vector3& maxs, const QAngle& angles, int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void AddGridOverlay(const Vector3& origin) = 0;
	virtual void AddCoordFrameOverlay(const matrix3x4_t& frame, float scale, const int* pColorTable) = 0;
	virtual OverlayText_t* GetFirstText() = 0;
	virtual OverlayText_t* GetNextText(const OverlayText_t* pCurrent) = 0;
	virtual bool IsTextOverlayDead(const OverlayText_t* pOverlay) = 0;
	virtual void ClearDeadTextOverlays() = 0;
	virtual void ClearAllOverlays() = 0;
	virtual std::uintptr_t DebugDebugOverlays(void* pOverlayRenderer, const std::uint32_t* pUnknownValue,
		float lineSpacing) = 0;
	virtual void AdvanceOverlayStage(int amount) = 0;
	virtual void UpdateOverlayRenderTick() = 0;
	virtual bool DebugDebugOverlaysEnabled() = 0;
	virtual void AddTextOverlayRGB(const Vector3& origin, int lineOffset, float duration, int r, int g, int b, int a,
		const char* pFormat, ...) = 0;
	virtual void AddTextOverlayRGB(const Vector3& origin, int lineOffset, float duration,
		float r, float g, float b, float a, const char* pFormat, ...) = 0;
	virtual void AddLineOverlayWithAlpha(const Vector3& start, const Vector3& end, int r, int g, int b, int a,
		bool noDepthTest, float duration) = 0;
	virtual void AddCapsuleOverlay(const Vector3& start, const Vector3& end, float radius,
		int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void ResetPersistentOverlayTicks() = 0;
	virtual void ClearOverlayBounds() = 0;
	virtual void SetOverlayBounds(const Vector3& mins, const Vector3& maxs) = 0;
	virtual bool GetOverlayBounds(Vector3& mins, Vector3& maxs) = 0;
	virtual bool HasOverlayBounds() = 0;
};

static_assert(sizeof(IVDebugOverlay) == sizeof(void*));
