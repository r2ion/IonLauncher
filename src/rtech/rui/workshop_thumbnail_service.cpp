#include "rtech/rui/workshop_thumbnail_service.h"

#include "config/profile.h"
#include "core/tier0.h"
#include "dedicated/dedicated.h"
#include "rtech/rui/render.h"
#include "rtech/rui/workshop_thumbnail_atlas.h"
#include "tier0/frametask.h"

#include <webp/decode.h>

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

class CWorkshopThumbnailService::CComApartment final
{
public:
	CComApartment() : m_Result(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
	{
	}

	~CComApartment()
	{
		if (SUCCEEDED(m_Result))
			CoUninitialize();
	}

	bool IsReady() const
	{
		return SUCCEEDED(m_Result) || m_Result == RPC_E_CHANGED_MODE;
	}

private:
	HRESULT m_Result;
};

struct CWorkshopThumbnailService::CacheFile
{
	fs::path m_Path;
	uint64_t m_Size = 0;
	fs::file_time_type m_Modified{};
};

uint64_t CWorkshopThumbnailService::Fnv1a(std::string_view value, uint64_t seed)
{
	uint64_t hash = seed;
	for (const unsigned char character : value)
	{
		hash ^= character;
		hash *= 1099511628211ull;
	}
	return hash;
}

std::string CWorkshopThumbnailService::CacheFilename(std::string_view key)
{
	return std::format("{:016x}-{:016x}.webp", Fnv1a(key, 1469598103934665603ull), Fnv1a(key, 1099511628211ull));
}

fs::path CWorkshopThumbnailService::CacheDirectory()
{
	return fs::path(GetNorthstarPrefix()) / "cache" / "modworkshop" / "images";
}

std::string CWorkshopThumbnailService::BuildImageKey(const ModWorkshopThumbnail& thumbnail)
{
	return thumbnail.file + "\x1f" + thumbnail.updatedAt + "\x1foriginal-v1";
}

std::string CWorkshopThumbnailService::BuildLocalImageKey(const fs::path& path)
{
	std::error_code error;
	const fs::path absolutePath = fs::absolute(path, error).lexically_normal();
	if (error)
		return {};
	const uintmax_t size = fs::file_size(absolutePath, error);
	if (error)
		return {};
	const fs::file_time_type modified = fs::last_write_time(absolutePath, error);
	if (error)
		return {};
	return std::format("{}\x1f{}\x1f{}", absolutePath.generic_string(), size, modified.time_since_epoch().count());
}

void CWorkshopThumbnailService::SetPixel(uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
	pixel[0] = red;
	pixel[1] = green;
	pixel[2] = blue;
	pixel[3] = alpha;
}

bool CWorkshopThumbnailService::DecodeWebPFile(const fs::path& imagePath, std::shared_ptr<const std::vector<uint8_t>>& decoded,
                                               std::string& errorMessage)
{
	std::error_code filesystemError;
	const uintmax_t fileSize = fs::file_size(imagePath, filesystemError);
	if (filesystemError || fileSize == 0 || fileSize > MAX_IMAGE_BYTES)
	{
		errorMessage = "Thumbnail cache entry has an invalid size";
		return false;
	}
	std::ifstream input(imagePath, std::ios::binary);
	std::vector<uint8_t> encoded(static_cast<size_t>(fileSize));
	input.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
	if (!input)
	{
		errorMessage = "Failed reading cached thumbnail";
		return false;
	}

	WebPBitstreamFeatures features{};
	const VP8StatusCode featureStatus = WebPGetFeatures(encoded.data(), encoded.size(), &features);
	if (featureStatus != VP8_STATUS_OK || features.width <= 0 || features.height <= 0 || features.width > static_cast<int>(MAX_IMAGE_DIMENSION) ||
	    features.height > static_cast<int>(MAX_IMAGE_DIMENSION) || static_cast<uint64_t>(features.width) * features.height > MAX_IMAGE_PIXELS ||
	    features.has_animation)
	{
		errorMessage = "Thumbnail is malformed, animated, or exceeds dimension limits";
		return false;
	}

	const int targetWidth = static_cast<int>(CWorkshopThumbnailAtlas::IMAGE_WIDTH);
	const int targetHeight = static_cast<int>(CWorkshopThumbnailAtlas::IMAGE_HEIGHT);
	int cropLeft = 0;
	int cropTop = 0;
	int cropWidth = features.width;
	int cropHeight = features.height;
	const uint64_t sourceAspect = static_cast<uint64_t>(features.width) * CWorkshopThumbnailAtlas::IMAGE_HEIGHT;
	const uint64_t targetAspect = static_cast<uint64_t>(features.height) * CWorkshopThumbnailAtlas::IMAGE_WIDTH;
	if (sourceAspect > targetAspect)
	{
		cropWidth = std::max(1, static_cast<int>(static_cast<uint64_t>(features.height) * CWorkshopThumbnailAtlas::IMAGE_WIDTH /
		                                         CWorkshopThumbnailAtlas::IMAGE_HEIGHT));
		cropLeft = ((features.width - cropWidth) / 2) & ~1;
	}
	else if (sourceAspect < targetAspect)
	{
		cropHeight = std::max(1, static_cast<int>(static_cast<uint64_t>(features.width) * CWorkshopThumbnailAtlas::IMAGE_HEIGHT /
		                                          CWorkshopThumbnailAtlas::IMAGE_WIDTH));
		cropTop = ((features.height - cropHeight) / 2) & ~1;
	}
	const uint32_t offsetX = CWorkshopThumbnailAtlas::GUTTER;
	const uint32_t offsetY = CWorkshopThumbnailAtlas::GUTTER;

	auto pixels =
	    std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_WIDTH) * CWorkshopThumbnailAtlas::CELL_HEIGHT * 4);
	for (size_t pixel = 0; pixel < pixels->size(); pixel += 4)
		SetPixel(pixels->data() + pixel, 31, 35, 40);

	WebPDecoderConfig config{};
	if (!WebPInitDecoderConfig(&config) || WebPGetFeatures(encoded.data(), encoded.size(), &config.input) != VP8_STATUS_OK)
	{
		errorMessage = "Failed initializing WebP decoder";
		return false;
	}
	config.options.use_cropping = 1;
	config.options.crop_left = cropLeft;
	config.options.crop_top = cropTop;
	config.options.crop_width = cropWidth;
	config.options.crop_height = cropHeight;
	config.options.use_scaling = 1;
	config.options.scaled_width = targetWidth;
	config.options.scaled_height = targetHeight;
	config.output.colorspace = MODE_RGBA;
	config.output.is_external_memory = 1;
	const size_t outputOffset = (static_cast<size_t>(offsetY) * CWorkshopThumbnailAtlas::CELL_WIDTH + offsetX) * 4;
	config.output.u.RGBA.rgba = pixels->data() + outputOffset;
	config.output.u.RGBA.stride = CWorkshopThumbnailAtlas::CELL_WIDTH * 4;
	config.output.u.RGBA.size = pixels->size() - outputOffset;
	const VP8StatusCode decodeStatus = WebPDecode(encoded.data(), encoded.size(), &config);
	WebPFreeDecBuffer(&config.output);
	if (decodeStatus != VP8_STATUS_OK)
	{
		errorMessage = std::format("WebP decoder failed with status {}", static_cast<int>(decodeStatus));
		return false;
	}

	for (int y = 0; y < targetHeight; ++y)
	{
		for (int x = 0; x < targetWidth; ++x)
		{
			uint8_t* pixel = pixels->data() + ((static_cast<size_t>(offsetY + y) * CWorkshopThumbnailAtlas::CELL_WIDTH) + offsetX + x) * 4;
			const uint32_t alpha = pixel[3];
			pixel[0] = static_cast<uint8_t>((pixel[0] * alpha + 31 * (255 - alpha)) / 255);
			pixel[1] = static_cast<uint8_t>((pixel[1] * alpha + 35 * (255 - alpha)) / 255);
			pixel[2] = static_cast<uint8_t>((pixel[2] * alpha + 40 * (255 - alpha)) / 255);
			pixel[3] = 255;
		}
	}

	const size_t rowBytes = static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_WIDTH) * 4;
	for (uint32_t y = CWorkshopThumbnailAtlas::GUTTER; y < CWorkshopThumbnailAtlas::CELL_HEIGHT - CWorkshopThumbnailAtlas::GUTTER; ++y)
	{
		uint8_t* row = pixels->data() + static_cast<size_t>(y) * rowBytes;
		for (uint32_t gutter = 0; gutter < CWorkshopThumbnailAtlas::GUTTER; ++gutter)
		{
			std::copy_n(row + CWorkshopThumbnailAtlas::GUTTER * 4, 4, row + gutter * 4);
			std::copy_n(row + (CWorkshopThumbnailAtlas::CELL_WIDTH - CWorkshopThumbnailAtlas::GUTTER - 1) * 4, 4,
			            row + (CWorkshopThumbnailAtlas::CELL_WIDTH - 1 - gutter) * 4);
		}
	}
	for (uint32_t gutter = 0; gutter < CWorkshopThumbnailAtlas::GUTTER; ++gutter)
	{
		std::copy_n(pixels->data() + static_cast<size_t>(CWorkshopThumbnailAtlas::GUTTER) * rowBytes, rowBytes,
		            pixels->data() + static_cast<size_t>(gutter) * rowBytes);
		std::copy_n(pixels->data() + static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_HEIGHT - CWorkshopThumbnailAtlas::GUTTER - 1) * rowBytes,
		            rowBytes, pixels->data() + static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_HEIGHT - 1 - gutter) * rowBytes);
	}

	decoded = std::move(pixels);
	return true;
}

bool CWorkshopThumbnailService::DecodeWicFile(const fs::path& imagePath, std::shared_ptr<const std::vector<uint8_t>>& decoded,
                                              std::string& errorMessage)
{
	std::error_code filesystemError;
	const uintmax_t fileSize = fs::file_size(imagePath, filesystemError);
	if (filesystemError || fileSize == 0 || fileSize > MAX_IMAGE_BYTES)
	{
		errorMessage = "Mod icon has an invalid size";
		return false;
	}

	CComApartment apartment;
	if (!apartment.IsReady())
	{
		errorMessage = "Failed initializing COM for mod icon decoding";
		return false;
	}

	using Microsoft::WRL::ComPtr;
	ComPtr<IWICImagingFactory> factory;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
	{
		errorMessage = "Failed creating the Windows image decoder";
		return false;
	}
	ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(factory->CreateDecoderFromFilename(imagePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)))
	{
		errorMessage = "Windows could not decode the mod icon";
		return false;
	}
	ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, &frame)))
	{
		errorMessage = "Mod icon has no readable frame";
		return false;
	}
	UINT sourceWidth = 0;
	UINT sourceHeight = 0;
	if (FAILED(frame->GetSize(&sourceWidth, &sourceHeight)) || sourceWidth == 0 || sourceHeight == 0 || sourceWidth > MAX_IMAGE_DIMENSION ||
	    sourceHeight > MAX_IMAGE_DIMENSION || static_cast<uint64_t>(sourceWidth) * sourceHeight > MAX_IMAGE_PIXELS)
	{
		errorMessage = "Mod icon dimensions are invalid or too large";
		return false;
	}

	UINT cropWidth = sourceWidth;
	UINT cropHeight = sourceHeight;
	const uint64_t sourceAspect = static_cast<uint64_t>(sourceWidth) * CWorkshopThumbnailAtlas::IMAGE_HEIGHT;
	const uint64_t targetAspect = static_cast<uint64_t>(sourceHeight) * CWorkshopThumbnailAtlas::IMAGE_WIDTH;
	if (sourceAspect > targetAspect)
		cropWidth = std::max<UINT>(
		    1, static_cast<UINT>(static_cast<uint64_t>(sourceHeight) * CWorkshopThumbnailAtlas::IMAGE_WIDTH / CWorkshopThumbnailAtlas::IMAGE_HEIGHT));
	else if (sourceAspect < targetAspect)
		cropHeight = std::max<UINT>(
		    1, static_cast<UINT>(static_cast<uint64_t>(sourceWidth) * CWorkshopThumbnailAtlas::IMAGE_HEIGHT / CWorkshopThumbnailAtlas::IMAGE_WIDTH));
	WICRect crop{
	    .X = static_cast<INT>((sourceWidth - cropWidth) / 2),
	    .Y = static_cast<INT>((sourceHeight - cropHeight) / 2),
	    .Width = static_cast<INT>(cropWidth),
	    .Height = static_cast<INT>(cropHeight),
	};

	ComPtr<IWICBitmapClipper> clipper;
	ComPtr<IWICBitmapScaler> scaler;
	ComPtr<IWICFormatConverter> converter;
	if (FAILED(factory->CreateBitmapClipper(&clipper)) || FAILED(clipper->Initialize(frame.Get(), &crop)) ||
	    FAILED(factory->CreateBitmapScaler(&scaler)) ||
	    FAILED(scaler->Initialize(clipper.Get(), CWorkshopThumbnailAtlas::IMAGE_WIDTH, CWorkshopThumbnailAtlas::IMAGE_HEIGHT,
	                              WICBitmapInterpolationModeFant)) ||
	    FAILED(factory->CreateFormatConverter(&converter)) ||
	    FAILED(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
	{
		errorMessage = "Failed converting the mod icon";
		return false;
	}

	auto pixels =
	    std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_WIDTH) * CWorkshopThumbnailAtlas::CELL_HEIGHT * 4);
	for (size_t pixel = 0; pixel < pixels->size(); pixel += 4)
		SetPixel(pixels->data() + pixel, 31, 35, 40);
	const uint32_t offsetX = CWorkshopThumbnailAtlas::GUTTER;
	const uint32_t offsetY = CWorkshopThumbnailAtlas::GUTTER;
	const size_t outputOffset = (static_cast<size_t>(offsetY) * CWorkshopThumbnailAtlas::CELL_WIDTH + offsetX) * 4;
	if (FAILED(converter->CopyPixels(nullptr, CWorkshopThumbnailAtlas::CELL_WIDTH * 4, static_cast<UINT>(pixels->size() - outputOffset),
	                                 pixels->data() + outputOffset)))
	{
		errorMessage = "Failed copying converted mod icon pixels";
		return false;
	}

	for (uint32_t y = 0; y < CWorkshopThumbnailAtlas::IMAGE_HEIGHT; ++y)
	{
		for (uint32_t x = 0; x < CWorkshopThumbnailAtlas::IMAGE_WIDTH; ++x)
		{
			uint8_t* pixel = pixels->data() + ((static_cast<size_t>(offsetY + y) * CWorkshopThumbnailAtlas::CELL_WIDTH) + offsetX + x) * 4;
			const uint32_t alpha = pixel[3];
			pixel[0] = static_cast<uint8_t>((pixel[0] * alpha + 31 * (255 - alpha)) / 255);
			pixel[1] = static_cast<uint8_t>((pixel[1] * alpha + 35 * (255 - alpha)) / 255);
			pixel[2] = static_cast<uint8_t>((pixel[2] * alpha + 40 * (255 - alpha)) / 255);
			pixel[3] = 255;
		}
	}

	const size_t rowBytes = static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_WIDTH) * 4;
	for (uint32_t y = CWorkshopThumbnailAtlas::GUTTER; y < CWorkshopThumbnailAtlas::CELL_HEIGHT - CWorkshopThumbnailAtlas::GUTTER; ++y)
	{
		uint8_t* row = pixels->data() + static_cast<size_t>(y) * rowBytes;
		for (uint32_t gutter = 0; gutter < CWorkshopThumbnailAtlas::GUTTER; ++gutter)
		{
			std::copy_n(row + CWorkshopThumbnailAtlas::GUTTER * 4, 4, row + gutter * 4);
			std::copy_n(row + (CWorkshopThumbnailAtlas::CELL_WIDTH - CWorkshopThumbnailAtlas::GUTTER - 1) * 4, 4,
			            row + (CWorkshopThumbnailAtlas::CELL_WIDTH - 1 - gutter) * 4);
		}
	}
	for (uint32_t gutter = 0; gutter < CWorkshopThumbnailAtlas::GUTTER; ++gutter)
	{
		std::copy_n(pixels->data() + static_cast<size_t>(CWorkshopThumbnailAtlas::GUTTER) * rowBytes, rowBytes,
		            pixels->data() + static_cast<size_t>(gutter) * rowBytes);
		std::copy_n(pixels->data() + static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_HEIGHT - CWorkshopThumbnailAtlas::GUTTER - 1) * rowBytes,
		            rowBytes, pixels->data() + static_cast<size_t>(CWorkshopThumbnailAtlas::CELL_HEIGHT - 1 - gutter) * rowBytes);
	}
	decoded = std::move(pixels);
	return true;
}

bool CWorkshopThumbnailService::DecodeLocalImageFile(const fs::path& imagePath, std::shared_ptr<const std::vector<uint8_t>>& decoded,
                                                     std::string& errorMessage)
{
	std::string extension = imagePath.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return extension == ".webp" ? DecodeWebPFile(imagePath, decoded, errorMessage) : DecodeWicFile(imagePath, decoded, errorMessage);
}

void CWorkshopThumbnailService::EnsureWorkers()
{
	std::scoped_lock lock(m_WorkerMutex);
	if (!m_Workers.empty() || m_Stopped.load(std::memory_order_acquire))
		return;
	m_Workers.reserve(WORKER_COUNT);
	for (size_t index = 0; index < WORKER_COUNT; ++index)
		m_Workers.emplace_back([this] { RunWorker(); });
}

bool CWorkshopThumbnailService::IsCurrent(const ThumbnailJob& job)
{
	if (m_Stopped.load(std::memory_order_acquire) || m_CurrentGeneration.load(std::memory_order_acquire) != job.generation)
	{
		return false;
	}
	std::scoped_lock lock(m_AssignmentMutex);
	const ThumbnailAssignment& assignment = m_Assignments[job.slot];
	return assignment.generation == job.generation && assignment.modId == job.modId && assignment.key == job.key;
}

void CWorkshopThumbnailService::NotifyReady(uint64_t generation, uint64_t modId, size_t slot, bool localIcon)
{
	if (localIcon)
	{
		CWorkshopThumbnailService::LocalIconReadyCallback callback;
		{
			std::scoped_lock lock(m_CallbackMutex);
			callback = m_LocalIconReady;
		}
		if (callback)
		{
			RunInMainThread([callback = std::move(callback), generation, slot] { callback(generation, slot); });
		}
		return;
	}

	CWorkshopThumbnailService::ThumbnailReadyCallback callback;
	{
		std::scoped_lock lock(m_CallbackMutex);
		callback = m_ThumbnailReady;
	}
	if (callback)
	{
		RunInMainThread([callback = std::move(callback), generation, modId, slot] { callback(generation, modId, slot); });
	}
}

std::shared_ptr<const std::vector<uint8_t>> CWorkshopThumbnailService::FindDecoded(const std::string& key)
{
	std::scoped_lock lock(m_DecodedCacheMutex);
	const auto found = m_DecodedCache.find(key);
	if (found == m_DecodedCache.end())
		return {};
	std::erase(m_DecodedCacheOrder, key);
	m_DecodedCacheOrder.push_back(key);
	return found->second;
}

void CWorkshopThumbnailService::StoreDecoded(const std::string& key, std::shared_ptr<const std::vector<uint8_t>> pixels)
{
	std::scoped_lock lock(m_DecodedCacheMutex);
	std::erase(m_DecodedCacheOrder, key);
	m_DecodedCache.insert_or_assign(key, std::move(pixels));
	m_DecodedCacheOrder.push_back(key);
	while (m_DecodedCache.size() > MAX_DECODED_CACHE_ENTRIES && !m_DecodedCacheOrder.empty())
	{
		std::string oldest = std::move(m_DecodedCacheOrder.front());
		m_DecodedCacheOrder.pop_front();
		if (std::ranges::find(m_DecodedCacheOrder, oldest) == m_DecodedCacheOrder.end())
			m_DecodedCache.erase(oldest);
	}
}

void CWorkshopThumbnailService::ScheduleFlush()
{
	if (m_FlushDispatched.exchange(true, std::memory_order_acq_rel))
		return;
	CRuiRenderTaskQueue::Get().Dispatch([this] { FlushUploads(); });
}

void CWorkshopThumbnailService::EnqueueUpload(ThumbnailUpload upload)
{
	{
		std::scoped_lock lock(m_UploadMutex);
		if (m_Uploads.size() >= CWorkshopThumbnailAtlas::SLOT_COUNT * 2)
			m_Uploads.pop_front();
		m_Uploads.push_back(std::move(upload));
	}
	ScheduleFlush();
}

void CWorkshopThumbnailService::FlushUploads()
{
	std::deque<ThumbnailUpload> ready;
	{
		std::scoped_lock lock(m_UploadMutex);
		ready.swap(m_Uploads);
	}

	CWorkshopThumbnailAtlas& atlas = CWorkshopThumbnailAtlas::Get();
	atlas.Initialize();
	for (ThumbnailUpload& upload : ready)
	{
		ThumbnailAssignment assignment;
		{
			std::scoped_lock lock(m_AssignmentMutex);
			assignment = m_Assignments[upload.slot];
		}
		if (assignment.generation != upload.generation || assignment.modId != upload.modId || assignment.key != upload.key)
		{
			continue;
		}

		if (upload.kind == UploadKind::Pixels && upload.pixels)
		{
			const bool updated = atlas.UpdateSlotRgba(upload.slot, *upload.pixels);
			if (!updated)
			{
				spdlog::warn("Failed uploading ModWorkshop thumbnail {} to slot {}", upload.modId, upload.slot);
			}
			else
			{
				{
					std::scoped_lock lock(m_AssignmentMutex);
					m_VisiblePixels[upload.slot] = std::move(upload.pixels);
				}
				NotifyReady(upload.generation, upload.modId, upload.slot, upload.localIcon);
			}
		}
		else
		{
			atlas.FillPlaceholder(upload.slot, upload.kind == UploadKind::Failure);
		}
	}

	m_FlushDispatched.store(false, std::memory_order_release);
	bool hasPendingUploads = false;
	{
		std::scoped_lock lock(m_UploadMutex);
		hasPendingUploads = !m_Uploads.empty();
	}
	if (hasPendingUploads)
		ScheduleFlush();
}

void CWorkshopThumbnailService::MaybePruneDiskCache()
{
	std::unique_lock lock(m_PruneMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return;
	const auto now = std::chrono::steady_clock::now();
	if (m_LastPrune.time_since_epoch().count() != 0 && now - m_LastPrune < std::chrono::minutes(10))
	{
		return;
	}
	m_LastPrune = now;

	std::vector<CacheFile> files;
	uint64_t totalSize = 0;
	std::error_code filesystemError;
	const fs::path directory = CacheDirectory();
	if (!fs::is_directory(directory, filesystemError))
		return;
	for (const fs::directory_entry& entry : fs::directory_iterator(directory, filesystemError))
	{
		if (filesystemError)
			return;
		if (!entry.is_regular_file(filesystemError) || entry.path().extension() != ".webp")
			continue;
		const uint64_t size = entry.file_size(filesystemError);
		if (filesystemError)
			continue;
		files.push_back({entry.path(), size, entry.last_write_time(filesystemError)});
		totalSize += size;
	}
	if (totalSize <= DISK_CACHE_BUDGET)
		return;
	std::ranges::sort(files, {}, &CacheFile::m_Modified);
	for (const CacheFile& file : files)
	{
		fs::remove(file.m_Path, filesystemError);
		if (!filesystemError)
			totalSize -= file.m_Size;
		filesystemError.clear();
		if (totalSize <= DISK_CACHE_TARGET)
			break;
	}
}

bool CWorkshopThumbnailService::EnsureCached(const ThumbnailJob& job, fs::path& cachePath)
{
	std::error_code filesystemError;
	const fs::path directory = CacheDirectory();
	fs::create_directories(directory, filesystemError);
	if (filesystemError)
		return false;
	cachePath = directory / CacheFilename(job.key);
	const uintmax_t existingSize = fs::file_size(cachePath, filesystemError);
	if (!filesystemError && existingSize > 0 && existingSize <= MAX_IMAGE_BYTES)
	{
		fs::last_write_time(cachePath, fs::file_time_type::clock::now(), filesystemError);
		return true;
	}
	filesystemError.clear();

	fs::path temporary = cachePath;
	temporary += std::format(L".part.{}.{}", GetCurrentProcessId(), m_TemporarySequence.fetch_add(1, std::memory_order_relaxed));
	ModWorkshopRequestOptions options;
	options.maxResponseBytes = MAX_IMAGE_BYTES;
	options.timeoutSeconds = 45;
	options.isCancelled = [this, generation = job.generation]
	{ return m_Stopped.load(std::memory_order_acquire) || m_CurrentGeneration.load(std::memory_order_acquire) != generation; };
	uint64_t downloaded = 0;
	ModWorkshopError requestError;
	bool success = m_Client.GetFile(job.url, temporary, downloaded, requestError, options);
	if (!success && !job.fallbackUrl.empty() && requestError.code != ModWorkshopErrorCode::Cancelled)
		success = m_Client.GetFile(job.fallbackUrl, temporary, downloaded, requestError, options);
	if (!success || downloaded == 0)
	{
		fs::remove(temporary, filesystemError);
		return false;
	}
	if (!MoveFileExW(temporary.c_str(), cachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		fs::remove(temporary, filesystemError);
		return false;
	}
	MaybePruneDiskCache();
	return true;
}

void CWorkshopThumbnailService::RunWorker()
{
	for (;;)
	{
		ThumbnailJob job;
		{
			std::unique_lock lock(m_WorkerMutex);
			m_JobsChanged.wait(lock, [this] { return m_Stopped.load(std::memory_order_acquire) || !m_Jobs.empty(); });
			if (m_Stopped.load(std::memory_order_acquire))
				return;
			job = std::move(m_Jobs.front());
			m_Jobs.pop_front();
		}
		if (!IsCurrent(job))
			continue;

		std::shared_ptr<const std::vector<uint8_t>> pixels = FindDecoded(job.key);
		if (!pixels)
		{
			fs::path imagePath;
			std::string decodeError;
			bool available = false;
			const bool fromLocalFile = job.localIcon && !job.localPath.empty();
			if (fromLocalFile)
			{
				imagePath = job.localPath;
				available = true;
			}
			else
			{
				available = EnsureCached(job, imagePath);
			}
			const bool current = IsCurrent(job);
			const bool decoded =
			    available && current &&
			    (fromLocalFile ? DecodeLocalImageFile(imagePath, pixels, decodeError) : DecodeWebPFile(imagePath, pixels, decodeError));
			if (!decoded)
			{
				std::error_code ignored;
				if (!fromLocalFile && !imagePath.empty() && !decodeError.empty())
					fs::remove(imagePath, ignored);
				if (current)
				{
					EnqueueUpload({.generation = job.generation,
					               .slot = job.slot,
					               .modId = job.modId,
					               .key = job.key,
					               .kind = UploadKind::Failure,
					               .localIcon = job.localIcon});
				}
				continue;
			}
			StoreDecoded(job.key, pixels);
		}
		if (IsCurrent(job))
		{
			EnqueueUpload({.generation = job.generation,
			               .slot = job.slot,
			               .modId = job.modId,
			               .key = job.key,
			               .kind = UploadKind::Pixels,
			               .pixels = std::move(pixels),
			               .localIcon = job.localIcon});
		}
	}
}

void CWorkshopThumbnailService::RequestPage(uint64_t generation, std::span<const ModWorkshopCatalogEntry> entries)
{
	if (IsDedicatedServer() || m_Stopped.load(std::memory_order_acquire))
		return;
	m_CurrentGeneration.store(generation, std::memory_order_release);
	std::vector<ThumbnailJob> jobs;
	jobs.reserve(CWorkshopThumbnailAtlas::SLOT_COUNT);
	{
		std::scoped_lock lock(m_AssignmentMutex);
		for (size_t slot = 0; slot < CWorkshopThumbnailAtlas::SLOT_COUNT; ++slot)
		{
			m_Assignments[slot] = {};
			m_VisiblePixels[slot].reset();
			if (slot >= entries.size())
				continue;
			const ModWorkshopCatalogEntry& entry = entries[slot];
			if (!entry.thumbnail)
			{
				m_Assignments[slot] = {generation, entry.id, "missing"};
				continue;
			}
			const std::string key = BuildImageKey(*entry.thumbnail);
			m_Assignments[slot] = {generation, entry.id, key};
			ModWorkshopThumbnail original = *entry.thumbnail;
			original.hasThumbnail = false;
			ModWorkshopThumbnail preview = *entry.thumbnail;
			preview.hasThumbnail = true;
			jobs.push_back({.generation = generation,
			                .slot = slot,
			                .modId = entry.id,
			                .key = key,
			                .url = CModWorkshopClient::BuildThumbnailUrl(original),
			                .fallbackUrl = entry.thumbnail->hasThumbnail ? CModWorkshopClient::BuildThumbnailUrl(preview) : std::string()});
		}
	}

	CRuiRenderTaskQueue::Get().Dispatch([this, generation]
	{
		if (m_CurrentGeneration.load(std::memory_order_acquire) != generation)
			return;
		CWorkshopThumbnailAtlas& atlas = CWorkshopThumbnailAtlas::Get();
		if (!atlas.Initialize())
			return;
		for (size_t slot = 0; slot < CWorkshopThumbnailAtlas::SLOT_COUNT; ++slot)
			atlas.FillPlaceholder(slot);
	});

	EnsureWorkers();
	{
		std::scoped_lock lock(m_WorkerMutex);
		m_Jobs.clear();
		for (ThumbnailJob& job : jobs)
		{
			if (!job.url.empty())
				m_Jobs.push_back(std::move(job));
		}
	}
	m_JobsChanged.notify_all();
}

void CWorkshopThumbnailService::RequestLocalPage(uint64_t generation, std::span<const LocalIconRequest> icons)
{
	if (IsDedicatedServer() || m_Stopped.load(std::memory_order_acquire))
		return;
	m_CurrentGeneration.store(generation, std::memory_order_release);
	std::vector<ThumbnailJob> jobs;
	jobs.reserve(std::min(icons.size(), CWorkshopThumbnailAtlas::SLOT_COUNT));
	{
		std::scoped_lock lock(m_AssignmentMutex);
		for (size_t slot = 0; slot < CWorkshopThumbnailAtlas::SLOT_COUNT; ++slot)
		{
			m_Assignments[slot] = {};
			m_VisiblePixels[slot].reset();
			if (slot >= icons.size())
				continue;
			const LocalIconRequest& icon = icons[slot];
			if (icon.path.empty() && !icon.thumbnail)
			{
				m_Assignments[slot] = {generation, icon.id, "missing", true};
				continue;
			}
			if (!icon.path.empty())
			{
				const std::string key = BuildLocalImageKey(icon.path);
				if (key.empty())
				{
					m_Assignments[slot] = {generation, icon.id, "invalid", true};
					continue;
				}
				m_Assignments[slot] = {generation, icon.id, key, true};
				jobs.push_back({.generation = generation, .slot = slot, .modId = icon.id, .key = key, .localPath = icon.path, .localIcon = true});
				continue;
			}

			const std::string key = BuildImageKey(*icon.thumbnail);
			m_Assignments[slot] = {generation, icon.id, key, true};
			ModWorkshopThumbnail original = *icon.thumbnail;
			original.hasThumbnail = false;
			ModWorkshopThumbnail preview = *icon.thumbnail;
			preview.hasThumbnail = true;
			jobs.push_back({.generation = generation,
			                .slot = slot,
			                .modId = icon.id,
			                .key = key,
			                .url = CModWorkshopClient::BuildThumbnailUrl(original),
			                .fallbackUrl = icon.thumbnail->hasThumbnail ? CModWorkshopClient::BuildThumbnailUrl(preview) : std::string(),
			                .localIcon = true});
		}
	}

	CRuiRenderTaskQueue::Get().Dispatch([this, generation]
	{
		if (m_CurrentGeneration.load(std::memory_order_acquire) != generation)
			return;
		CWorkshopThumbnailAtlas& atlas = CWorkshopThumbnailAtlas::Get();
		if (!atlas.Initialize())
			return;
		for (size_t slot = 0; slot < CWorkshopThumbnailAtlas::SLOT_COUNT; ++slot)
			atlas.FillPlaceholder(slot);
	});

	EnsureWorkers();
	{
		std::scoped_lock lock(m_WorkerMutex);
		m_Jobs.clear();
		for (ThumbnailJob& job : jobs)
			m_Jobs.push_back(std::move(job));
	}
	m_JobsChanged.notify_all();
}

bool CWorkshopThumbnailService::IsReady(uint64_t generation, uint64_t modId, size_t slot) const
{
	if (slot >= CWorkshopThumbnailAtlas::SLOT_COUNT || m_CurrentGeneration.load(std::memory_order_acquire) != generation)
	{
		return false;
	}
	std::scoped_lock lock(m_AssignmentMutex);
	const ThumbnailAssignment& assignment = m_Assignments[slot];
	return assignment.generation == generation && assignment.modId == modId && m_VisiblePixels[slot] != nullptr;
}

void CWorkshopThumbnailService::SetReadyCallback(ThumbnailReadyCallback callback)
{
	std::scoped_lock lock(m_CallbackMutex);
	m_ThumbnailReady = std::move(callback);
}

void CWorkshopThumbnailService::SetLocalIconReadyCallback(LocalIconReadyCallback callback)
{
	std::scoped_lock lock(m_CallbackMutex);
	m_LocalIconReady = std::move(callback);
}

void CWorkshopThumbnailService::RepaintVisible()
{
	std::array<std::shared_ptr<const std::vector<uint8_t>>, CWorkshopThumbnailAtlas::SLOT_COUNT> visible;
	{
		std::scoped_lock lock(m_AssignmentMutex);
		visible = m_VisiblePixels;
	}
	CRuiRenderTaskQueue::Get().Dispatch([visible = std::move(visible)]
	{
		CWorkshopThumbnailAtlas& atlas = CWorkshopThumbnailAtlas::Get();
		if (!atlas.Initialize())
			return;
		for (size_t slot = 0; slot < visible.size(); ++slot)
		{
			if (visible[slot])
				atlas.UpdateSlotRgba(slot, *visible[slot]);
			else
				atlas.FillPlaceholder(slot);
		}
	});
}

void CWorkshopThumbnailService::Shutdown()
{
	if (m_Stopped.exchange(true, std::memory_order_acq_rel))
		return;
	m_JobsChanged.notify_all();
	for (std::thread& worker : m_Workers)
	{
		if (worker.joinable() && worker.get_id() != std::this_thread::get_id())
			worker.join();
	}
	m_Workers.clear();
}
