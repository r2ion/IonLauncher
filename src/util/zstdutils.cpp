#include "zstdutils.h"

ZSTDEncoder_s::ZSTDEncoder_s()
{
	cctx = ZSTD_createCCtx();
}

ZSTDEncoder_s::~ZSTDEncoder_s()
{
	ZSTD_freeCCtx(cctx);
}

ZSTDDecoder_s::ZSTDDecoder_s()
{
	__debugbreak();
	dctx = ZSTD_createDCtx();
}

ZSTDDecoder_s::~ZSTDDecoder_s()
{
	ZSTD_freeDCtx(dctx);
}
