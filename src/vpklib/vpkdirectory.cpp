#include "vpklib/vpkdirectory.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <utility>

static constexpr uint32_t VPK_HEADER_MARKER = 0x55AA1234;
static constexpr uint16_t VPK_MAJOR_VERSION = 2;
static constexpr uint8_t VPK_MINOR_VERSION = 3;
static constexpr uint16_t PACKFILEINDEX_END = 0xffff;

static constexpr size_t VPK_MAX_DIRECTORY_SIZE = 64 * 1024 * 1024;
static constexpr size_t VPK_MAX_INDEXED_PATH_BYTES = 64 * 1024 * 1024;
static constexpr size_t VPK_MAX_TREE_COMPONENT_LENGTH = 4096;

#pragma pack(push, 1)
struct VPKDirHeader_t
{
	uint32_t m_nHeaderMarker;
	uint16_t m_nMajorVersion;
	uint16_t m_nMinorVersion;
	uint32_t m_nDirectorySize;
	uint32_t m_nSignatureSize;
};

struct VPKEntryBlockHeader_t
{
	uint32_t m_nFileCRC;
	uint16_t m_iPreloadSize;
	uint16_t m_iPackFileIndex;
};

struct VPKChunkDescriptor_t
{
	uint32_t m_nLoadFlags;
	uint16_t m_nTextureFlags;
	uint64_t m_nPackFileOffset;
	uint64_t m_nCompressedSize;
	uint64_t m_nUncompressedSize;
};
#pragma pack(pop)

static_assert(sizeof(VPKDirHeader_t) == 16);
static_assert(sizeof(VPKEntryBlockHeader_t) == 8);
static_assert(sizeof(VPKChunkDescriptor_t) == 30);

class CVPKDirectoryCursor
{
  public:
	explicit CVPKDirectoryCursor(const std::vector<uint8_t>& data) : m_Data(data) {}

	bool ReadString(std::string& value)
	{
		const size_t start = m_Position;
		while (m_Position < m_Data.size() && m_Data[m_Position] != '\0')
		{
			if (m_Position - start == VPK_MAX_TREE_COMPONENT_LENGTH)
				return false;

			++m_Position;
		}

		if (m_Position == m_Data.size())
			return false;

		value.assign(reinterpret_cast<const char*>(m_Data.data() + start), m_Position - start);
		++m_Position;
		return true;
	}

	bool SkipEntry()
	{
		if (!Skip(sizeof(VPKEntryBlockHeader_t)))
			return false;

		uint16_t marker = 0;
		do
		{
			if (!Skip(sizeof(VPKChunkDescriptor_t)) || !ReadUInt16(marker))
				return false;
		} while (marker != PACKFILEINDEX_END);

		return true;
	}

	bool HasOnlyZeroesRemaining() const
	{
		return std::all_of(m_Data.begin() + m_Position, m_Data.end(), [](const uint8_t value) { return value == 0; });
	}

  private:
	bool Skip(const size_t size)
	{
		if (size > m_Data.size() - m_Position)
			return false;

		m_Position += size;
		return true;
	}

	bool ReadUInt16(uint16_t& value)
	{
		if (sizeof(value) > m_Data.size() - m_Position)
			return false;

		value = static_cast<uint16_t>(m_Data[m_Position]) | (static_cast<uint16_t>(m_Data[m_Position + 1]) << 8);
		m_Position += sizeof(value);
		return true;
	}

	const std::vector<uint8_t>& m_Data;
	size_t m_Position = 0;
};

static bool VPKDirectory_ExtensionMatches(const std::string_view left, const std::string_view right)
{
	return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b)
	{
		if (a >= 'A' && a <= 'Z')
			a += 'a' - 'A';
		if (b >= 'A' && b <= 'Z')
			b += 'a' - 'A';
		return a == b;
	});
}

static std::string VPKDirectory_FormatEntryPath(
	const std::string& directory, const std::string& fileName, const std::string& extension)
{
	std::string entry;
	if (directory != " " && directory != ".")
	{
		entry = directory;
		std::replace(entry.begin(), entry.end(), '\\', '/');
		if (!entry.empty() && entry.back() != '/')
			entry.push_back('/');
	}

	entry.append(fileName);
	entry.push_back('.');
	entry.append(extension);
	return entry;
}

static bool VPKDirectory_ReadTree(
	const std::vector<uint8_t>& tree, const std::string_view requestedExtension, std::vector<std::string>& entries)
{
	CVPKDirectoryCursor cursor(tree);
	std::string extension;
	std::string directory;
	std::string fileName;
	size_t indexedPathBytes = 0;

	while (true)
	{
		if (!cursor.ReadString(extension))
			return false;
		if (extension.empty())
			break;

		const bool includeExtension = VPKDirectory_ExtensionMatches(extension, requestedExtension);
		while (true)
		{
			if (!cursor.ReadString(directory))
				return false;
			if (directory.empty())
				break;

			while (true)
			{
				if (!cursor.ReadString(fileName))
					return false;
				if (fileName.empty())
					break;
				if (!cursor.SkipEntry())
					return false;

				if (!includeExtension)
					continue;

				std::string entry = VPKDirectory_FormatEntryPath(directory, fileName, extension);
				if (entry.size() > VPK_MAX_INDEXED_PATH_BYTES - indexedPathBytes)
					return false;

				indexedPathBytes += entry.size();
				entries.push_back(std::move(entry));
			}
		}
	}

	return cursor.HasOnlyZeroesRemaining();
}

bool VPKDirectory_GetFileList(
	const std::filesystem::path& directoryFile, const std::string_view extension, std::vector<std::string>& entries)
{
	entries.clear();

	std::ifstream stream(directoryFile, std::ios::binary | std::ios::ate);
	if (!stream)
		return false;

	const std::streamoff fileSize = stream.tellg();
	if (fileSize < static_cast<std::streamoff>(sizeof(VPKDirHeader_t)))
		return false;

	stream.seekg(0, std::ios::beg);
	VPKDirHeader_t header = {};
	if (!stream.read(reinterpret_cast<char*>(&header), sizeof(header)))
		return false;

	if (header.m_nHeaderMarker != VPK_HEADER_MARKER || header.m_nMajorVersion != VPK_MAJOR_VERSION ||
		static_cast<uint8_t>(header.m_nMinorVersion) != VPK_MINOR_VERSION)
		return false;

	if (header.m_nDirectorySize == 0 || header.m_nDirectorySize > VPK_MAX_DIRECTORY_SIZE)
		return false;

	const uint64_t indexedFileSize = sizeof(header) + static_cast<uint64_t>(header.m_nDirectorySize) + header.m_nSignatureSize;
	if (indexedFileSize > static_cast<uint64_t>(fileSize))
		return false;

	std::vector<uint8_t> tree(header.m_nDirectorySize);
	if (!stream.read(reinterpret_cast<char*>(tree.data()), tree.size()) || !VPKDirectory_ReadTree(tree, extension, entries))
	{
		entries.clear();
		return false;
	}

	return true;
}
