#include <zstd.h>
#include <decompress/zstd_decompress_internal.h>
#include <compress/zstd_compress_internal.h>
struct ZSTDEncoder_s
{
	ZSTDEncoder_s();
	~ZSTDEncoder_s();

	ZSTD_CCtx* cctx;
};

struct ZSTDDecoder_s
{
	ZSTDDecoder_s();
	~ZSTDDecoder_s();

	ZSTD_DCtx* dctx;
};
