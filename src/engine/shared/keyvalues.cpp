#include "tier1/keyvalues.h"
#include "vstdlib/ikeyvaluessystem.h"
#include "core/filesystem/filesystem.h"
#include "modsystem/modmanager.h"
#include "tier0/vanilla.h"
#include "util/utils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>
#include <winnt.h>

// implementation of the ConVar class
// heavily based on https://github.com/Mauler125/r5sdk/blob/master/r5dev/vpc/keyvalues.cpp

static int (*s_UTF8ToUnicode)(const char* pUTF8, wchar_t* pwchDest, int cubDestSizeInBytes);
static int (*s_UnicodeToUTF8)(const wchar_t* pUnicode, char* pUTF8, int cubDestSizeInBytes);
KeyValuesSystemFn KeyValuesSystem = nullptr;
KeyValuesLoadFromTextBuffer_t KeyValuesLoadFromTextBuffer = nullptr;
KeyValuesEvaluateSymbol_t DefaultKeyValuesSymbol = nullptr;
thread_local KeyValuesEvaluateSymbol_t g_pKeyValuesSymbol = nullptr;
thread_local std::vector<const char*> g_KeyValuesPatchingResources;

// not the best solution but whatever
#define DECLARE_KEYVALUES_LOADER(name, dll, address, textOffset)                                                                                     \
    static KeyValuesLoadFromTextBuffer_t s_##name##LoadText = nullptr;                                                                               \
    DECLARE_HOOK(name, address,                                                                                                                      \
                 [](auto& hook, KeyValues* self, const char* resourceName, void* buffer, IBaseFileSystem* fileSystem, const char* pathID,            \
                    KeyValuesEvaluateSymbol_t evaluateSymbol, int flags) -> char                                                                     \
    { return LoadKeyValuesBuffer(hook, s_##name##LoadText, self, resourceName, buffer, fileSystem, pathID, evaluateSymbol, flags); })                \
    ON_DLL_LOAD(dll, name, [](CModule module)                                                                                                        \
    {                                                                                                                                                \
        s_##name##LoadText = module.Offset(textOffset).RCast<KeyValuesLoadFromTextBuffer_t>();                                                       \
        DISPATCH_HOOK(KeyValuesHooks, name)                                                                                                          \
    })

#define MAKE_3_BYTES_FROM_1_AND_2(x1, x2) ((((uint16_t)x2) << 8) | (uint8_t)(x1))
#define SPLIT_3_BYTES_INTO_1_AND_2(x1, x2, x3)                                                                                             \
	do                                                                                                                                     \
	{                                                                                                                                      \
		x1 = (uint8_t)(x3);                                                                                                                \
		x2 = (uint16_t)((x3) >> 8);                                                                                                        \
	} while (0)

DECLARE_MODULE(KeyValuesHooks)

static void WriteKeyValuesIndent(std::ostream& output, const std::size_t depth)
{
	for (std::size_t index = 0; index < depth; ++index)
		output << '\t';
}

static void WriteKeyValuesString(
	std::ostream& output, const std::string_view value, const bool useEscapeSequences)
{
	for (const char character : value)
	{
		if (character == '"')
			output << "\\\"";
		else if (character == '\\' && useEscapeSequences)
			output << "\\\\";
		else if (character == '\r')
			output << "\\r";
		else if (character == '\n')
			output << "\\n";
		else if (character == '\t')
			output << "\\t";
		else
			output << character;
	}
}

static void WriteKeyValuesNode(std::ostream& output, const KeyValues& keyValues, const std::size_t depth)
{
	WriteKeyValuesIndent(output, depth);
	output << '"';
	WriteKeyValuesString(output, keyValues.GetName() ? keyValues.GetName() : "", keyValues.m_bHasEscapeSequences);
	output << '"';

	if (keyValues.m_pSub || keyValues.GetDataType() == TYPE_NONE)
	{
		output << "\n";
		WriteKeyValuesIndent(output, depth);
		output << "{\n";

		for (const KeyValues* child = keyValues.m_pSub; child; child = child->m_pPeer)
			WriteKeyValuesNode(output, *child, depth + 1);

		WriteKeyValuesIndent(output, depth);
		output << "}\n";
		return;
	}

	output << "\t\t\"";
	WriteKeyValuesString(output, keyValues.GetStringValue(), keyValues.m_bHasEscapeSequences);
	output << "\"\n";
}

KeyValues::KeyValues() {} // default constructor for copying and such

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *pszSetName -
//-----------------------------------------------------------------------------
KeyValues::KeyValues(const char* pszSetName)
{
	Init();
	SetName(pszSetName);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *pszSetName -
//			*pszFirstKey -
//			*pszFirstValue -
//-----------------------------------------------------------------------------
KeyValues::KeyValues(const char* pszSsetName, const char* pszFirstKey, const char* pszFirstValue)
{
	Init();
	SetName(pszSsetName);
	SetString(pszFirstKey, pszFirstValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *pszSetName -
//			*pszFirstKey -
//			*pwszFirstValue -
//-----------------------------------------------------------------------------
KeyValues::KeyValues(const char* pszSetName, const char* pszFirstKey, const wchar_t* pwszFirstValue)
{
	Init();
	SetName(pszSetName);
	SetWString(pszFirstKey, pwszFirstValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *pszSetName -
//			*pszFirstKey -
//			iFirstValue -
//-----------------------------------------------------------------------------
KeyValues::KeyValues(const char* pszSetName, const char* pszFirstKey, int iFirstValue)
{
	Init();
	SetName(pszSetName);
	SetInt(pszFirstKey, iFirstValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *pszSetName -
//			*pszFirstKey -
//			*pszFirstValue -
//			*pszSecondKey -
//			*pszSecondValue -
//-----------------------------------------------------------------------------
KeyValues::KeyValues(
	const char* pszSetName, const char* pszFirstKey, const char* pszFirstValue, const char* pszSecondKey, const char* pszSecondValue)
{
	Init();
	SetName(pszSetName);
	SetString(pszFirstKey, pszFirstValue);
	SetString(pszSecondKey, pszSecondValue);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *pszSetName -
//			*pszFirstKey -
//			iFirstValue -
//			*pszSecondKey -
//			iSecondValue -
//-----------------------------------------------------------------------------
KeyValues::KeyValues(const char* pszSetName, const char* pszFirstKey, int iFirstValue, const char* pszSecondKey, int iSecondValue)
{
	Init();
	SetName(pszSetName);
	SetInt(pszFirstKey, iFirstValue);
	SetInt(pszSecondKey, iSecondValue);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
KeyValues::~KeyValues(void)
{
	RemoveEverything();
}

//-----------------------------------------------------------------------------
// Purpose: Initialize member variables
//-----------------------------------------------------------------------------
void KeyValues::Init(void)
{
	m_iKeyName = 0;
	m_iKeyNameCaseSensitive1 = 0;
	m_iKeyNameCaseSensitive2 = 0;
	m_iDataType = TYPE_NONE;

	m_pSub = nullptr;
	m_pPeer = nullptr;
	m_pChain = nullptr;

	m_sValue = nullptr;
	m_wsValue = nullptr;
	m_pValue = nullptr;

	m_bHasEscapeSequences = 0;
}

void KeyValues::UsesEscapeSequences(bool state)
{
	m_bHasEscapeSequences = state;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out all subkeys, and the current value
//-----------------------------------------------------------------------------
void KeyValues::Clear(void)
{
	delete m_pSub;
	m_pSub = nullptr;
	m_iDataType = TYPE_NONE;
}

//-----------------------------------------------------------------------------
// for backwards compat - we used to need this to force the free to run from the same DLL
// as the alloc
//-----------------------------------------------------------------------------
void KeyValues::DeleteThis(void)
{
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: remove everything
//-----------------------------------------------------------------------------
void KeyValues::RemoveEverything(void)
{
	KeyValues* dat;
	KeyValues* datNext = nullptr;
	for (dat = m_pSub; dat != nullptr; dat = datNext)
	{
		datNext = dat->m_pPeer;
		dat->m_pPeer = nullptr;
		delete dat;
	}

	for (dat = m_pPeer; dat && dat != this; dat = datNext)
	{
		datNext = dat->m_pPeer;
		dat->m_pPeer = nullptr;
		delete dat;
	}

	delete[] m_sValue;
	m_sValue = nullptr;
	delete[] m_wsValue;
	m_wsValue = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Find a keyValue, create it if it is not found.
//			Set bCreate to true to create the key if it doesn't already exist
//			(which ensures a valid pointer will be returned)
// Input  : *pszKeyName -
//			bCreate -
// Output : *KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::FindKey(const char* pszKeyName, bool bCreate)
{
	assert_msg(this, "Member function called on NULL KeyValues");

	if (!pszKeyName || !*pszKeyName)
		return this;

	const char* pSubStr = strchr(pszKeyName, '/');
	const char* pSearchStr = pszKeyName;
	if (pSubStr && !*(pSubStr + 1))
	{
		// if key name is just '/', then use it as a key directly
		pSearchStr = pSubStr;
		pSubStr = nullptr;
	}

	HKeySymbol iSearchStr = KeyValuesSystem()->GetSymbolForString(pSearchStr, bCreate);
	if (iSearchStr == INVALID_KEY_SYMBOL)
	{
		// not found, couldn't possibly be in key value list
		return nullptr;
	}

	KeyValues* pLastKVs = nullptr;
	KeyValues* pCurrentKVs;
	// find the searchStr in the current peer list
	for (pCurrentKVs = m_pSub; pCurrentKVs != nullptr; pCurrentKVs = pCurrentKVs->m_pPeer)
	{
		pLastKVs = pCurrentKVs; // record the last item looked at (for if we need to append to the end of the list)

		// symbol compare
		if (pLastKVs->m_iKeyName == (uint32_t)iSearchStr)
			break;
	}

	if (!pCurrentKVs && m_pChain)
		pCurrentKVs = m_pChain->FindKey(pSearchStr, false);

	// make sure a key was found
	if (!pCurrentKVs)
	{
		if (bCreate)
		{
			// we need to create a new key
			pCurrentKVs = new KeyValues(pSearchStr);
			//			Assert(dat != NULL);

			// insert new key at end of list
			if (pLastKVs)
				pLastKVs->m_pPeer = pCurrentKVs;
			else
				m_pSub = pCurrentKVs;

			pCurrentKVs->m_pPeer = nullptr;

			// a key graduates to be a submsg as soon as it's m_pSub is set
			// this should be the only place m_pSub is set
			m_iDataType = TYPE_NONE;
		}
		else
		{
			return nullptr;
		}
	}

	// if we've still got a subStr we need to keep looking deeper in the tree
	if (pSubStr)
	{
		// recursively chain down through the paths in the string
		return pCurrentKVs->FindKey(pSubStr + 1, bCreate);
	}

	return pCurrentKVs;
}

//-----------------------------------------------------------------------------
// Purpose: Locate last child.  Returns NULL if we have no children
// Output : *KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::FindLastSubKey(void) const
{
	// No children?
	if (m_pSub == nullptr)
		return nullptr;

	// Scan for the last one
	KeyValues* pLastChild = m_pSub;
	while (pLastChild->m_pPeer)
		pLastChild = pLastChild->m_pPeer;
	return pLastChild;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a subkey. Make sure the subkey isn't a child of some other keyvalues
// Input  : *pSubKey -
//-----------------------------------------------------------------------------
void KeyValues::AddSubKey(KeyValues* pSubkey)
{
	// Make sure the subkey isn't a child of some other keyvalues
	assert(pSubkey != nullptr);
	assert(pSubkey->m_pPeer == nullptr);

	// add into subkey list
	if (m_pSub == nullptr)
	{
		m_pSub = pSubkey;
	}
	else
	{
		KeyValues* pTempDat = m_pSub;
		while (pTempDat->GetNextKey() != nullptr)
		{
			pTempDat = pTempDat->GetNextKey();
		}

		pTempDat->SetNextKey(pSubkey);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove a subkey from the list
// Input  : *pSubKey -
//-----------------------------------------------------------------------------
void KeyValues::RemoveSubKey(KeyValues* pSubKey)
{
	if (!pSubKey)
		return;

	// check the list pointer
	if (m_pSub == pSubKey)
	{
		m_pSub = pSubKey->m_pPeer;
	}
	else
	{
		// look through the list
		KeyValues* kv = m_pSub;
		while (kv->m_pPeer)
		{
			if (kv->m_pPeer == pSubKey)
			{
				kv->m_pPeer = pSubKey->m_pPeer;
				break;
			}

			kv = kv->m_pPeer;
		}
	}

	pSubKey->m_pPeer = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: Insert a subkey at index
// Input  : nIndex -
//			*pSubKey -
//-----------------------------------------------------------------------------
void KeyValues::InsertSubKey(int nIndex, KeyValues* pSubKey)
{
	// Sub key must be valid and not part of another chain
	assert(pSubKey && pSubKey->m_pPeer == nullptr);

	if (nIndex == 0)
	{
		pSubKey->m_pPeer = m_pSub;
		m_pSub = pSubKey;
		return;
	}
	else
	{
		int nCurrentIndex = 0;
		for (KeyValues* pIter = GetFirstSubKey(); pIter != nullptr; pIter = pIter->GetNextKey())
		{
			++nCurrentIndex;
			if (nCurrentIndex == nIndex)
			{
				pSubKey->m_pPeer = pIter->m_pPeer;
				pIter->m_pPeer = pSubKey;
				return;
			}
		}
		// Index is out of range if we get here
		assert(0);
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks if key contains a subkey
// Input  : *pSubKey -
// Output : true if contains, false otherwise
//-----------------------------------------------------------------------------
bool KeyValues::ContainsSubKey(KeyValues* pSubKey)
{
	for (KeyValues* pIter = GetFirstSubKey(); pIter != nullptr; pIter = pIter->GetNextKey())
	{
		if (pSubKey == pIter)
		{
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Swaps existing subkey with another
// Input  : *pExistingSubkey -
//			*pNewSubKey -
//-----------------------------------------------------------------------------
void KeyValues::SwapSubKey(KeyValues* pExistingSubkey, KeyValues* pNewSubKey)
{
	assert(pExistingSubkey != nullptr && pNewSubKey != nullptr);

	// Make sure the new sub key isn't a child of some other keyvalues
	assert(pNewSubKey->m_pPeer == nullptr);

	// Check the list pointer
	if (m_pSub == pExistingSubkey)
	{
		pNewSubKey->m_pPeer = pExistingSubkey->m_pPeer;
		pExistingSubkey->m_pPeer = nullptr;
		m_pSub = pNewSubKey;
	}
	else
	{
		// Look through the list
		KeyValues* kv = m_pSub;
		while (kv->m_pPeer)
		{
			if (kv->m_pPeer == pExistingSubkey)
			{
				pNewSubKey->m_pPeer = pExistingSubkey->m_pPeer;
				pExistingSubkey->m_pPeer = nullptr;
				kv->m_pPeer = pNewSubKey;
				break;
			}

			kv = kv->m_pPeer;
		}
		// Existing sub key should always be found, otherwise it's a bug in the calling code.
		assert(kv->m_pPeer != nullptr);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Elides subkey
// Input  : *pSubKey -
//-----------------------------------------------------------------------------
void KeyValues::ElideSubKey(KeyValues* pSubKey)
{
	// This pointer's "next" pointer needs to be fixed up when we elide the key
	KeyValues** ppPointerToFix = &m_pSub;
	for (KeyValues* pKeyIter = m_pSub; pKeyIter != nullptr; ppPointerToFix = &pKeyIter->m_pPeer, pKeyIter = pKeyIter->GetNextKey())
	{
		if (pKeyIter == pSubKey)
		{
			if (pSubKey->m_pSub == nullptr)
			{
				// No children, simply remove the key
				*ppPointerToFix = pSubKey->m_pPeer;
				delete pSubKey;
			}
			else
			{
				*ppPointerToFix = pSubKey->m_pSub;
				// Attach the remainder of this chain to the last child of pSubKey
				KeyValues* pChildIter = pSubKey->m_pSub;
				while (pChildIter->m_pPeer != nullptr)
				{
					pChildIter = pChildIter->m_pPeer;
				}
				// Now points to the last child of pSubKey
				pChildIter->m_pPeer = pSubKey->m_pPeer;
				// Detach the node to be elided
				pSubKey->m_pSub = nullptr;
				pSubKey->m_pPeer = nullptr;
				delete pSubKey;
			}
			return;
		}
	}
	// Key not found; that's caller error.
	assert(0);
}

//-----------------------------------------------------------------------------
// Purpose: Check if a keyName has no value assigned to it.
// Input  : *pszKeyName -
// Output : true on success, false otherwise
//-----------------------------------------------------------------------------
bool KeyValues::IsEmpty(const char* pszKeyName)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (!pKey)
		return true;

	if (pKey->m_iDataType == TYPE_NONE && pKey->m_pSub == nullptr)
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: gets the first true sub key
// Output : *KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetFirstTrueSubKey(void) const
{
	assert_msg(this, "Member function called on NULL KeyValues");
	KeyValues* pRet = m_pSub;
	while (pRet && pRet->m_iDataType != TYPE_NONE)
		pRet = pRet->m_pPeer;

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: gets the next true sub key
// Output : *KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetNextTrueSubKey(void) const
{
	assert_msg(this, "Member function called on NULL KeyValues");
	KeyValues* pRet = m_pPeer;
	while (pRet && pRet->m_iDataType != TYPE_NONE)
		pRet = pRet->m_pPeer;

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: gets the first value
// Output : *KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetFirstValue(void) const
{
	assert_msg(this, "Member function called on NULL KeyValues");
	KeyValues* pRet = m_pSub;
	while (pRet && pRet->m_iDataType == TYPE_NONE)
		pRet = pRet->m_pPeer;

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: gets the next value
// Output : *KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetNextValue(void) const
{
	assert_msg(this, "Member function called on NULL KeyValues");
	KeyValues* pRet = m_pPeer;
	while (pRet && pRet->m_iDataType == TYPE_NONE)
		pRet = pRet->m_pPeer;

	return pRet;
}

//-----------------------------------------------------------------------------
// Purpose: Return the first subkey in the list
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetFirstSubKey() const
{
	assert_msg(this, "Member function called on NULL KeyValues");
	return m_pSub;
}

//-----------------------------------------------------------------------------
// Purpose: Return the next subkey
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetNextKey() const
{
	assert_msg(this, "Member function called on NULL KeyValues");
	return m_pPeer;
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of the current key section
// Output : const char*
//-----------------------------------------------------------------------------
const char* KeyValues::GetName(void) const
{
	return KeyValuesSystem()->GetStringForSymbol(
		MAKE_3_BYTES_FROM_1_AND_2(m_iKeyNameCaseSensitive1, m_iKeyNameCaseSensitive2));
}

//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
// Input  : *pszKeyName -
//			nDefaultValue -
// Output : int
//-----------------------------------------------------------------------------
int KeyValues::GetInt(const char* pszKeyName, int iDefaultValue)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		switch (pKey->m_iDataType)
		{
		case TYPE_STRING:
			return atoi(pKey->m_sValue);
		case TYPE_WSTRING:
			return _wtoi(pKey->m_wsValue);
		case TYPE_FLOAT:
			return static_cast<int>(pKey->m_flValue);
		case TYPE_UINT64:
			// can't convert, since it would lose data
			assert(0);
			return 0;
		case TYPE_INT:
		case TYPE_PTR:
		default:
			return pKey->m_iValue;
		};
	}
	return iDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
// Input  : *pszKeyName -
//			nDefaultValue -
// Output : uint64_t
//-----------------------------------------------------------------------------
uint64_t KeyValues::GetUint64(const char* pszKeyName, uint64_t nDefaultValue)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		switch (pKey->m_iDataType)
		{
		case TYPE_STRING:
		{
			uint64_t uiResult = 0ull;
			sscanf(pKey->m_sValue, "%lld", &uiResult);
			return uiResult;
		}
		case TYPE_WSTRING:
		{
			uint64_t uiResult = 0ull;
			swscanf(pKey->m_wsValue, L"%lld", &uiResult);
			return uiResult;
		}
		case TYPE_FLOAT:
			return static_cast<int>(pKey->m_flValue);
		case TYPE_UINT64:
			return *reinterpret_cast<uint64_t*>(pKey->m_sValue);
		case TYPE_PTR:
			return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pKey->m_pValue));
		case TYPE_INT:
		default:
			return pKey->m_iValue;
		};
	}
	return nDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the pointer value of a keyName. Default value is returned
//			if the keyName can't be found.
// Input  : *pszKeyName -
//			pDefaultValue -
// Output : void*
//-----------------------------------------------------------------------------
void* KeyValues::GetPtr(const char* pszKeyName, void* pDefaultValue)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		switch (pKey->m_iDataType)
		{
		case TYPE_PTR:
			return pKey->m_pValue;

		case TYPE_WSTRING:
		case TYPE_STRING:
		case TYPE_FLOAT:
		case TYPE_INT:
		case TYPE_UINT64:
		default:
			return nullptr;
		};
	}
	return pDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the float value of a keyName. Default value is returned
//			if the keyName can't be found.
// Input  : *pszKeyName -
//			flDefaultValue -
// Output : float
//-----------------------------------------------------------------------------
float KeyValues::GetFloat(const char* pszKeyName, float flDefaultValue)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		switch (pKey->m_iDataType)
		{
		case TYPE_STRING:
			return static_cast<float>(atof(pKey->m_sValue));
		case TYPE_WSTRING:
			return static_cast<float>(_wtof(pKey->m_wsValue)); // no wtof
		case TYPE_FLOAT:
			return pKey->m_flValue;
		case TYPE_INT:
			return static_cast<float>(pKey->m_iValue);
		case TYPE_UINT64:
			return static_cast<float>((*(reinterpret_cast<uint64_t*>(pKey->m_sValue))));
		case TYPE_PTR:
		default:
			return 0.0f;
		};
	}
	return flDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the string pointer of a keyName. Default value is returned
//			if the keyName can't be found.
// // Input  : *pszKeyName -
//			pszDefaultValue -
// Output : const char*
//-----------------------------------------------------------------------------
const char* KeyValues::GetString(const char* pszKeyName, const char* pszDefaultValue)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		// convert the data to string form then return it
		char buf[64];
		switch (pKey->m_iDataType)
		{
		case TYPE_FLOAT:
			snprintf(buf, sizeof(buf), "%f", pKey->m_flValue);
			SetString(pszKeyName, buf);
			break;
		case TYPE_PTR:
			snprintf(buf, sizeof(buf), "%lld", reinterpret_cast<uint64_t>(pKey->m_pValue));
			SetString(pszKeyName, buf);
			break;
		case TYPE_INT:
			snprintf(buf, sizeof(buf), "%d", pKey->m_iValue);
			SetString(pszKeyName, buf);
			break;
		case TYPE_UINT64:
			snprintf(buf, sizeof(buf), "%lld", *(reinterpret_cast<uint64_t*>(pKey->m_sValue)));
			SetString(pszKeyName, buf);
			break;
		case TYPE_COLOR:
			snprintf(buf, sizeof(buf), "%d %d %d %d", pKey->m_Color[0], pKey->m_Color[1], pKey->m_Color[2], pKey->m_Color[3]);
			SetString(pszKeyName, buf);
			break;

		case TYPE_WSTRING:
		{
			// convert the string to char *, set it for future use, and return it
			char wideBuf[512];
			int result = s_UnicodeToUTF8(pKey->m_wsValue, wideBuf, 512);
			if (result)
			{
				// note: this will copy wideBuf
				SetString(pszKeyName, wideBuf);
			}
			else
			{
				return pszDefaultValue;
			}
			break;
		}
		case TYPE_STRING:
			break;
		default:
			return pszDefaultValue;
		};

		return pKey->m_sValue;
	}
	return pszDefaultValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the wide string pointer of a keyName. Default value is returned
//			if the keyName can't be found.
// // Input  : *pszKeyName -
//			pwszDefaultValue -
// Output : const wchar_t*
//-----------------------------------------------------------------------------
const wchar_t* KeyValues::GetWString(const char* pszKeyName, const wchar_t* pwszDefaultValue)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		wchar_t wbuf[64];
		switch (pKey->m_iDataType)
		{
		case TYPE_FLOAT:
			swprintf(wbuf, ARRAYSIZE(wbuf), L"%f", pKey->m_flValue);
			SetWString(pszKeyName, wbuf);
			break;
		case TYPE_PTR:
			swprintf(wbuf, ARRAYSIZE(wbuf), L"%lld", static_cast<int64_t>(reinterpret_cast<size_t>(pKey->m_pValue)));
			SetWString(pszKeyName, wbuf);
			break;
		case TYPE_INT:
			swprintf(wbuf, ARRAYSIZE(wbuf), L"%d", pKey->m_iValue);
			SetWString(pszKeyName, wbuf);
			break;
		case TYPE_UINT64:
		{
			swprintf(wbuf, ARRAYSIZE(wbuf), L"%lld", *(reinterpret_cast<uint64_t*>(pKey->m_sValue)));
			SetWString(pszKeyName, wbuf);
		}
		break;
		case TYPE_COLOR:
			swprintf(wbuf, ARRAYSIZE(wbuf), L"%d %d %d %d", pKey->m_Color[0], pKey->m_Color[1], pKey->m_Color[2], pKey->m_Color[3]);
			SetWString(pszKeyName, wbuf);
			break;

		case TYPE_WSTRING:
			break;
		case TYPE_STRING:
		{
			size_t bufSize = strlen(pKey->m_sValue) + 1;
			wchar_t* pWBuf = new wchar_t[bufSize];
			int result = s_UTF8ToUnicode(pKey->m_sValue, pWBuf, static_cast<int>(bufSize * sizeof(wchar_t)));
			if (result >= 0) // may be a zero length string
			{
				SetWString(pszKeyName, pWBuf);
				delete[] pWBuf;
			}
			else
			{
				delete[] pWBuf;
				return pwszDefaultValue;
			}

			break;
		}
		default:
			return pwszDefaultValue;
		};

		return reinterpret_cast<const wchar_t*>(pKey->m_wsValue);
	}
	return pwszDefaultValue;
}

std::string KeyValues::GetStringValue(void) const
{
	switch (m_iDataType)
	{
	case TYPE_STRING:
		return m_sValue ? m_sValue : "";
	case TYPE_INT:
	case TYPE_COMPILED_INT_BYTE:
		return std::to_string(m_iValue);
	case TYPE_COMPILED_INT_0:
		return "0";
	case TYPE_COMPILED_INT_1:
		return "1";
	case TYPE_FLOAT:
	{
		std::ostringstream value;
		value << std::setprecision(std::numeric_limits<float>::max_digits10) << m_flValue;
		std::string result = value.str();

		// The default stream formatting renders integral floats such as 8.0f as
		// "8", which changes the value's type to int when the compiled .set
		// loader and script bridge consume it. Keep a float representation by
		// preserving the decimal point unless the value is already scientific
		// notation or a non-finite placeholder.
		if (result.find_first_of(".eE") == std::string::npos && result.find("nan") == std::string::npos &&
		    result.find("inf") == std::string::npos)
			result += ".0";

		return result;
	}
	case TYPE_PTR:
		return std::to_string(reinterpret_cast<uintptr_t>(m_pValue));
	case TYPE_WSTRING:
	{
		if (!m_wsValue)
			return {};

		std::string value((wcslen(m_wsValue) * 4) + 1, '\0');
		const int length = s_UnicodeToUTF8(m_wsValue, value.data(), static_cast<int>(value.size()));
		if (length <= 0)
			return {};

		value.resize(strnlen(value.c_str(), value.size()));
		return value;
	}
	case TYPE_COLOR:
		return std::to_string(m_Color[0]) + " " + std::to_string(m_Color[1]) + " " + std::to_string(m_Color[2]) + " " +
			   std::to_string(m_Color[3]);
	case TYPE_UINT64:
		return m_sValue ? std::to_string(*reinterpret_cast<const uint64_t*>(m_sValue)) : "";
	default:
		return {};
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color
// Input  : *pszKeyName -
//			&defaultColor -
// Output : Color
//-----------------------------------------------------------------------------
Color KeyValues::GetColor(const char* pszKeyName, const Color& defaultColor)
{
	Color color = defaultColor;
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
	{
		if (pKey->m_iDataType == TYPE_COLOR)
		{
			color[0] = pKey->m_Color[0];
			color[1] = pKey->m_Color[1];
			color[2] = pKey->m_Color[2];
			color[3] = pKey->m_Color[3];
		}
		else if (pKey->m_iDataType == TYPE_FLOAT)
		{
			color[0] = static_cast<unsigned char>(pKey->m_flValue);
		}
		else if (pKey->m_iDataType == TYPE_INT)
		{
			color[0] = static_cast<unsigned char>(pKey->m_iValue);
		}
		else if (pKey->m_iDataType == TYPE_STRING)
		{
			// parse the colors out of the string
			float a = 0, b = 0, c = 0, d = 0;
			sscanf(pKey->m_sValue, "%f %f %f %f", &a, &b, &c, &d);
			color[0] = static_cast<unsigned char>(a);
			color[1] = static_cast<unsigned char>(b);
			color[2] = static_cast<unsigned char>(c);
			color[3] = static_cast<unsigned char>(d);
		}
	}
	return color;
}

//-----------------------------------------------------------------------------
// Purpose: Get the data type of the value stored in a keyName
// Input  : *pszKeyName -
//-----------------------------------------------------------------------------
KeyValuesTypes_t KeyValues::GetDataType(const char* pszKeyName)
{
	KeyValues* pKey = FindKey(pszKeyName, false);
	if (pKey)
		return static_cast<KeyValuesTypes_t>(pKey->m_iDataType);

	return TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Get the data type of the value stored in this keyName
//-----------------------------------------------------------------------------
KeyValuesTypes_t KeyValues::GetDataType(void) const
{
	return static_cast<KeyValuesTypes_t>(m_iDataType);
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName.
// Input  : *pszKeyName -
//			iValue -
//-----------------------------------------------------------------------------
void KeyValues::SetInt(const char* pszKeyName, int iValue)
{
	KeyValues* pKey = FindKey(pszKeyName, true);
	if (pKey)
	{
		pKey->m_iValue = iValue;
		pKey->m_iDataType = TYPE_INT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName.
//-----------------------------------------------------------------------------
void KeyValues::SetUint64(const char* pszKeyName, uint64_t nValue)
{
	KeyValues* pKey = FindKey(pszKeyName, true);

	if (pKey)
	{
		// delete the old value
		delete[] pKey->m_sValue;
		// make sure we're not storing the WSTRING  - as we're converting over to STRING
		delete[] pKey->m_wsValue;
		pKey->m_wsValue = nullptr;

		pKey->m_sValue = new char[sizeof(uint64_t)];
		*(reinterpret_cast<uint64_t*>(pKey->m_sValue)) = nValue;
		pKey->m_iDataType = TYPE_UINT64;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the float value of a keyName.
// Input  : *pszKeyName -
//			flValue -
//-----------------------------------------------------------------------------
void KeyValues::SetFloat(const char* pszKeyName, float flValue)
{
	KeyValues* pKey = FindKey(pszKeyName, true);
	if (pKey)
	{
		pKey->m_flValue = flValue;
		pKey->m_iDataType = TYPE_FLOAT;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the name value of a keyName.
// Input  : *pszSetName -
//-----------------------------------------------------------------------------
void KeyValues::SetName(const char* pszSetName)
{
	HKeySymbol hCaseSensitiveKeyName = INVALID_KEY_SYMBOL;
	HKeySymbol hCaseInsensitiveKeyName = INVALID_KEY_SYMBOL;
	hCaseSensitiveKeyName =
		KeyValuesSystem()->GetSymbolForStringCaseSensitive(hCaseInsensitiveKeyName, pszSetName, true);

	m_iKeyName = hCaseInsensitiveKeyName;
	SPLIT_3_BYTES_INTO_1_AND_2(m_iKeyNameCaseSensitive1, m_iKeyNameCaseSensitive2, hCaseSensitiveKeyName);
}

//-----------------------------------------------------------------------------
// Purpose: Set the pointer value of a keyName.
// Input  : *pszKeyName -
//			*pValue -
//-----------------------------------------------------------------------------
void KeyValues::SetPtr(const char* pszKeyName, void* pValue)
{
	KeyValues* pKey = FindKey(pszKeyName, true);

	if (pKey)
	{
		pKey->m_pValue = pValue;
		pKey->m_iDataType = TYPE_PTR;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value (internal)
// Input  : *pszValue -
//-----------------------------------------------------------------------------
void KeyValues::SetStringValue(char const* pszValue)
{
	// delete the old value
	delete[] m_sValue;
	// make sure we're not storing the WSTRING  - as we're converting over to STRING
	delete[] m_wsValue;
	m_wsValue = nullptr;

	if (!pszValue)
	{
		// ensure a valid value
		pszValue = "";
	}

	// allocate memory for the new value and copy it in
	size_t len = strlen(pszValue);
	m_sValue = new char[len + 1];
	memcpy(m_sValue, pszValue, len + 1);

	m_iDataType = TYPE_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: Sets this key's peer to the KeyValues passed in
// Input  : *pDat -
//-----------------------------------------------------------------------------
void KeyValues::SetNextKey(KeyValues* pDat)
{
	m_pPeer = pDat;
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName.
// Input  : *pszKeyName -
//			*pszValue -
//-----------------------------------------------------------------------------
void KeyValues::SetString(const char* pszKeyName, const char* pszValue)
{
	if (KeyValues* pKey = FindKey(pszKeyName, true))
	{
		pKey->SetStringValue(pszValue);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName.
// Input  : *pszKeyName -
//			*pwszValue -
//-----------------------------------------------------------------------------
void KeyValues::SetWString(const char* pszKeyName, const wchar_t* pwszValue)
{
	KeyValues* pKey = FindKey(pszKeyName, true);
	if (pKey)
	{
		// delete the old value
		delete[] pKey->m_wsValue;
		// make sure we're not storing the STRING  - as we're converting over to WSTRING
		delete[] pKey->m_sValue;
		pKey->m_sValue = nullptr;

		if (!pwszValue)
		{
			// ensure a valid value
			pwszValue = L"";
		}

		// allocate memory for the new value and copy it in
		size_t len = wcslen(pwszValue);
		pKey->m_wsValue = new wchar_t[len + 1];
		memcpy(pKey->m_wsValue, pwszValue, (len + 1) * sizeof(wchar_t));

		pKey->m_iDataType = TYPE_WSTRING;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets a color
// Input  : *pszKeyName -
//			color -
//-----------------------------------------------------------------------------
void KeyValues::SetColor(const char* pszKeyName, Color color)
{
	KeyValues* pKey = FindKey(pszKeyName, true);

	if (pKey)
	{
		pKey->m_iDataType = TYPE_COLOR;
		pKey->m_Color[0] = color[0];
		pKey->m_Color[1] = color[1];
		pKey->m_Color[2] = color[2];
		pKey->m_Color[3] = color[3];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &src -
//-----------------------------------------------------------------------------
void KeyValues::RecursiveCopyKeyValues(KeyValues& src)
{
	// garymcthack - need to check this code for possible buffer overruns.

	m_iKeyName = src.m_iKeyName;
	m_iKeyNameCaseSensitive1 = src.m_iKeyNameCaseSensitive1;
	m_iKeyNameCaseSensitive2 = src.m_iKeyNameCaseSensitive2;

	if (!src.m_pSub)
	{
		m_iDataType = src.m_iDataType;
		char buf[256];
		switch (src.m_iDataType)
		{
		case TYPE_NONE:
			break;
		case TYPE_STRING:
			if (src.m_sValue)
			{
				size_t len = strlen(src.m_sValue) + 1;
				m_sValue = new char[len];
				strncpy(m_sValue, src.m_sValue, len);
			}
			break;
		case TYPE_INT:
		{
			m_iValue = src.m_iValue;
			snprintf(buf, sizeof(buf), "%d", m_iValue);
			size_t len = strlen(buf) + 1;
			m_sValue = new char[len];
			strncpy(m_sValue, buf, len);
		}
		break;
		case TYPE_FLOAT:
		{
			m_flValue = src.m_flValue;
			snprintf(buf, sizeof(buf), "%f", m_flValue);
			size_t len = strlen(buf) + 1;
			m_sValue = new char[len];
			strncpy(m_sValue, buf, len);
		}
		break;
		case TYPE_PTR:
		{
			m_pValue = src.m_pValue;
		}
		break;
		case TYPE_UINT64:
		{
			m_sValue = new char[sizeof(uint64_t)];
			memcpy(m_sValue, src.m_sValue, sizeof(uint64_t));
		}
		break;
		case TYPE_COLOR:
		{
			m_Color[0] = src.m_Color[0];
			m_Color[1] = src.m_Color[1];
			m_Color[2] = src.m_Color[2];
			m_Color[3] = src.m_Color[3];
		}
		break;

		default:
		{
			// do nothing . .what the heck is this?
			assert(0);
		}
		break;
		}
	}

	// Handle the immediate child
	if (src.m_pSub)
	{
		m_pSub = new KeyValues;

		m_pSub->Init();
		m_pSub->SetName(nullptr);

		m_pSub->RecursiveCopyKeyValues(*src.m_pSub);
	}

	// Handle the immediate peer
	if (src.m_pPeer)
	{
		m_pPeer = new KeyValues;

		m_pPeer->Init();
		m_pPeer->SetName(nullptr);

		m_pPeer->RecursiveCopyKeyValues(*src.m_pPeer);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Make a new copy of all subkeys, add them all to the passed-in keyvalues
// Input  : *pParent -
//-----------------------------------------------------------------------------
void KeyValues::CopySubkeys(KeyValues* pParent) const
{
	// recursively copy subkeys
	// Also maintain ordering....
	KeyValues* pPrev = nullptr;
	for (KeyValues* pSub = m_pSub; pSub != nullptr; pSub = pSub->m_pPeer)
	{
		// take a copy of the subkey
		KeyValues* pKey = pSub->MakeCopy();

		// add into subkey list
		if (pPrev)
		{
			pPrev->m_pPeer = pKey;
		}
		else
		{
			pParent->m_pSub = pKey;
		}
		pKey->m_pPeer = nullptr;
		pPrev = pKey;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes a copy of the whole key-value pair set
// Output : KeyValues*
//-----------------------------------------------------------------------------
KeyValues* KeyValues::MakeCopy(void) const
{
	KeyValues* pNewKeyValue = new KeyValues;

	pNewKeyValue->Init();
	pNewKeyValue->UsesEscapeSequences(m_bHasEscapeSequences != 0);
	pNewKeyValue->SetName(GetName());

	// copy data
	pNewKeyValue->m_iDataType = m_iDataType;
	switch (m_iDataType)
	{
	case TYPE_STRING:
	{
		if (m_sValue)
		{
			size_t len = strlen(m_sValue);
			assert(!pNewKeyValue->m_sValue);
			pNewKeyValue->m_sValue = new char[len + 1];
			memcpy(pNewKeyValue->m_sValue, m_sValue, len + 1);
		}
	}
	break;
	case TYPE_WSTRING:
	{
		if (m_wsValue)
		{
			size_t len = wcslen(m_wsValue);
			pNewKeyValue->m_wsValue = new wchar_t[len + 1];
			memcpy(pNewKeyValue->m_wsValue, m_wsValue, len + 1 * sizeof(wchar_t));
		}
	}
	break;

	case TYPE_INT:
		pNewKeyValue->m_iValue = m_iValue;
		break;

	case TYPE_FLOAT:
		pNewKeyValue->m_flValue = m_flValue;
		break;

	case TYPE_PTR:
		pNewKeyValue->m_pValue = m_pValue;
		break;

	case TYPE_COLOR:
		pNewKeyValue->m_Color[0] = m_Color[0];
		pNewKeyValue->m_Color[1] = m_Color[1];
		pNewKeyValue->m_Color[2] = m_Color[2];
		pNewKeyValue->m_Color[3] = m_Color[3];
		break;

	case TYPE_UINT64:
		pNewKeyValue->m_sValue = new char[sizeof(uint64_t)];
		memcpy(pNewKeyValue->m_sValue, m_sValue, sizeof(uint64_t));
		break;
	};

	// recursively copy subkeys
	CopySubkeys(pNewKeyValue);
	return pNewKeyValue;
}

bool KeyValues::SaveToFile(const char* fileName) const
{
	if (!fileName)
		return false;

	std::ofstream output(fileName, std::ios::binary | std::ios::trunc);
	if (!output)
		return false;

	for (const KeyValues* root = this; root; root = root->m_pPeer)
		WriteKeyValuesNode(output, *root, 0);

	output.close();
	return !output.fail();
}

ON_DLL_LOAD("vstdlib.dll", KeyValues, [](CModule module)
{
	s_UTF8ToUnicode = module.GetExportedFunction("V_UTF8ToUnicode").RCast<int (*)(const char*, wchar_t*, int)>();
	s_UnicodeToUTF8 = module.GetExportedFunction("V_UnicodeToUTF8").RCast<int (*)(const wchar_t*, char*, int)>();
	KeyValuesSystem = module.GetExportedFunction("KeyValuesSystem").RCast<KeyValuesSystemFn>();
})

bool EvaluateKeyValuesSymbol(const char* symbol)
{
    const char* name = *symbol == '$' ? symbol + 1 : symbol;
    const bool vanilla = g_pVanillaCompatibility && g_pVanillaCompatibility->GetVanillaCompatibility();
    if (!_stricmp(name, "NORTHSTAR"))
        return !vanilla;
    if (!_stricmp(name, "VANILLA"))
        return vanilla;

    return (g_pKeyValuesSymbol ? g_pKeyValuesSymbol : DefaultKeyValuesSymbol)(symbol);
}

template <typename Hook>
char LoadKeyValuesBuffer(Hook& hook, KeyValuesLoadFromTextBuffer_t loadText, KeyValues* self, const char* resourceName, void* buffer,
                                IBaseFileSystem* fileSystem, const char* pathID, KeyValuesEvaluateSymbol_t evaluateSymbol, int flags)
{
    if (!DefaultKeyValuesSymbol)
        return hook.Original(self, resourceName, buffer, fileSystem, pathID, evaluateSymbol, flags);

    const KeyValuesEvaluateSymbol_t previousSymbol = g_pKeyValuesSymbol;
    if (evaluateSymbol != EvaluateKeyValuesSymbol)
        g_pKeyValuesSymbol = evaluateSymbol ? evaluateSymbol : DefaultKeyValuesSymbol;
    const ScopeGuard restoreSymbol([previousSymbol] { g_pKeyValuesSymbol = previousSymbol; });
    const char result = hook.Original(self, resourceName, buffer, fileSystem, pathID, EvaluateKeyValuesSymbol, flags);
    if (!result || !g_pModManager || !loadText || !resourceName)
        return result;

    const char* patchResource = !strcmp(resourceName, "playlists") ? "playlists_v2.txt" : resourceName;
    if (std::ranges::any_of(g_KeyValuesPatchingResources, [patchResource](const char* path) { return !_stricmp(path, patchResource); }))
        return result;
    g_KeyValuesPatchingResources.push_back(patchResource);
    const ScopeGuard restoreResource([] { g_KeyValuesPatchingResources.pop_back(); });
    return g_pModManager->ApplyKeyValuesPatches(*self, patchResource, loadText, fileSystem, pathID, EvaluateKeyValuesSymbol, flags);
}

bool KeyValues_LoadFromBuffer(KeyValues* keyValues, const char* resourceName, const char* buffer, IFileSystem* fileSystem,
                              KeyValuesEvaluateSymbol_t evaluateSymbol)
{
    if (!KeyValuesLoadFromTextBuffer || !keyValues || !resourceName || !buffer)
        return false;

    IBaseFileSystem* baseFileSystem = fileSystem ? static_cast<IBaseFileSystem*>(fileSystem) : nullptr;
    return KeyValuesLoadFromTextBuffer(keyValues, resourceName, buffer, baseFileSystem, nullptr, evaluateSymbol, 2) != 0;
}

// clang-format off
DECLARE_HOOK(KeyValues__LoadFromBuffer, engine.dll + 0x426C30,
[](auto& hook, KeyValues* self, const char* pResourceName, void* pBuffer, IBaseFileSystem* pFileSystem,
	const char* pathID, KeyValuesEvaluateSymbol_t evaluateSymbol, int flags) -> char
             // clang-format on
{
    if (!pFileSystem && pResourceName && !strcmp(pResourceName, "playlists"))
        pFileSystem = static_cast<IBaseFileSystem*>(g_pFilesystem);
    return LoadKeyValuesBuffer(hook, KeyValuesLoadFromTextBuffer, self, pResourceName, pBuffer, pFileSystem, pathID, evaluateSymbol, flags);
})

ON_DLL_LOAD_RELIESON("engine.dll", EngineKeyValues, (Filesystem), [](CModule module)
{
    // 0x426AD0 is the public text-buffer wrapper. It constructs the CUtlBuffer
    // consumed by the hooked parser at 0x426C30.
    KeyValuesLoadFromTextBuffer = module.Offset(0x426AD0).RCast<KeyValuesLoadFromTextBuffer_t>();
    DefaultKeyValuesSymbol = module.Offset(0x43C070).RCast<KeyValuesEvaluateSymbol_t>();
    DISPATCH_MODULE(KeyValuesHooks)
})

DECLARE_KEYVALUES_LOADER(ClientKeyValues, "client.dll", client.dll + 0x72C450, 0x72C2F0)
DECLARE_KEYVALUES_LOADER(ServerKeyValues, "server.dll", server.dll + 0x71F130, 0x71EFD0)
DECLARE_KEYVALUES_LOADER(MaterialKeyValues, "materialsystem_dx11.dll", materialsystem_dx11.dll + 0x127BF0, 0x127A90)
DECLARE_KEYVALUES_LOADER(FilesystemKeyValues, "filesystem_stdio.dll", filesystem_stdio.dll + 0x50150, 0x4FFF0)
DECLARE_KEYVALUES_LOADER(LocalizeKeyValues, "localize.dll", localize.dll + 0x22890, 0x22730)
DECLARE_KEYVALUES_LOADER(VGuiKeyValues, "vgui2.dll", vgui2.dll + 0x3E440, 0x3E2E0)
DECLARE_KEYVALUES_LOADER(VGuiSurfaceKeyValues, "vguimatsurface.dll", vguimatsurface.dll + 0x4C8C0, 0x4C760)
DECLARE_KEYVALUES_LOADER(LauncherKeyValues, "launcher.dll", launcher.dll + 0x22F30, 0x22DD0)
