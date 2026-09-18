/*******************************************************************
 * ██████╗  ██╗    ██╗   ██╗██████╗ ██╗  ██╗    ██╗     ██╗██████╗  *
 * ██╔══██╗███║    ██║   ██║██╔══██╗██║ ██╔╝    ██║     ██║██╔══██╗ *
 * ██████╔╝╚██║    ██║   ██║██████╔╝█████╔╝     ██║     ██║██████╔╝ *
 * ██╔══██╗ ██║    ╚██╗ ██╔╝██╔═══╝ ██╔═██╗     ██║     ██║██╔══██╗ *
 * ██║  ██║ ██║     ╚████╔╝ ██║     ██║  ██╗    ███████╗██║██████╔╝ *
 * ╚═╝  ╚═╝ ╚═╝      ╚═══╝  ╚═╝     ╚═╝  ╚═╝    ╚══════╝╚═╝╚═════╝  *
 *******************************************************************/
#ifndef PACKEDSTORE_H
#define PACKEDSTORE_H

#include "common/qlimits.h"
#include "filesystem/ifilesystem.h"
#include "filesystem/ipackedstore.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include <filesystem>
#include <list>
#include <lzham.h>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

constexpr unsigned int VPK_HEADER_MARKER = 0x55AA1234;
constexpr unsigned int VPK_MAJOR_VERSION = 2;
constexpr unsigned int VPK_MINOR_VERSION = 3;
constexpr unsigned int VPK_DICT_SIZE = 20;
constexpr unsigned int VPK_ENTRY_MAX_LEN = 1024 * 1024;
constexpr int PACKFILEPATCH_MAX = 512;
constexpr int PACKFILEINDEX_SEP = 0x0;
constexpr int PACKFILEINDEX_END = 0xffff;
constexpr const char VPK_IGNORE_FILE[] = ".vpkignore";

//-----------------------------------------------------------------------------
// KeyValues structure for the VPK manifest file. This struct gets populated by
// the VPK's corresponding manifest file, which ultimately determines how each
// asset is getting packed into the VPK.
//-----------------------------------------------------------------------------
struct VPKKeyValues_t
{
    static constexpr uint16_t TEXTURE_FLAGS_DEFAULT = EPackedTextureFlags::TEXTURE_DEFAULT;

    static constexpr uint32_t LOAD_FLAGS_DEFAULT = EPackedLoadFlags::LOAD_VISIBLE | EPackedLoadFlags::LOAD_CACHE;

    CUtlString m_EntryPath;
    uint16_t m_iPreloadSize;
    uint32_t m_nLoadFlags;
    uint16_t m_nTextureFlags;
    bool m_bUseCompression;
    bool m_bDeduplicate;

    VPKKeyValues_t(const CUtlString& svEntryPath = "", uint16_t iPreloadSize = NULL, uint32_t nLoadFlags = LOAD_FLAGS_DEFAULT,
                   uint16_t nTextureFlags = TEXTURE_FLAGS_DEFAULT, bool bUseCompression = true, bool bDeduplicate = true);
};

//-----------------------------------------------------------------------------
// An asset packed into a VPK is carved into 'ENTRY_MAX_LEN' chunks, the chunk
// is then optionally compressed. A chunk is NOT compressed if the compressed
// size equals the uncompressed size.
//-----------------------------------------------------------------------------
struct VPKChunkDescriptor_t
{
    uint32_t m_nLoadFlags;

    // Texture flags (only used if the entry is a vtf).
    uint16_t m_nTextureFlags;

    // Offset in pack file.
    uint64_t m_nPackFileOffset;
    uint64_t m_nCompressedSize;
    uint64_t m_nUncompressedSize;

    VPKChunkDescriptor_t() : m_nLoadFlags(0), m_nTextureFlags(0), m_nPackFileOffset(0), m_nCompressedSize(0), m_nUncompressedSize(0)
    {
    }
    VPKChunkDescriptor_t(CUtlBuffer& directory);
    VPKChunkDescriptor_t(uint32_t nLoadFlags, uint16_t nTextureFlags, uint64_t nPackFileOffset, uint64_t nCompressedSize, uint64_t nUncompressedSize);
};

//-----------------------------------------------------------------------------
// An asset packed into a VPK is represented as an entry block.
//-----------------------------------------------------------------------------
struct VPKEntryBlock_t
{
    // Crc32 for the uncompressed entry.
    uint32_t m_nFileCRC;
    uint16_t m_iPreloadSize;

    // Index of the pack file that contains this entry.
    uint16_t m_iPackFileIndex;

    // Vector of all the chunks of a given entry
    // (chunks have a size limit of 1 MiB, anything
    // over this limit is fragmented into smaller chunks).
    CUtlVector<VPKChunkDescriptor_t> m_Fragments;
    CUtlString m_EntryPath;

    VPKEntryBlock_t(CUtlBuffer& directory, const char* svEntryPath);
    VPKEntryBlock_t(const uint8_t* pData, size_t nLen, int64_t nOffset, uint16_t iPreloadSize, uint16_t iPackFileIndex, uint32_t nLoadFlags,
                    uint16_t nTextureFlags, const char* svEntryPath);

    VPKEntryBlock_t(const VPKEntryBlock_t& other)
        : m_nFileCRC(other.m_nFileCRC), m_iPreloadSize(other.m_iPreloadSize), m_iPackFileIndex(other.m_iPackFileIndex), m_EntryPath(other.m_EntryPath)
    {
        // Has to be explicitly copied!
        m_Fragments = other.m_Fragments;
    }
};

//-----------------------------------------------------------------------------
// The VPK directory file header.
//-----------------------------------------------------------------------------
struct VPKDirHeader_t
{
    int32_t m_nHeaderMarker;
    int32_t m_nVersion;       // Low 16 bits: major; high 16 bits: minor.
    int64_t m_nDirectorySize; // Tree bytes, excluding the header and embedded file data.

    VPKDirHeader_t() : m_nHeaderMarker(0), m_nVersion(0), m_nDirectorySize(0)
    {
    }
};
static_assert(sizeof(VPKDirHeader_t) == 0x10);
static_assert(offsetof(VPKDirHeader_t, m_nDirectorySize) == 0x8);

//-----------------------------------------------------------------------------
// The VPK directory tree structure.
//-----------------------------------------------------------------------------
struct VPKDir_t
{
    VPKDirHeader_t m_Header;
    CUtlString m_DirFilePath;
    CUtlVector<VPKEntryBlock_t> m_EntryBlocks;

    // This set only contains packfile indices used
    // by the directory tree, notated as pak000_xxx.
    std::set<uint16_t> m_PakFileIndices;
    bool m_bInitFailed;

    class CTreeBuilder
    {
      public:
        typedef std::map<std::string, std::list<const VPKEntryBlock_t*>> PathContainer_t;
        typedef std::map<std::string, PathContainer_t> TypeContainer_t;

        void BuildTree(const CUtlVector<VPKEntryBlock_t>& entryBlocks);
        int WriteTree(FileHandle_t hDirectoryFile) const;

      private:
        TypeContainer_t m_FileTree;
    };

    VPKDir_t()
    {
        m_Header.m_nHeaderMarker = NULL;
        m_Header.m_nVersion = NULL;
        m_Header.m_nDirectorySize = NULL;
        m_bInitFailed = false;
    };
    VPKDir_t(const CUtlString& svDirectoryFile);
    VPKDir_t(const CUtlString& svDirectoryFile, bool bSanitizeName);

    void Init(const std::filesystem::path& svPath);
    inline bool Failed() const
    {
        return m_bInitFailed;
    }

    CUtlString StripLocalePrefix(const CUtlString& svDirectoryFile) const;
    CUtlString GetPackFileNameForIndex(uint16_t iPackFileIndex) const;

    void WriteHeader(FileHandle_t hDirectoryFile);
    void BuildDirectoryFile(const CUtlString& svDirectoryFile, const CUtlVector<VPKEntryBlock_t>& entryBlocks);
};

//-----------------------------------------------------------------------------
// Contains the VPK directory name, and the pack file name. Used for building
// the VPK file.
// !TODO[ AMOS ]: Remove this when patching is implemented!
//-----------------------------------------------------------------------------
struct VPKPair_t
{
    CUtlString m_PackName;
    CUtlString m_DirName;

    VPKPair_t(const char* svLocale, const char* svTarget, const char* svLevel, int nPatch);
};

//-----------------------------------------------------------------------------
// VPK building class.
//-----------------------------------------------------------------------------
class CPackedStoreBuilder
{
  public:
    void InitLzEncoder(const lzham_int32 maxHelperThreads = -1, const char* compressionLevel = "default");
    void InitLzDecoder(void);

    bool Deduplicate(const uint8_t* pEntryBuffer, VPKChunkDescriptor_t& descriptor, const size_t chunkIndex);

    void PackStore(const VPKPair_t& vpkPair, const char* workspaceName, const char* buildPath);
    void UnpackStore(const VPKDir_t& vpkDir, const char* workspaceName = "");

  private:
    lzham_compress_params m_Encoder;   // LZham compression parameters.
    lzham_decompress_params m_Decoder; // LZham decompression parameters.
    std::unordered_map<std::string, const VPKChunkDescriptor_t&> m_ChunkHashMap;
};

bool PackedStore_GetDirBaseName(const CUtlString& dirFileName, CUtlString& dirBaseName);
bool PackedStore_GetDirNameParts(const CUtlString& dirFileName, const int nCaptureGroup, CUtlString& dirNameParts);
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// Native VPK loading class. Lifetime and synchronization belong to the filesystem.
//-----------------------------------------------------------------------------
class CPackedStore
{
  public:
    CPackedStore() = delete;
    CPackedStore(const CPackedStore&) = delete;
    CPackedStore& operator=(const CPackedStore&) = delete;
    ~CPackedStore() = delete;

    inline int GetPackFileID() const
    {
        return m_PackFileID;
    }
    inline uint8_t GetStatus() const
    {
        return m_Status;
    }

  private:
    struct DirectoryBucket_t
    {
        DirectoryBucket_t* m_pNext;
        const char* m_pszDirectory; // Borrowed from m_DirectoryData.
    };

    struct ExtensionBucket_t
    {
        ExtensionBucket_t* m_pNext;
        const char* m_pszExtension;
        DirectoryBucket_t* m_Directories[43];
    };

    struct ArchiveHandle_t
    {
        int32_t m_nArchiveIndex;
        int32_t m_nAsyncFileHandle;
        int64_t m_nCurrentPosition;
        CRITICAL_SECTION m_Mutex;
    };
    static_assert(sizeof(ArchiveHandle_t) == 0x38);
    static_assert(sizeof(ExtensionBucket_t) == 0x168);

    int m_PackFileID;
    uint8_t m_Status;
    char m_pszFileBaseName[MAX_OSPATH];
    char m_pszFullPathName[MAX_OSPATH];
    uint64_t m_nWriteOffset;          // 0x210
    int32_t m_nWriteArchiveIndex;     // 0x218
    int32_t m_nOpenWriteArchiveIndex; // 0x21c
    int64_t m_nDirectorySize;         // 0x220
    uint64_t m_nMaximumArchiveSize;   // 0x228
    uint32_t m_Reserved230;           // Only zero initialization observed.
    int32_t m_nInstanceSerial;        // 0x234
    uint32_t m_nFileEntryCount;       // 0x238
    VPKLookupNode_t* m_pLookupNodes;  // 0x240; owns callback state and node allocation.
    VPKFileEntry_t* m_pFileEntries;   // 0x248; owns entries, which borrow directory bytes.
    int32_t m_nCacheID;               // 0x250
    FileHandle_t m_hWriteArchive;     // 0x258
    IBaseFileSystem* m_pFileSystem;   // 0x260
    // Native CThreadFastMutex is a critical section, unlike the SDK spin-mutex declaration.
    CRITICAL_SECTION m_ArchiveMutex;                                            // 0x268
    ExtensionBucket_t* m_Extensions[15];                                        // 0x290
    CUtlVector<uint8_t, CUtlMemory<uint8_t>, int64_t> m_DirectoryData;          // 0x308
    CUtlVector<uint8_t, CUtlBlockMemory<uint8_t, int>, int64_t> m_EmbeddedData; // 0x328
    int32_t m_nHighestArchiveIndex;                                             // 0x340
    ArchiveHandle_t m_ArchiveHandles[PACKFILEPATCH_MAX];                        // 0x348
    CUtlVector<char*> m_DirectoryNames;                                         // 0x7348; owns each string.
    CUtlMap<uint32_t, CUtlVector<char*>*> m_DirectoryFiles;                     // 0x7368; one-based directory index, owns vectors/strings.
};
static_assert(MAX_OSPATH == 260);
static_assert(sizeof(CPackedStore) == 0x7398);
static_assert(alignof(CPackedStore) == 8);

///////////////////////////////////////////////////////////////////////////////
#endif // PACKEDSTORE_H
