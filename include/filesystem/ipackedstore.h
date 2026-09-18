#ifndef IPACKEDSTORE_H
#define IPACKEDSTORE_H

#include <cstddef>
#include <stdint.h>

enum EPackedLoadFlags
{
    LOAD_NONE,
    LOAD_VISIBLE = 1 << 0, // Visible to FileSystem.
    LOAD_CACHE = 1 << 8,   // Only set for assets not stored in the depot directory.
    LOAD_TEXTURE_UNK0 = 1 << 18,
    LOAD_TEXTURE_UNK1 = 1 << 19,
    LOAD_TEXTURE_UNK2 = 1 << 20,
};

enum EPackedTextureFlags
{
    TEXTURE_NONE,
    TEXTURE_DEFAULT = 1 << 3,
    TEXTURE_ENVIRONMENT_MAP = 1 << 10,
};

enum EPackedStoreTargets
{
    STORE_TARGET_SERVER,
    STORE_TARGET_CLIENT
};

class CPackedStore;

#pragma pack(push, 1)
struct CFilePartDescr
{
    enum : uint8_t
    {
        kLZHAM = 1
    };

    uint16_t m_nArchiveIndex;
    uint8_t m_nCompressionFlags;
    uint8_t m_nLoadFlags;
    uint32_t m_nFlags;
    uint64_t m_nOffset;
    uint64_t m_nCompressedSize;
    uint64_t m_nUncompressedSize;
};

struct VPKFileMetadata_t
{
    uint32_t m_nCRC;
    uint16_t m_nPreloadSize;
};
#pragma pack(pop)

struct VPKFileEntry_t
{
    const char* m_pszDirectory;
    const char* m_pszFileName;
    const char* m_pszExtension;
    const VPKFileMetadata_t* m_pMetadata;
    uint64_t m_nUncompressedSize;
    uint64_t m_nFirstChunkOffset;
    uint32_t m_nPathHash;
    uint32_t m_nFirstChunkFlags;
    uint16_t m_nArchiveIndex;
    uint16_t m_nDirectoryLength;
    uint16_t m_nFileNameLength;
    uint16_t m_nExtensionLength;
    uint8_t m_nFlags;
    uint64_t m_nCompressedSize;
};

struct VPKLookupNode_t
{
    void* m_pAsset;
    uint8_t m_ExtensionData[20];
    uint32_t m_nFileEntryIndex;
};

class FileSystemCache
{
  public:
    int32_t m_nPackedStoreID;
    VPKFileEntry_t* m_pFileEntry;
    VPKLookupNode_t* m_pLookupNode;
};

struct PackedFileOpenData
{
    int32_t m_nArchiveIndex;
    uint64_t m_nDataOffset;
    uint64_t m_nDataSize;
    uint64_t m_nCurrentOffset;
    const uint8_t* m_pPreloadData;
    uint16_t m_nPreloadSize;
    uint32_t m_nFileEntryIndex;
    CPackedStore* m_pPackedStore;
    const VPKFileMetadata_t* m_pMetadata;
    bool m_bTextMode;
};

#endif // IPACKEDSTORE_H
