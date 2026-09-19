#pragma once

#include "rtech/rui/atlas.h"

#include <span>
#include <string>
#include <vector>

struct PakAssetBinding_s;

struct RuiImageAtlasAppend
{
    std::string pakPath;
    std::string sourceAtlasPath;
    std::string targetAtlasPath;
};

void RuiConfigureImageAtlasAppends(std::span<const RuiImageAtlasAppend> appends);
void RuiConfigureImageAtlasAssetBinding(PakAssetBinding_s& binding);
void RuiBeforeImageAtlasReplace(RuiImageAtlasHandle atlasHandle);
void RuiAfterImageAtlasReplace(RuiImageAtlasHandle atlasHandle);
void RuiBeginImageAtlasFrame(uint16_t rendererIndex);
