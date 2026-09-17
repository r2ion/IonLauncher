#pragma once

class QAngle;
class Vector3D;

inline constexpr char VPHYSICS_DEBUG_OVERLAY_INTERFACE_VERSION[] = "VPhysicsDebugOverlay001";

class IVPhysicsDebugOverlay
{
public:
	virtual void AddEntityTextOverlay(int entityIndex, int lineOffset, float duration, int r, int g, int b, int a,
		const char* pFormat, ...) = 0;
	virtual void AddBoxOverlay(const Vector3D& origin, const Vector3D& mins, const Vector3D& maxs, const QAngle& angles,
		int r, int g, int b, int a, float duration) = 0;
	virtual void AddTriangleOverlay(const Vector3D& point1, const Vector3D& point2, const Vector3D& point3,
		int r, int g, int b, int a, bool noDepthTest, float duration) = 0;
	virtual void AddLineOverlay(const Vector3D& start, const Vector3D& end, int r, int g, int b,
		bool noDepthTest, float duration) = 0;
	virtual void AddTextOverlay(const Vector3D& origin, float duration, const char* pFormat, ...) = 0;
	virtual void AddTextOverlay(const Vector3D& origin, int lineOffset, float duration, const char* pFormat, ...) = 0;
	virtual void AddScreenTextOverlay(const float* pScreenPosition, int lineOffset, float duration,
		int r, int g, int b, int a, const char* pText) = 0;
	virtual void AddSweptBoxOverlay(const Vector3D& start, const Vector3D& end, const Vector3D& mins,
		const Vector3D& maxs, const QAngle& angles, int r, int g, int b, int a, float duration) = 0;
	virtual void AddTextOverlayRGB(const Vector3D& origin, int lineOffset, float duration,
		float r, float g, float b, float a, const char* pFormat, ...) = 0;
};

static_assert(sizeof(IVPhysicsDebugOverlay) == sizeof(void*));
