#include "rtech/rui/imageatlas.h"
#include "rtech/paktools.h"
#include "rtech/rstdlib.h"

#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <mutex>

DECLARE_MODULE(RuiImageAtlasHooks)

bool CImageAtlas::s_LoaderConfigured = false;
CImageAtlas::CreateGpuBufferFn CImageAtlas::s_CreateGpuBuffer = nullptr;
CImageAtlas::DestroyGpuBufferFn CImageAtlas::s_DestroyGpuBuffer = nullptr;
CImageAtlas::FindDescriptorFn CImageAtlas::s_FindDescriptor = nullptr;
CImageAtlas::FindOrReserveDescriptorFn CImageAtlas::s_FindOrReserveDescriptor = nullptr;
CImageAtlas::RemoveDescriptorFn CImageAtlas::s_RemoveDescriptor = nullptr;
CImageAtlas::PakStringToGuidFn CImageAtlas::s_PakStringToGuidAligned = nullptr;
CImageAtlas::PakStringToGuidFn CImageAtlas::s_PakStringToGuidUnaligned = nullptr;
RHashMapU32* CImageAtlas::s_DescriptorMap = nullptr;
RuiImageAtlas CImageAtlas::s_Atlases[RUI_IMAGE_ATLAS_CAPACITY]{};
std::mutex* const CImageAtlas::s_AtlasSlotMutex = new std::mutex;
std::array<bool, RUI_IMAGE_ATLAS_CAPACITY>* const CImageAtlas::s_OwnedAtlasSlots =
	new std::array<bool, RUI_IMAGE_ATLAS_CAPACITY>{};

std::optional<uint8_t> CImageAtlas::ReserveAtlasSlot()
{
	std::lock_guard<std::mutex> lock(*s_AtlasSlotMutex);
	for (int atlasIndex = RUI_DYNAMIC_IMAGE_ATLAS_LAST; atlasIndex >= RUI_DYNAMIC_IMAGE_ATLAS_FIRST; --atlasIndex)
	{
		if ((*s_OwnedAtlasSlots)[atlasIndex])
			continue;

		(*s_OwnedAtlasSlots)[atlasIndex] = true;
		return static_cast<uint8_t>(atlasIndex);
	}

	return std::nullopt;
}

void CImageAtlas::ReleaseAtlasSlot(uint8_t atlasIndex)
{
	if (atlasIndex < RUI_DYNAMIC_IMAGE_ATLAS_FIRST || atlasIndex > RUI_DYNAMIC_IMAGE_ATLAS_LAST)
		return;

	std::lock_guard<std::mutex> lock(*s_AtlasSlotMutex);
	(*s_OwnedAtlasSlots)[atlasIndex] = false;
}

RuiImageAssetDescriptor* CImageAtlas::FindDescriptor(uint32_t nameHash)
{
	if (!s_DescriptorMap || !s_FindDescriptor)
		return nullptr;

	return s_FindDescriptor(s_DescriptorMap, nameHash);
}

RuiImageAssetDescriptor* CImageAtlas::FindOrReserveDescriptor(
	uint32_t nameHash,
	uint8_t* reservedNewEntry)
{
	if (!s_DescriptorMap || !s_FindOrReserveDescriptor)
		return nullptr;

	return static_cast<RuiImageAssetDescriptor*>(
		s_FindOrReserveDescriptor(s_DescriptorMap, nameHash, reservedNewEntry));
}

bool CImageAtlas::HasFreeDescriptor()
{
	return s_DescriptorMap
		&& (s_DescriptorMap->freeListHead != s_DescriptorMap->nextUnusedIndex
			|| s_DescriptorMap->nextUnusedIndex < s_DescriptorMap->bucketPairCount);
}

void CImageAtlas::CommitReservedDescriptor()
{
	s_DescriptorMap->bucketEntryIndices[s_DescriptorMap->pendingBucketIndex]
		= static_cast<int32_t>(s_DescriptorMap->pendingEntryIndex);
	++s_DescriptorMap->liveEntryCount;
}

uint32_t* CImageAtlas::RemoveDescriptor(uint32_t nameHash)
{
	if (!s_DescriptorMap || !s_RemoveDescriptor)
		return nullptr;

	return s_RemoveDescriptor(s_DescriptorMap, nameHash);
}

void CImageAtlas::ReplaceBoundAsset(
	void* boundAsset,
	const void* newHeader,
	const void* previousHeader)
{
	auto* destination = static_cast<RuiImageAtlas*>(boundAsset);
	const auto* source = static_cast<const RuiImageAtlas*>(newHeader);
	const auto* previous = static_cast<const RuiImageAtlas*>(previousHeader);
	if (!destination)
		return;

	// The native replacement callback derives the descriptor atlas index from
	// the engine's 20-slot atlas array. The uimg binding below redirects storage
	// to this extended array, so its callback must use the same storage base.
	const uintptr_t destinationOffset = reinterpret_cast<uintptr_t>(destination)
		- reinterpret_cast<uintptr_t>(s_Atlases);
	if (destinationOffset >= sizeof(s_Atlases)
		|| destinationOffset % sizeof(RuiImageAtlas) != 0)
	{
		if (source)
			*destination = *source;
		return;
	}

	const uint8_t atlasIndex = static_cast<uint8_t>(destinationOffset / sizeof(RuiImageAtlas));
	if (!s_DescriptorMap)
	{
		if (source)
			*destination = *source;
		return;
	}

	// Removal and publication form one descriptor-map transaction. Only remove
	// records still owned by this slot: a later atlas may already have replaced
	// the same hash, and unloading the older asset must not erase that winner.
	bool descriptorTableFull = false;
	AcquireSRWLockExclusive(&s_DescriptorMap->lock);
	if (previous && previous->imageNameRecords)
	{
		for (uint16_t imageIndex = 0; imageIndex < previous->imageCount; ++imageIndex)
		{
			const uint32_t nameHash = previous->imageNameRecords[imageIndex].nameHash;
			const RuiImageAssetDescriptor* descriptor = FindDescriptor(nameHash);
			if (descriptor && descriptor->nameHash == nameHash
				&& descriptor->imageIndex == static_cast<int16_t>(imageIndex)
				&& descriptor->atlasIndex == atlasIndex)
			{
				RemoveDescriptor(nameHash);
			}
		}
	}

	if (source)
	{
		*destination = *source;
		if (source->imageNameRecords)
		{
			for (uint16_t imageIndex = 0; imageIndex < source->imageCount; ++imageIndex)
			{
				const RuiImageAtlasNameRecord& nameRecord = source->imageNameRecords[imageIndex];
				uint8_t reservedNewEntry = 0;
				RuiImageAssetDescriptor* descriptor = FindDescriptor(nameRecord.nameHash);
				if (!descriptor)
				{
					if (!HasFreeDescriptor())
					{
						descriptorTableFull = true;
						continue;
					}

					descriptor = FindOrReserveDescriptor(
						nameRecord.nameHash,
						&reservedNewEntry);
				}
				if (!descriptor)
					continue;

				descriptor->nameHash = nameRecord.nameHash;
				descriptor->imageIndex = static_cast<int16_t>(imageIndex);
				descriptor->atlasIndex = atlasIndex;
				descriptor->flags = static_cast<uint8_t>(nameRecord.flags);
				if (reservedNewEntry)
					CommitReservedDescriptor();
			}
		}
	}
	ReleaseSRWLockExclusive(&s_DescriptorMap->lock);
	if (descriptorTableFull)
	{
		spdlog::error(
			"RUI image descriptor table is full while publishing native atlas {}",
			atlasIndex);
	}
}

void CImageAtlas::ConfigureAssetBinding(PakAssetBinding_s* binding)
{
	if (!binding || std::memcmp(binding->type, "uimg", sizeof(binding->type)) != 0)
		return;

	// Native atlases use the lower range. The upper range is reserved for
	// runtime wrappers around ordinary texture assets.
	binding->assetCapacity = RUI_DYNAMIC_IMAGE_ATLAS_FIRST;
	binding->assetStorage = s_Atlases;
	binding->replaceAssetFunc = ReplaceBoundAsset;
	s_LoaderConfigured = true;
}

DECLARE_HOOK(
	Pak_RegisterAssetBindingType,
	rtech_game.DLL + 0x7BE0,
	[](auto& hook, PakAssetBinding_s* binding, JobPriority_e priority, uint32_t affinity) -> JobTypeID_t
	{
		CImageAtlas::ConfigureAssetBinding(binding);
		return hook.Original(binding, priority, affinity);
	})

bool CImageAtlas::AreRuntimeBindingsReady()
{
	return GetModuleHandleW(L"engine.dll") && s_LoaderConfigured && s_DescriptorMap
		&& s_CreateGpuBuffer && s_DestroyGpuBuffer && s_FindDescriptor
		&& s_FindOrReserveDescriptor && s_RemoveDescriptor;
}

uint64_t CImageAtlas::HashAssetPath(const char* path)
{
	if (!path)
		return 0;

	PakStringToGuidFn hashFunction = (reinterpret_cast<uintptr_t>(path) & 3)
		? s_PakStringToGuidUnaligned
		: s_PakStringToGuidAligned;
	return hashFunction ? hashFunction(path) : Pak_StringToGuid(path);
}

uint32_t CImageAtlas::HashImagePath(const char* path)
{
	const uint64_t guid = HashAssetPath(path);
	return static_cast<uint32_t>(guid) ^ static_cast<uint32_t>(guid >> 32);
}

const RuiImageAssetDescriptor* CImageAtlas::FindAssetDescriptor(uint32_t nameHash)
{
	return FindDescriptor(nameHash);
}

const RuiImageAssetDescriptor* CImageAtlas::GetAssetDescriptor(int32_t descriptorIndex) noexcept
{
	if (!s_DescriptorMap || !s_DescriptorMap->entryStorage || descriptorIndex < 0
		|| static_cast<uint32_t>(descriptorIndex) >= s_DescriptorMap->bucketPairCount)
	{
		return nullptr;
	}

	return &static_cast<const RuiImageAssetDescriptor*>(
		s_DescriptorMap->entryStorage)[descriptorIndex];
}

RuiImageAtlas* CImageAtlas::GetAtlas(uint8_t atlasIndex) noexcept
{
	return atlasIndex < RUI_IMAGE_ATLAS_CAPACITY ? &s_Atlases[atlasIndex] : nullptr;
}

CImageAtlas::~CImageAtlas()
{
	Destroy();
}

bool CImageAtlas::DescriptorStillOwned(size_t imageIndex) const
{
	if (!m_AtlasIndex || imageIndex >= m_Descriptors.size() || imageIndex >= m_NameRecords.size())
		return false;

	const RuiImageAssetDescriptor* descriptor = m_Descriptors[imageIndex];
	const uint32_t nameHash = m_NameRecords[imageIndex].nameHash;
	return descriptor && FindDescriptor(nameHash) == descriptor
		&& descriptor->nameHash == nameHash
		&& descriptor->imageIndex == static_cast<int16_t>(imageIndex)
		&& descriptor->atlasIndex == *m_AtlasIndex;
}

void CImageAtlas::UnregisterDescriptors()
{
	if (!AreRuntimeBindingsReady() || !m_AtlasIndex)
	{
		m_Descriptors.clear();
		return;
	}

	AcquireSRWLockExclusive(&s_DescriptorMap->lock);
	for (size_t imageIndex = 0; imageIndex < m_Descriptors.size(); ++imageIndex)
	{
		if (DescriptorStillOwned(imageIndex))
			RemoveDescriptor(m_NameRecords[imageIndex].nameHash);
	}
	ReleaseSRWLockExclusive(&s_DescriptorMap->lock);
	m_Descriptors.clear();
}

bool CImageAtlas::EnsureRegistered()
{
	if (!AreRuntimeBindingsReady() || !m_AtlasIndex)
		return false;

	// Reserve scratch space before taking the engine's non-RAII SRW lock so no
	// allocation or logging can throw while the lock is held.
	std::vector<uint32_t> failedHashes;
	failedHashes.reserve(m_NameRecords.size());
	bool descriptorTableFull = false;
	bool ownsDescriptor = false;

	AcquireSRWLockExclusive(&s_DescriptorMap->lock);
	for (size_t imageIndex = 0; imageIndex < m_Descriptors.size(); ++imageIndex)
	{
		if (m_Descriptors[imageIndex] && !DescriptorStillOwned(imageIndex))
			m_Descriptors[imageIndex] = nullptr;
		if (m_Descriptors[imageIndex])
			ownsDescriptor = true;
	}

	for (size_t imageIndex = 0; imageIndex < m_NameRecords.size(); ++imageIndex)
	{
		const RuiImageAtlasNameRecord& nameRecord = m_NameRecords[imageIndex];
		if (FindDescriptor(nameRecord.nameHash))
			continue;

		if (!HasFreeDescriptor())
		{
			descriptorTableFull = true;
			break;
		}

		uint8_t reservedNewEntry = 0;
		RuiImageAssetDescriptor* descriptor =
			FindOrReserveDescriptor(nameRecord.nameHash, &reservedNewEntry);
		if (!descriptor || !reservedNewEntry)
		{
			failedHashes.push_back(nameRecord.nameHash);
			continue;
		}

		descriptor->nameHash = nameRecord.nameHash;
		descriptor->imageIndex = static_cast<int16_t>(imageIndex);
		descriptor->atlasIndex = *m_AtlasIndex;
		descriptor->flags = static_cast<uint8_t>(nameRecord.flags);
		CommitReservedDescriptor();
		m_Descriptors[imageIndex] = descriptor;

		if (FindDescriptor(nameRecord.nameHash) != descriptor)
		{
			failedHashes.push_back(nameRecord.nameHash);
			continue;
		}

		ownsDescriptor = true;
	}
	ReleaseSRWLockExclusive(&s_DescriptorMap->lock);

	if (descriptorTableFull)
	{
		spdlog::error(
			"RUI image descriptor table is full while publishing runtime atlas {}",
			*m_AtlasIndex);
	}
	for (const uint32_t nameHash : failedHashes)
		spdlog::error("RUI image descriptor insertion failed for hash 0x{:08X}", nameHash);

	return ownsDescriptor;
}

bool CImageAtlas::Create(
	uint16_t width,
	uint16_t height,
	void* texture,
	std::span<const Image> images)
{
	if (m_AtlasIndex || !AreRuntimeBindingsReady() || !texture || width == 0 || height == 0
		|| images.empty() || images.size() > INT16_MAX)
	{
		return false;
	}

	for (const Image& image : images)
	{
		const uint64_t maxX = static_cast<uint64_t>(image.m_PosX) + image.m_Width;
		const uint64_t maxY = static_cast<uint64_t>(image.m_PosY) + image.m_Height;
		if (image.m_Width == 0 || image.m_Height == 0
			|| maxX > width || maxY > height)
		{
			return false;
		}
	}

	m_AtlasEntries.resize(images.size());
	m_ImageDimensions.resize(images.size());
	m_GpuRecords.resize(images.size());
	m_NameRecords.resize(images.size());
	m_Descriptors.assign(images.size(), nullptr);

	const float inverseWidth = 1.0f / static_cast<float>(width);
	const float inverseHeight = 1.0f / static_cast<float>(height);
	for (size_t imageIndex = 0; imageIndex < images.size(); ++imageIndex)
	{
		const Image& image = images[imageIndex];
		RuiImageAtlasEntry& atlasEntry = m_AtlasEntries[imageIndex];
		atlasEntry.pixelBounds[0] = -0.0f;
		atlasEntry.pixelBounds[1] = -0.0f;
		atlasEntry.pixelBounds[2] = 1.0f;
		atlasEntry.pixelBounds[3] = 1.0f;
		atlasEntry.uvBase[0] = 0.0f;
		atlasEntry.uvBase[1] = 0.0f;
		atlasEntry.uvScale[0] = 1.0f;
		atlasEntry.uvScale[1] = 1.0f;

		m_ImageDimensions[imageIndex] = {
			static_cast<uint16_t>(image.m_Width),
			static_cast<uint16_t>(image.m_Height)};

		RuiImageAtlasGpuRecord& gpuRecord = m_GpuRecords[imageIndex];
		gpuRecord.uvMin[0] = static_cast<float>(image.m_PosX) * inverseWidth;
		gpuRecord.uvMin[1] = static_cast<float>(image.m_PosY) * inverseHeight;
		gpuRecord.uvSize[0] = static_cast<float>(image.m_Width) * inverseWidth;
		gpuRecord.uvSize[1] = static_cast<float>(image.m_Height) * inverseHeight;

		RuiImageAtlasNameRecord& nameRecord = m_NameRecords[imageIndex];
		nameRecord.nameHash = image.m_NameHash;
		nameRecord.flags = image.m_Flags;
		nameRecord.nameOffset = 0;
	}

	const std::optional<uint8_t> atlasIndex = ReserveAtlasSlot();
	if (!atlasIndex)
	{
		m_AtlasEntries.clear();
		m_ImageDimensions.clear();
		m_GpuRecords.clear();
		m_NameRecords.clear();
		m_Descriptors.clear();
		return false;
	}

	RuiImageAtlas& atlas = s_Atlases[*atlasIndex];
	atlas = {};
	atlas.inverseWidth = inverseWidth;
	atlas.inverseHeight = inverseHeight;
	atlas.width = width;
	atlas.height = height;
	atlas.imageCount = static_cast<uint16_t>(images.size());
	atlas.nineSliceImageCount = 0;
	atlas.images = m_AtlasEntries.data();
	atlas.imageDimensions = m_ImageDimensions.data();
	atlas.nineSliceData = nullptr;
	atlas.imageNameRecords = m_NameRecords.data();
	atlas.imageNames = nullptr;
	atlas.texture = texture;
	atlas.gpuRecordBuffer = UINT_MAX;

	const uint32_t gpuRecordBuffer = s_CreateGpuBuffer(&atlas, m_GpuRecords.data());
	if (gpuRecordBuffer == UINT_MAX || atlas.gpuRecordBuffer != gpuRecordBuffer)
	{
		atlas = {};
		m_AtlasEntries.clear();
		m_ImageDimensions.clear();
		m_GpuRecords.clear();
		m_NameRecords.clear();
		m_Descriptors.clear();
		ReleaseAtlasSlot(*atlasIndex);
		return false;
	}

	m_AtlasIndex = *atlasIndex;
	if (!EnsureRegistered())
	{
		Destroy();
		return false;
	}

	return true;
}

void CImageAtlas::Destroy()
{
	if (!m_AtlasIndex)
		return;

	const uint8_t atlasIndex = *m_AtlasIndex;
	UnregisterDescriptors();
	RuiImageAtlas& atlas = s_Atlases[atlasIndex];
	if (atlas.gpuRecordBuffer != UINT_MAX && s_DestroyGpuBuffer
		&& GetModuleHandleW(L"engine.dll"))
	{
		s_DestroyGpuBuffer(&atlas);
	}
	atlas = {};

	m_AtlasIndex.reset();
	m_AtlasEntries.clear();
	m_ImageDimensions.clear();
	m_GpuRecords.clear();
	m_NameRecords.clear();
	ReleaseAtlasSlot(atlasIndex);
}

bool CImageAtlas::IsResident() const noexcept
{
	return m_AtlasIndex.has_value();
}

std::optional<uint8_t> CImageAtlas::GetAtlasIndex() const noexcept
{
	return m_AtlasIndex;
}

void CImageAtlas::OnEngineLoaded(CModule module)
{
	s_DescriptorMap = module.Offset(0x12A4E508).RCast<RHashMapU32*>();
	s_PakStringToGuidAligned = module.Offset(0x4305D0).RCast<PakStringToGuidFn>();
	s_PakStringToGuidUnaligned = module.Offset(0x4305E0).RCast<PakStringToGuidFn>();
	s_CreateGpuBuffer = module.Offset(0xFBF60).RCast<CreateGpuBufferFn>();
	s_DestroyGpuBuffer = module.Offset(0xFC4F0).RCast<DestroyGpuBufferFn>();
	s_FindDescriptor = module.Offset(0xF3C60).RCast<FindDescriptorFn>();
	s_FindOrReserveDescriptor = module.Offset(0xF3BB0).RCast<FindOrReserveDescriptorFn>();
	s_RemoveDescriptor = module.Offset(0xF3E30).RCast<RemoveDescriptorFn>();
}

ON_DLL_LOAD("engine.dll", RuiImageAtlasEngine, [](CModule module)
{
	CImageAtlas::OnEngineLoaded(module);
})

ON_DLL_LOAD("rtech_game.DLL", RuiImageAtlasRtech, [](CModule module)
{
	(void)module;
	DISPATCH_MODULE(RuiImageAtlasHooks);
})
