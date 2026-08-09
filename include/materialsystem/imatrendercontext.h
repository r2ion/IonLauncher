#pragma once

#include "tier1/refcount.h"

#include <cstdint>
#include <type_traits>

class IMaterial;
class ITexture;
struct MaterialLightingState_t;

class IMatRenderContext : public IRefCounted
{
public:
	virtual void SetRenderTargetAndMip(ITexture *texture, std::uint16_t mipLevel) = 0; // 2
	virtual void PushRenderTarget(ITexture *texture) = 0; // 3
	virtual void PopRenderTarget() = 0; // 4
	virtual void SetGlobalRenderContextPointer5(void *value) = 0; // 5
	virtual void ClearGlobalRenderContextPointer6() = 0; // 6
	virtual void CopyRenderTargetToTexture(ITexture *texture) = 0; // 7
	virtual void CopyRenderTargetToTextureEx(ITexture *texture, int renderTargetId,
		const void *sourceRect, const void *destinationRect) = 0; // 8
	virtual void CopyTextureToRenderTarget(ITexture *texture) = 0; // 9
	virtual void CopyTextureToRenderTargetEx(int renderTargetId, ITexture *texture,
		const void *sourceRect, const void *destinationRect) = 0; // 10
	virtual void SetRenderTargetMode11(int mode) = 0; // 11
	virtual ITexture * GetRenderTarget() = 0; // 12
	virtual ITexture * GetLocalCubemap() = 0; // 13
	virtual void BindLocalCubemap(ITexture *texture) = 0; // 14
	virtual void GetRenderTargetDimensions(int *width, int *height) const = 0; // 15
	virtual void Bind(IMaterial *material, void *proxyData = nullptr) = 0; // 16
	virtual void BindLightmapPage(unsigned int lightmapPageId) = 0; // 17
	virtual void SetIndexedRenderResource18(unsigned int index, ITexture *texture) = 0; // 18
	virtual unsigned int GetIndexedRenderResourceHandle19(unsigned int index) = 0; // 19
	virtual void DepthRange(float zNear, float zFar) = 0; // 20
	virtual void SetGlobalFloatState21(float value) = 0; // 21
	virtual void ClearBuffers(bool clearColor, bool clearDepth, bool clearStencil = false) = 0; // 22
	virtual void ReadPixels(int x, int y, int width, int height, unsigned char *data,
		int destinationFormat, bool readBackBuffer) = 0; // 23
	virtual void SetLightingState(const MaterialLightingState_t *state) = 0; // 24
	virtual void SetGlobalFloatState25(float value) = 0; // 25
	virtual void SetLights(const void *lights, int count) = 0; // 26
	virtual void SetIndexedRenderVector27(int index, const void *value) = 0; // 27
	virtual void GetIndexedRenderVector28(int index, float *value) = 0; // 28
	virtual std::uint64_t DispatchRenderState29() = 0; // 29
	virtual void SelectRenderStateIndex30(std::uint16_t index) = 0; // 30
	virtual void UpdateRenderStateIndex31(std::uint16_t index, const float *value) = 0; // 31
	virtual std::uint64_t DispatchRenderState32(const void *first, const void *second) = 0; // 32
	virtual void RemoveRenderStateMapping33(const void *first, const void *second) = 0; // 33
	virtual std::int16_t FindRenderStateMapping34(const void *key) = 0; // 34
	virtual std::uint64_t SetRenderStateMapping35(std::uint16_t index, const float *value,
		int valueType) = 0; // 35
	virtual std::uint64_t QueryRenderState36() = 0; // 36
	virtual void * GetRenderStateStorage37() = 0; // 37
	virtual void NullSub38() = 0; // 38
	virtual void NullSub39() = 0; // 39
	virtual void NullSub40() = 0; // 40
	virtual void NullSub41() = 0; // 41
	virtual void NullSub42() = 0; // 42
	virtual void NullSub43() = 0; // 43
	virtual void * GetDynamicMesh(IMaterial *autoBind) = 0; // 44
	virtual void SetFrameBufferCopyTexture(ITexture *texture, int textureIndex = 0) = 0; // 45
	virtual ITexture * GetFrameBufferCopyTexture(int textureIndex) = 0; // 46
	virtual void PushMatrixIdentity(int matrixMode) = 0; // 47
	virtual void PushMatrix(int matrixMode) = 0; // 48
	virtual void PushMatrix3x4(int matrixMode, const float *matrix) = 0; // 49
	virtual void PushMatrix4x4(int matrixMode, const float *matrix) = 0; // 50
	virtual void PushMultipliedMatrix4x4(int matrixMode, const float *matrix) = 0; // 51
	virtual void PopMatrix(int matrixMode) = 0; // 52
	virtual void LoadIdentity(int matrixMode) = 0; // 53
	virtual void LoadMatrix3x4(int matrixMode, const float *matrix) = 0; // 54
	virtual void LoadMatrix4x4(int matrixMode, const float *matrix) = 0; // 55
	virtual void LoadBoneMatrix(const float *matrix, std::uint16_t boneIndex) = 0; // 56
	virtual void Viewport(int x, int y, int width, int height) = 0; // 57
	virtual void GetViewport(int *x, int *y, int *width, int *height) const = 0; // 58
	virtual void CullMode(int cullMode) = 0; // 59
	virtual void SetCullModeOverride60(bool enabled) = 0; // 60
	virtual bool ApplyRenderState61(const void *state) = 0; // 61
	virtual bool PushRenderState62(const void *state) = 0; // 62
	virtual bool PopRenderState63() = 0; // 63
	virtual void PushBooleanRenderState64(bool enabled) = 0; // 64
	virtual void PopBooleanRenderState65() = 0; // 65
	virtual void SelectRenderStateVariant66(bool alternate) = 0; // 66
	virtual void NullSub67() = 0; // 67
	virtual void DispatchRenderState68() = 0; // 68
	virtual void SetRenderStateFlag69(bool enabled) = 0; // 69
	virtual void SetRenderStateClippingEnabled(bool enabled) = 0; // 70
	virtual void SetRenderStateBit0(bool enabled) = 0; // 71
	virtual bool LockDynamicVertexData(int bufferType, int vertexCount, std::uint32_t vertexFormat, void *allocationContext, void *bufferOwner, int allocationFlags, void *lockData) = 0; // 72
	virtual void UnlockDynamicVertexData(void *lockData) = 0; // 73
	virtual bool LockDynamicIndexData(int indexCount, void *allocationContext, std::uint32_t allocationFlags, void *lockData) = 0; // 74
	virtual void UnlockDynamicIndexData(void *lockData) = 0; // 75
	virtual void PushDynamicBufferState(int state) = 0; // 76
	virtual void PopDynamicBufferState() = 0; // 77
	virtual void * GetDynamicVertexBuffer(int vertexCount, void *drawParams) = 0; // 78
	virtual void * GetMaximumDynamicVertexBuffer(void *drawParams) = 0; // 79
	virtual void EndDynamicVertexBuffer(int vertexCount) = 0; // 80
	virtual void * GetDynamicIndexBuffer(int indexCount, void *indexParams) = 0; // 81
	virtual void * GetMaximumDynamicIndexBuffer(void *indexParams) = 0; // 82
	virtual void EndDynamicIndexBuffer(int indexCount) = 0; // 83
	virtual void DrawIndexedTriangleList(const void *vertexParams, const std::uint32_t *indexParams) = 0; // 84
	virtual void DrawIndexedPrimitive85(const void *vertexParams, const std::uint32_t *indexParams, const void *drawState, int primitiveType) = 0; // 85
	virtual void DrawIndexedTriangleList86(const void *vertexParams, const void *indexBuffer, std::uint32_t firstIndex, const void *indexParams, const void *drawState) = 0; // 86
	virtual void DrawIndexedTriangleList87(const void *combinedBuffer, bool alternateVertexBuffer, const std::uint32_t *indexParams, const void *drawState) = 0; // 87
	virtual void DrawIndexedTriangleList88(const void *combinedBuffer, bool alternateVertexBuffer, const std::uint32_t *indexParams) = 0; // 88
	virtual void DrawTriangleFan(const void *vertexParams, const void *drawState) = 0; // 89
	virtual void DrawQuadList(const void *vertexParams, const void *drawState) = 0; // 90
	virtual void DrawTriangleList(const void *vertexParams, const void *drawState) = 0; // 91
	virtual void DrawTriangleStrip(const void *vertexParams, const void *drawState) = 0; // 92
	virtual void DrawGeneratedTriangleStrip93(const void *vertexParams, const void *drawState) = 0; // 93
	virtual void DrawLineList(const void *vertexParams, const void *drawState) = 0; // 94
	virtual void DrawPointList(const void *vertexParams, const void *drawState) = 0; // 95
	virtual void DrawIndexedTriangleListRange(const void *combinedBuffer, std::uint32_t vertexOffset, const void *indexBuffer, std::uint32_t firstIndex, std::uint32_t indexCount, const void *drawState) = 0; // 96
	virtual void ClearColor3ub(std::uint8_t red, std::uint8_t green, std::uint8_t blue) = 0; // 97
	virtual void ClearColor4ub(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) = 0; // 98
	virtual void ClearColor4f(float red, float green, float blue, float alpha) = 0; // 99
	virtual void NullSub100() = 0; // 100
	virtual void OverrideDepthEnable(bool overrideEnabled, bool depthWriteEnabled, bool depthTestEnabled) = 0; // 101
	virtual void DrawScreenSpaceQuad(IMaterial *material) = 0; // 102
	virtual std::uint64_t CreateOcclusionQueryObject() = 0; // 103
	virtual void DestroyOcclusionQueryObject(std::uint64_t query) = 0; // 104
	virtual void BeginOcclusionQueryDrawing(std::uint64_t query) = 0; // 105
	virtual void EndOcclusionQueryDrawing(std::uint64_t query) = 0; // 106
	virtual int GetNumPixelsRendered(std::uint64_t query) = 0; // 107
	virtual void DrawTexturedRectBatch(const void *rects, std::uint32_t rectCount, void *texture, void *drawState) = 0; // 108
	virtual std::uint64_t GetPerformanceCounterMicroseconds(std::uint64_t *microseconds) const = 0; // 109
	virtual void SubmitRenderTimestamp(std::uint64_t timestamp) = 0; // 110
	virtual void RecordRenderEvent111(std::uint64_t eventData) = 0; // 111
	virtual void SetFlashlightMode(bool enabled) = 0; // 112
	virtual void SetFlashlightTexture(ITexture *texture) = 0; // 113
	virtual void SetFlashlightState(const void *state) = 0; // 114
	virtual void SetGlobalRenderMode115(std::uint8_t mode) = 0; // 115
	virtual void WaitForQueryGroup(void *queryGroup) = 0; // 116
	virtual void ReadPixelsAndStretch(const std::uint32_t *sourceRect, const std::uint32_t *destinationRect, void *destinationBuffer, std::uint32_t destinationFormat, int destinationStride) = 0; // 117
	virtual void NullSub118() = 0; // 118
	virtual void NullSub119() = 0; // 119
	virtual void NullSub120() = 0; // 120
	virtual void GetWindowSize(int *width, int *height) const = 0; // 121
	virtual std::uint32_t GetConditionalRenderStateValue122() const = 0; // 122
	virtual void CopyGlobalRenderState(void *destination) const = 0; // 123
	virtual void SetGlobalScalar124(double value) = 0; // 124
	virtual void StoreIndexedScalar125(double value) = 0; // 125
	virtual void DrawScreenSpaceRectangle(IMaterial *material, int destinationX, int destinationY, int width, int height, float sourceX0, float sourceY0, float sourceX1, float sourceY1, int sourceWidth, int sourceHeight, void *clientRenderable, int xDice, int yDice) = 0; // 126
	virtual void BindRenderResources(const std::uint16_t *resourceState, void *resourceData, std::uint16_t fallbackResource, bool alternate) = 0; // 127
	virtual std::uint32_t AllocateRenderParameterHandle(std::uint32_t nameHash) = 0; // 128
	virtual void PushRenderTargetAndViewport() = 0; // 129
	virtual void PopRenderTargetAndViewport() = 0; // 130
	virtual void BindLightmapTexture(ITexture *lightmapTexture) = 0; // 131
	virtual void CopyTextureSubresource(ITexture *source, ITexture *destination, std::uint32_t subresource) = 0; // 132
	virtual void DispatchTextureCopy133(ITexture *texture, std::uint32_t subresource, const void *sourceRegion, void *destination) = 0; // 133
	virtual void ClearRenderingParameter(std::uint32_t parameter) = 0; // 134
	virtual void SynchronizeRenderTargets() = 0; // 135
	virtual void SetFloatRenderingParameter(int parameter, float value) = 0; // 136
	virtual void SetIntRenderingParameter(int parameter, std::uint32_t value) = 0; // 137
	virtual void SetVectorRenderingParameter(int parameter, const float *value) = 0; // 138
	virtual void SetStencilState(const void *stencilState) = 0; // 139
	virtual void PushCustomClipPlane(const float *plane) = 0; // 140
	virtual void PopCustomClipPlane() = 0; // 141
	virtual void GetMaxToRender(void *mesh, bool maxUntilFlush, int *maxVertices, int *maxIndices) = 0; // 142
	virtual int GetMaxVerticesToRender(IMaterial *material) = 0; // 143
	virtual bool EnableClipping(bool enabled) = 0; // 144
	virtual void NullSub145(std::uint32_t color, const char *name) = 0; // 145
	virtual void NullSub146() = 0; // 146
	virtual void NullSub147(std::uint32_t color, const char *name) = 0; // 147
	virtual void DispatchRenderBackend148(void *argument) = 0; // 148
	virtual void DispatchRenderBackend149() = 0; // 149
	virtual void DispatchRenderBackend150() = 0; // 150
	virtual void DispatchRenderBackend151(std::uint32_t argument, bool enabled) = 0; // 151
	virtual void PushScissorRect(int left, int top, int right, int bottom) = 0; // 152
	virtual void PopScissorRect() = 0; // 153
	virtual IMaterial *GetCurrentMaterial() = 0; // 154
	virtual void GetWorldSpaceCameraPosition(float *position) = 0; // 155
	virtual void *GetCurrentProxy() = 0; // 156
	virtual void DispatchRenderBackendSlot30() = 0; // 157
	virtual void DispatchRenderBackendSlot02() = 0; // 158
	virtual void DispatchRenderBackendSlot03() = 0; // 159
	virtual void DispatchRenderBackendSlot10() = 0; // 160
	virtual void DispatchRenderBackendSlot18() = 0; // 161
	virtual void DispatchRenderBackendSlot12Enabled(void *argument) = 0; // 162
	virtual void DispatchRenderBackendSlot04() = 0; // 163
	virtual void DispatchRenderBackendSlot07() = 0; // 164
	virtual void DispatchRenderBackendSlot08() = 0; // 165
	virtual void DispatchRenderBackendSlot09() = 0; // 166
	virtual void DispatchRenderBackendSlot26() = 0; // 167
	virtual void DispatchRenderBackendSlot29() = 0; // 168
	virtual void SetRenderContextFlag169(bool enabled) = 0; // 169
	virtual void SubmitModelInstanceGroup170(std::uint32_t count, const void *instances) = 0; // 170
	virtual void SubmitModelInstanceGroup171(std::uint32_t count, const void *instances) = 0; // 171
	virtual void SubmitModelInstances(std::uint32_t count, const void *instances, int instanceStride, const void *auxiliaryData, std::uint32_t flags) = 0; // 172
	virtual void UpdateGameTime(float time) = 0; // 173
	virtual void SetWorldSpaceCameraVectors(const float *forward, const float *right, const float *up, int state) = 0; // 174
	virtual void GetWorldSpaceCameraVectors(float *forward, float *right, float *up, int *state) = 0; // 175
	virtual void NullSub176() = 0; // 176
	virtual void NullSub177() = 0; // 177
	virtual double ReturnZero178() = 0; // 178
	virtual void DispatchRenderBackendState179(std::uint32_t first, std::uint32_t second, std::uint32_t third) = 0; // 179
	virtual float GetFloatRenderingParameter(int parameter) = 0; // 180
	virtual std::uint32_t GetIntRenderingParameter(int parameter) = 0; // 181
	virtual void GetVectorRenderingParameter(float *value, int parameter) = 0; // 182
	virtual void SynchronizeRenderResources183() = 0; // 183
	virtual IMaterial *ResolveCurrentMaterial() = 0; // 184
	virtual int GetCurrentLightmapPage() = 0; // 185
	virtual ITexture *GetRenderTargetEx(int renderTargetId) = 0; // 186
	virtual void ApplyMaterialConfig187(int config) = 0; // 187
	virtual void NullSub188() = 0; // 188
	virtual void FogMaxDensity(float maxDensity) = 0; // 189
	virtual void SetCurrentProxy(void *proxy) = 0; // 190
	virtual void ResetRenderData(bool switchBuffer) = 0; // 191
	virtual std::uint16_t ResolvePrimaryLightmapHandle(int pageId, int layerIndex) = 0; // 192
	virtual std::uint16_t ResolveSecondaryLightmapHandle(int pageId, int layerIndex) = 0; // 193
	virtual void CopyRenderContextState(const IMatRenderContext *source) = 0; // 194
	virtual void ApplyCustomClipPlanes() = 0; // 195
	virtual void *LockRenderData(int sizeInBytes) = 0; // 196
	virtual void UnlockRenderData(void *data) = 0; // 197
	virtual void AddRefRenderData() = 0; // 198
	virtual void ReleaseRenderData() = 0; // 199
	virtual bool IsRenderData(const void *data) const = 0; // 200
	virtual void ApplyRenderTargetAndViewport() = 0; // 201
	virtual void FreeRenderDataStorage() = 0; // 202
	virtual void GetStandardTextureDimensions(int* width, int* height, int standardTextureId) = 0; // 203
};
static_assert(std::is_base_of_v<IRefCounted, IMatRenderContext>);
static_assert(sizeof(IMatRenderContext) == sizeof(void*));
