#pragma once

class QAngle;
struct Vector3;

inline constexpr char VPHYSICS_DEBUG_OVERLAY_INTERFACE_VERSION[] = "VPhysicsDebugOverlay001";

class IVPhysicsDebugOverlay
{
public:
	virtual void AddEntityTextOverlay(int entityIndex, int lineOffset, float duration, int r, int g, int b, int a,
		const char* pFormat, ...) = 0;
	virtual void AddBoxOverlay(const Vector3& origin, const Vector3& mins, const Vector3& maxs, const QAngle& angles,
		int r, int g, int b, int a, float duration) = 0;
	virtual void AddTriangleOverlay(const Vector3& point1, const Vector3& point2, const Vector3& point3,
		int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void AddLineOverlay(const Vector3& start, const Vector3& end, int r, int g, int b,
		bool noDepthTest, float duration) = 0;
	virtual void AddTextOverlay(const Vector3& origin, float duration, const char* pFormat, ...) = 0;
	virtual void AddTextOverlay(const Vector3& origin, int lineOffset, float duration, const char* pFormat, ...) = 0;
	virtual void AddScreenTextOverlay(const float* pScreenPosition, int lineOffset, float duration,
		int r, int g, int b, int a, const char* pText) = 0;
	virtual void AddSweptBoxOverlay(const Vector3& start, const Vector3& end, const Vector3& mins,
		const Vector3& maxs, const QAngle& angles, int r, int g, int b, int a, float duration) = 0;
	virtual void AddTextOverlayRGB(const Vector3& origin, int lineOffset, float duration,
		float r, float g, float b, float a, const char* pFormat, ...) = 0;
};

static_assert(sizeof(IVPhysicsDebugOverlay) == sizeof(void*));
