#pragma once

// Render Line
inline void (*RenderLine)(const Vector3D& v1, const Vector3D& v2, Color c, bool bZBuffer);

// Render box
inline void (*RenderBox)(
	const Vector3D& vOrigin, const QAngle& angles, const Vector3D& vMins, const Vector3D& vMaxs, Color c, bool bZBuffer, bool bInsideOut);

// Render wireframe box
inline void (*RenderWireframeBox)(
	const Vector3D& vOrigin, const QAngle& angles, const Vector3D& vMins, const Vector3D& vMaxs, Color c, bool bZBuffer, bool bInsideOut);

// Render swept box
inline void (*RenderWireframeSweptBox)(
	const Vector3D& vStart, const Vector3D& vEnd, const QAngle& angles, const Vector3D& vMins, const Vector3D& vMaxs, Color c, bool bZBuffer);

// Render Triangle
inline void (*RenderTriangle)(const Vector3D& p1, const Vector3D& p2, const Vector3D& p3, Color c, bool bZBuffer);

// Render Axis
inline void (*RenderAxis)(const Vector3D& vOrigin, float flScale, bool bZBuffer);

// I dont know
inline void (*RenderUnknown)(const Vector3D& vUnk, float flUnk, bool bUnk);

// Render Sphere
inline void (*RenderSphere)(const Vector3D& vCenter, float flRadius, int nTheta, int nPhi, Color c, bool bZBuffer);
