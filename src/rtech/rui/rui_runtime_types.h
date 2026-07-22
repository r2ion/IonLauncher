#pragma once

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

struct RuiRenderContext;

struct RuiRenderJobState
{
	float fittedScale;
	uint8_t firstLine;
	uint8_t lineCount;
	uint8_t firstInlineImage;
	uint8_t inlineImageCount;
};
static_assert(sizeof(RuiRenderJobState) == 0x8);

struct RuiTransform
{
	__m128 rows[2];
};
static_assert(sizeof(RuiTransform) == 0x20);

struct RuiInlineImageSpan
{
	uint16_t descriptorIndex;
	uint16_t styleIndex;
	float boundsMin[2];
	float boundsMax[2];
};
static_assert(sizeof(RuiInlineImageSpan) == 0x14);

struct RuiTextLineRecord
{
	float terminalWidth;
	uint32_t breakGlyph;
	float wrappedWidth;
};
static_assert(sizeof(RuiTextLineRecord) == 0xC);

inline constexpr size_t RUI_TEXT_LINE_CAPACITY = 64;
inline constexpr size_t RUI_INLINE_IMAGE_CAPACITY = 64;
inline constexpr size_t RUI_RENDER_JOB_CAPACITY = 179;
inline constexpr size_t RUI_TRANSFORM_CAPACITY = 203;

struct RuiRuntimeState
{
	uint8_t* transformDataCursor;
	uint32_t transformCount;
	uint32_t randomState[3];
	RuiRenderContext* textContext;
	RuiRenderJobState renderJobStates[RUI_RENDER_JOB_CAPACITY];
	uint32_t textScratchUsed;
	uint8_t textScratch[0x2008];
	RuiTextLineRecord textLines[RUI_TEXT_LINE_CAPACITY];
	float textLineTerminalWidth;
	uint32_t textLineCount;
	uint32_t inlineImageCount;
	RuiInlineImageSpan inlineImages[RUI_INLINE_IMAGE_CAPACITY];
	__m128 transformSizes[RUI_TRANSFORM_CAPACITY];
	RuiTransform transforms[RUI_TRANSFORM_CAPACITY];

	float& GetTextLineTerminalWidth(size_t lineIndex)
	{
		return lineIndex < RUI_TEXT_LINE_CAPACITY
			? textLines[lineIndex].terminalWidth
			: textLineTerminalWidth;
	}

	const float& GetTextLineTerminalWidth(size_t lineIndex) const
	{
		return lineIndex < RUI_TEXT_LINE_CAPACITY
			? textLines[lineIndex].terminalWidth
			: textLineTerminalWidth;
	}
};
static_assert(sizeof(RuiRuntimeState) == 0x53E0);
static_assert(offsetof(RuiRuntimeState, transformDataCursor) == 0x0);
static_assert(offsetof(RuiRuntimeState, transformCount) == 0x8);
static_assert(offsetof(RuiRuntimeState, randomState) == 0xC);
static_assert(offsetof(RuiRuntimeState, textContext) == 0x18);
static_assert(offsetof(RuiRuntimeState, renderJobStates) == 0x20);
static_assert(offsetof(RuiRuntimeState, textScratchUsed) == 0x5B8);
static_assert(offsetof(RuiRuntimeState, textScratch) == 0x5BC);
static_assert(offsetof(RuiRuntimeState, textLines) == 0x25C4);
static_assert(offsetof(RuiRuntimeState, textLineTerminalWidth) == 0x28C4);
static_assert(offsetof(RuiRuntimeState, textLineCount) == 0x28C8);
static_assert(offsetof(RuiRuntimeState, inlineImageCount) == 0x28CC);
static_assert(offsetof(RuiRuntimeState, inlineImages) == 0x28D0);
static_assert(offsetof(RuiRuntimeState, transformSizes) == 0x2DD0);
static_assert(offsetof(RuiRuntimeState, transforms) == 0x3A80);
