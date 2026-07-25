#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <immintrin.h>
#include <mutex>

class CModule;

struct RuiBaseUv;
struct RuiDrawBatch;
struct RuiGlobalState;
struct RuiImageAssetDescriptor;
struct RuiInstance;
struct RuiTransform;

class CRuiRenderTaskQueue final
{
public:
	using MaterialTaskCallback = uint64_t (*)(uint64_t, uint32_t, uint32_t, uint64_t);
	using QueueMaterialTask = void (*)(MaterialTaskCallback, uint64_t, uint32_t, uint32_t, uint64_t);

	static CRuiRenderTaskQueue& Get()
	{
		static CRuiRenderTaskQueue* s_pInstance = new CRuiRenderTaskQueue;
		return *s_pInstance;
	}

	bool IsCurrentThread() const noexcept;
	void Dispatch(std::function<void()> task);
	void Initialize(CModule module);

	CRuiRenderTaskQueue(const CRuiRenderTaskQueue&) = delete;
	CRuiRenderTaskQueue& operator=(const CRuiRenderTaskQueue&) = delete;

private:
	CRuiRenderTaskQueue() = default;
	~CRuiRenderTaskQueue() = delete;

	static uint64_t RunMaterialTasks(uint64_t, uint32_t, uint32_t, uint64_t);
	void RunPending();
	void Schedule();

	std::mutex m_Mutex;
	std::deque<std::function<void()>> m_Tasks;
	std::atomic<uint32_t> m_ThreadId = 0;
	QueueMaterialTask m_QueueMaterialTask = nullptr;
	bool m_DispatchScheduled = false;
};

bool RuiDrawImageAtlasEntry(
	RuiGlobalState* globalState,
	RuiInstance* rui,
	RuiDrawBatch* batch,
	const RuiBaseUv* baseUv,
	const RuiTransform* transform,
	int orientation,
	const RuiImageAssetDescriptor* descriptor,
	const __m128i* atlasUv,
	const __m128* clipThreshold,
	const __m128* uvBias,
	const __m128* viewportScale);
