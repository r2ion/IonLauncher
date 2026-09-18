#pragma once

#include "localize/ilocalize.h"
#include "tier1/byteswap.h"
#include "tier1/utlmemory.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlvector.h"

struct LocalizeCharacterFrequencyBuffer_t
{
    CUtlMemory<unsigned char> m_Memory;
    ssize_t m_Get;
    ssize_t m_Put;
    unsigned char m_Error;
    unsigned char m_Flags;
    unsigned char m_Reserved;
    int m_nTab;
    ssize_t m_nMaxPut;
    ssize_t m_nOffset;
    bool (*m_GetOverflowFunc)(LocalizeCharacterFrequencyBuffer_t* buffer, ssize_t size);
    bool (*m_PutOverflowFunc)(LocalizeCharacterFrequencyBuffer_t* buffer, ssize_t size);
    CByteSwap m_Byteswap;
};

class CLocalize : public ILocalize
{
  public:
    CLocalize() = delete;
    CLocalize(const CLocalize&) = delete;
    CLocalize& operator=(const CLocalize&) = delete;

    struct localizedstring_t
    {
        int nameIndex;
        union
        {
            int valueIndex;
            const char* searchName;
        };
        CUtlSymbol fileName;
    };

    struct fastvalue_t
    {
        int valueIndex;
        const wchar_t* searchValue;
    };

    struct LocalizationFile_t
    {
        CUtlSymbol fileName;
        CUtlSymbol pathID;
        bool includeFallbackSearchPaths;
    };

    char m_szLanguage[64];
    bool m_bUseOnlyLongestLanguageString;
    bool m_bSuppressChangeCallbacks;
    bool m_bQueuedChangeCallback;
    CUtlRBTree<localizedstring_t, unsigned int> m_Lookup;
    CUtlVector<char> m_Names;
    CUtlVector<wchar_t> m_Values;
    CUtlSymbol m_CurrentFile;
    CUtlVector<LocalizationFile_t> m_LocalizationFiles;
    bool m_bFastValueLookup;
    CUtlRBTree<fastvalue_t, unsigned int> m_FastValueLookup;
    ILocalizeTextQuery* m_pTextQuery;
    CUtlVector<ILocalizeChangeCallback*> m_ChangeCallbacks;
    LocalizeCharacterFrequencyBuffer_t m_CharacterFrequency;
    bool m_bCharacterFrequencyLoaded;

  protected:
    ~CLocalize() override = default;

    virtual int ConvertANSIToUCS2(const char* ansi, wchar_t* unicode, int unicodeBufferSizeInBytes) = 0;
    virtual int ConvertUCS2ToANSI(const wchar_t* unicode, char* ansi, int ansiBufferSizeInBytes) = 0;
};
