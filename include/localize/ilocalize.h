#ifndef ILOCALIZE_H
#define ILOCALIZE_H

#include "appframework/IAppSystem.h"
#include "tier0/commonmacros.h"
#include "tier1/strtools.h"

class KeyValues;

typedef unsigned int StringIndex_t;
constexpr StringIndex_t INVALID_LOCALIZE_STRING_INDEX = static_cast<StringIndex_t>(-1);
#define LOCALIZE_INTERFACE_VERSION "Localize_001"

class ILocalizeTextQuery
{
  public:
    virtual int ComputeTextWidth(const wchar_t* text) = 0;
};

class ILocalizeChangeCallback
{
  public:
    virtual void OnLocalizationChanged() = 0;
};

abstract_class ILocalize : public IAppSystem
{
  public:
    virtual bool AddFile(const char* fileName, const char* pathID = nullptr, bool includeFallbackSearchPaths = false) = 0;
    virtual void RemoveAll() = 0;
    virtual const wchar_t* Find(const char* tokenName) = 0;
    virtual const wchar_t* FindSafe(const char* tokenName) = 0;

    virtual ssize_t ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, ssize_t unicodeBufferSizeInBytes) = 0;
    virtual ssize_t ConvertUnicodeToANSI(const wchar_t* unicode, char* ansi, ssize_t ansiBufferSizeInBytes) = 0;
    virtual StringIndex_t FindIndex(const char* tokenName) = 0;

    virtual void ConstructString(wchar_t * output, ssize_t outputSizeInBytes, const wchar_t* format, int numFormatParameters, ...) = 0;
    virtual void ConstructString(wchar_t * output, int outputSizeInBytes, const char* tokenName, KeyValues* variables) = 0;
    virtual void ConstructString(wchar_t * output, int outputSizeInBytes, StringIndex_t stringIndex, KeyValues* variables) = 0;
    virtual bool ConstructString(wchar_t * output, ssize_t outputSizeInBytes, const wchar_t* format, size_t numFormatParameters,
                                 const wchar_t** parameters) = 0;

    virtual const char* GetNameByIndex(StringIndex_t index) = 0;
    virtual const wchar_t* GetValueByIndex(StringIndex_t index) = 0;
    virtual StringIndex_t GetFirstStringIndex() = 0;
    virtual StringIndex_t GetNextStringIndex(StringIndex_t index) = 0;
    virtual void AddString(const char* tokenName, const wchar_t* unicodeString, const char* fileName = nullptr) = 0;
    virtual void SetValueByIndex(StringIndex_t index, const wchar_t* newValue) = 0;
    virtual bool SaveToFile(const char* fileName) = 0;
    virtual int GetLocalizationFileCount() = 0;
    virtual const char* GetLocalizationFileName(int index) = 0;
    virtual const char* GetFileNameByIndex(StringIndex_t index) = 0;
    virtual void ReloadLocalizationFiles() = 0;
    virtual void SetTextQuery(ILocalizeTextQuery * textQuery) = 0;
    virtual void InstallChangeCallback(ILocalizeChangeCallback * callback) = 0;
    virtual void RemoveChangeCallback(ILocalizeChangeCallback * callback) = 0;
    virtual const wchar_t* GetCharacterFrequency(const char* language) = 0;
    virtual void BuildFastValueLookup() = 0;
    virtual void DiscardFastValueLookup() = 0;
};

static_assert(sizeof(StringIndex_t) == 4);
static_assert(sizeof(ILocalize) == sizeof(void*));

inline const char* const g_LanguageNames[] = {
    "english", "german", "french", "italian", "korean", "spanish", "mspanish", "schinese", "tchinese", "russian", "japanese", "portuguese", "polish",
};

inline const char* const g_LanguageCodes[] = {
    "en_US", "de_DE", "fr_FR", "it_IT", "ko_KR", "es_ES", "es_MX", "zh_CN", "zh_TW", "ru_RU", "ja_JP", "pt_BR", "pl_PL",
};

inline bool V_LocaleNameExists(const char* const localeName)
{
    for (size_t i = 0; i < V_ARRAYSIZE(g_LanguageNames); i++)
    {
        if (V_strcmp(localeName, g_LanguageNames[i]) == NULL)
        {
            return true;
        }
    }

    return false;
}

inline bool V_LocaleCodeExists(const char* const localeCode)
{
    for (size_t i = 0; i < V_ARRAYSIZE(g_LanguageCodes); i++)
    {
        if (V_strcmp(localeCode, g_LanguageCodes[i]) == NULL)
        {
            return true;
        }
    }

    return false;
}

#endif // ILOCALIZE_H
