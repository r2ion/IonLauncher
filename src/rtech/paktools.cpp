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
