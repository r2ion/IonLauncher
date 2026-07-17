#include "rtech/paktools.h"

const char* Pak_StatusToString(const PakStatus_e status)
{
	switch (status)
	{
	case PakStatus_e::PAK_STATUS_FREED:                  return "PAK_STATUS_FREED";
	case PakStatus_e::PAK_STATUS_LOAD_PENDING:           return "PAK_STATUS_LOAD_PENDING";
	case PakStatus_e::PAK_STATUS_REPAK_RUNNING:          return "PAK_STATUS_REPAK_RUNNING";
	case PakStatus_e::PAK_STATUS_REPAK_DONE:             return "PAK_STATUS_REPAK_DONE";
	case PakStatus_e::PAK_STATUS_LOAD_STARTING:          return "PAK_STATUS_LOAD_STARTING";
	case PakStatus_e::PAK_STATUS_LOAD_PATCH_INIT:        return "PAK_STATUS_LOAD_PATCH_INIT";
	case PakStatus_e::PAK_STATUS_LOAD_ASSETS:            return "PAK_STATUS_LOAD_ASSETS";
	case PakStatus_e::PAK_STATUS_LOADED:                 return "PAK_STATUS_LOADED";
	case PakStatus_e::PAK_STATUS_UNLOAD_PENDING:         return "PAK_STATUS_UNLOAD_PENDING";
	case PakStatus_e::PAK_STATUS_FREE_PENDING:           return "PAK_STATUS_FREE_PENDING";
	case PakStatus_e::PAK_STATUS_CANCELING:              return "PAK_STATUS_CANCELING";
	case PakStatus_e::PAK_STATUS_ERROR:                  return "PAK_STATUS_ERROR";
	case PakStatus_e::PAK_STATUS_INVALID_PAKHANDLE:      return "PAK_STATUS_INVALID_PAKHANDLE";
	case PakStatus_e::PAK_STATUS_BUSY:                   return "PAK_STATUS_BUSY";
	default:                                            return "PAK_STATUS_UNKNOWN";
	}
}

static PakGuid_t Pak_StringToGuidAligned(const char* string)
{
	uint64_t hash = 0;
	uint64_t mixedWord = 0;
	uint64_t multipliedHash = 0;
	uint32_t zeroBytes = 0;
	int byteOffset = 0;
	int terminalMask = 0;

	for (;; byteOffset += 4)
	{
		const uint32_t word = *reinterpret_cast<const uint32_t*>(string);
		zeroBytes = ~word & (word - 0x01010101) & 0x80808080;
		terminalMask = zeroBytes ^ (zeroBytes - 1);
		const uint32_t truncatedWord = terminalMask & word;
		const uint32_t slashes = truncatedWord ^ 0x5C5C5C5C;
		const uint32_t slashBytes = ~slashes & (slashes - 0x01010101) & 0x80808080;
		uint32_t exactSlashBytes = slashBytes & -static_cast<int32_t>(slashBytes);

		if (slashBytes != exactSlashBytes)
		{
			for (uint32_t byteMask = 0xFF000000; byteMask >= 0x100; byteMask >>= 8)
			{
				if ((byteMask & slashes) == 0)
					exactSlashBytes |= byteMask & 0x80808080;
			}
		}

		multipliedHash = 0x0633D5F1 * hash;
		mixedWord = (0xFB8C4D96501ull * static_cast<uint64_t>(
			((truncatedWord - 45 * (exactSlashBytes >> 7)) & 0xDFDFDFDF))) >> 24;

		if (zeroBytes)
			break;

		string += 4;
		hash = ((multipliedHash + mixedWord) >> 61) ^ (multipliedHash + mixedWord);
	}

	unsigned long terminalBit = 0;
	const int finalByte = _BitScanReverse(&terminalBit, terminalMask) ? static_cast<int>(terminalBit) / 8 : -1;
	return mixedWord + multipliedHash - 0x00AE502812AA7333ll * static_cast<uint32_t>(byteOffset + finalByte);
}

static PakGuid_t Pak_StringToGuidUnaligned(const char* string)
{
	uint64_t hash = 0;
	uint64_t mixedWord = 0;
	uint64_t multipliedHash = 0;
	uint32_t zeroBytes = 0;
	int byteOffset = 0;
	int terminalMask = 0;
	uintptr_t cursor = reinterpret_cast<uintptr_t>(string + 3);

	for (;; byteOffset += 4)
	{
		uint32_t word = 0;
		if ((cursor ^ (cursor - 3)) >= 0x1000)
		{
			const uint8_t* const bytes = reinterpret_cast<const uint8_t*>(cursor - 3);
			word = bytes[0];
			if (bytes[0])
			{
				word |= static_cast<uint32_t>(bytes[1]) << 8;
				if (bytes[1])
				{
					word |= static_cast<uint32_t>(bytes[2]) << 16;
					if (bytes[2])
						word |= static_cast<uint32_t>(bytes[3]) << 24;
				}
			}
		}
		else
		{
			word = *reinterpret_cast<const uint32_t*>(cursor - 3);
		}

		zeroBytes = ~word & (word - 0x01010101) & 0x80808080;
		terminalMask = zeroBytes ^ (zeroBytes - 1);
		const uint32_t truncatedWord = terminalMask & word;
		const uint32_t slashes = truncatedWord ^ 0x5C5C5C5C;
		const uint32_t slashBytes = ~slashes & (slashes - 0x01010101) & 0x80808080;
		uint32_t exactSlashBytes = slashBytes & -static_cast<int32_t>(slashBytes);

		if (slashBytes != exactSlashBytes)
		{
			for (uint32_t byteMask = 0xFF000000; byteMask >= 0x100; byteMask >>= 8)
			{
				if ((byteMask & slashes) == 0)
					exactSlashBytes |= byteMask & 0x80808080;
			}
		}

		multipliedHash = 0x0633D5F1 * hash;
		mixedWord = (0xFB8C4D96501ull * static_cast<uint64_t>(
			((truncatedWord - 45 * (exactSlashBytes >> 7)) & 0xDFDFDFDF))) >> 24;

		if (zeroBytes)
			break;

		cursor += 4;
		hash = ((multipliedHash + mixedWord) >> 61) ^ (multipliedHash + mixedWord);
	}

	unsigned long terminalBit = 0;
	const int finalByte = _BitScanReverse(&terminalBit, terminalMask) ? static_cast<int>(terminalBit) / 8 : -1;
	return mixedWord + multipliedHash - 0x00AE502812AA7333ll * static_cast<uint32_t>(byteOffset + finalByte);
}

PakGuid_t Pak_StringToGuid(const char* const string)
{
	if (!string)
		return 0;

	return (reinterpret_cast<uintptr_t>(string) & 3)
		? Pak_StringToGuidUnaligned(string)
		: Pak_StringToGuidAligned(string);
}
