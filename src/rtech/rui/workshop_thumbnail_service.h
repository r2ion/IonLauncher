#pragma once

#include "modsystem/platform/modworkshop.h"
#include "rtech/rui/workshop_thumbnail_atlas.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

class CWorkshopThumbnailService final
{
public:
	using ThumbnailReadyCallback = std::function<void(uint64_t, uint64_t, size_t)>;
	using LocalIconReadyCallback = std::function<void(uint64_t, size_t)>;

	struct LocalIconRequest
	{
		uint64_t id = 0;
		std::filesystem::path path;
		std::optional<ModWorkshopThumbnail> thumbnail;
	};

	static CWorkshopThumbnailService& Get()
	{
		static CWorkshopThumbnailService* s_pInstance = new CWorkshopThumbnailService;
		return *s_pInstance;
	}

	void RequestPage(uint64_t generation, std::span<const ModWorkshopCatalogEntry> entries);
	void RequestLocalPage(uint64_t generation, std::span<const LocalIconRequest> icons);
	bool IsReady(uint64_t generation, uint64_t modId, size_t slot) const;
	void SetReadyCallback(ThumbnailReadyCallback callback);
	void SetLocalIconReadyCallback(LocalIconReadyCallback callback);
	void RepaintVisible();
	void Shutdown();

	CWorkshopThumbnailService(const CWorkshopThumbnailService&) = delete;
	CWorkshopThumbnailService& operator=(const CWorkshopThumbnailService&) = delete;

private:
	CWorkshopThumbnailService() = default;
	~CWorkshopThumbnailService() = delete;
	class CComApartment;
	struct CacheFile;

	static constexpr size_t WORKER_COUNT = 3;
	static constexpr size_t MAX_IMAGE_BYTES = 16 * 1024 * 1024;
	static constexpr size_t MAX_DECODED_CACHE_ENTRIES = 18;
	static constexpr uint64_t DISK_CACHE_BUDGET = 256ull * 1024 * 1024;
	static constexpr uint64_t DISK_CACHE_TARGET = 230ull * 1024 * 1024;
	static constexpr uint32_t MAX_IMAGE_DIMENSION = 8192;
	static constexpr uint64_t MAX_IMAGE_PIXELS = 64ull * 1024 * 1024;

	struct ThumbnailAssignment
	{
		uint64_t generation = 0;
		uint64_t modId = 0;
		std::string key;
		bool localIcon = false;
	};

	struct ThumbnailJob
	{
		uint64_t generation = 0;
		size_t slot = 0;
		uint64_t modId = 0;
		std::string key;
		std::string url;
		std::string fallbackUrl;
		std::filesystem::path localPath;
		bool localIcon = false;
	};

	enum class UploadKind
	{
		Pixels,
		Placeholder,
		Failure,
	};

	struct ThumbnailUpload
	{
		uint64_t generation = 0;
		size_t slot = 0;
		uint64_t modId = 0;
		std::string key;
		UploadKind kind = UploadKind::Placeholder;
		std::shared_ptr<const std::vector<uint8_t>> pixels;
		bool localIcon = false;
	};

	static uint64_t Fnv1a(std::string_view value, uint64_t seed);
	static std::string CacheFilename(std::string_view key);
	static std::filesystem::path CacheDirectory();
	static std::string BuildImageKey(const ModWorkshopThumbnail& thumbnail);
	static std::string BuildLocalImageKey(const std::filesystem::path& path);
	static void SetPixel(uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);
	static bool DecodeWebPFile(const std::filesystem::path& imagePath, std::shared_ptr<const std::vector<uint8_t>>& decoded,
	                           std::string& errorMessage);
	static bool DecodeWicFile(const std::filesystem::path& imagePath, std::shared_ptr<const std::vector<uint8_t>>& decoded,
	                          std::string& errorMessage);
	static bool DecodeLocalImageFile(const std::filesystem::path& imagePath, std::shared_ptr<const std::vector<uint8_t>>& decoded,
	                                 std::string& errorMessage);

	CModWorkshopClient m_Client;
	std::atomic<bool> m_Stopped = false;
	std::atomic<uint64_t> m_CurrentGeneration = 0;
	std::mutex m_WorkerMutex;
	std::condition_variable m_JobsChanged;
	std::deque<ThumbnailJob> m_Jobs;
	std::vector<std::thread> m_Workers;
	mutable std::mutex m_AssignmentMutex;
	std::array<ThumbnailAssignment, CWorkshopThumbnailAtlas::SLOT_COUNT> m_Assignments;
	std::array<std::shared_ptr<const std::vector<uint8_t>>, CWorkshopThumbnailAtlas::SLOT_COUNT> m_VisiblePixels;
	std::mutex m_CallbackMutex;
	ThumbnailReadyCallback m_ThumbnailReady;
	LocalIconReadyCallback m_LocalIconReady;
	std::mutex m_UploadMutex;
	std::deque<ThumbnailUpload> m_Uploads;
	std::atomic<bool> m_FlushDispatched = false;
	std::mutex m_DecodedCacheMutex;
	std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> m_DecodedCache;
	std::deque<std::string> m_DecodedCacheOrder;
	std::mutex m_PruneMutex;
	std::chrono::steady_clock::time_point m_LastPrune{};
	std::atomic<uint64_t> m_TemporarySequence = 0;

	void EnsureWorkers();
	bool IsCurrent(const ThumbnailJob& job);
	void NotifyReady(uint64_t generation, uint64_t modId, size_t slot, bool localIcon);
	std::shared_ptr<const std::vector<uint8_t>> FindDecoded(const std::string& key);
	void StoreDecoded(const std::string& key, std::shared_ptr<const std::vector<uint8_t>> pixels);
	void ScheduleFlush();
	void EnqueueUpload(ThumbnailUpload upload);
	void FlushUploads();
	void MaybePruneDiskCache();
	bool EnsureCached(const ThumbnailJob& job, std::filesystem::path& cachePath);
	void RunWorker();
};
