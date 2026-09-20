#include "rtech/rui/scriptatlas.h"
#include "rtech/rui/atlas.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqarray.h"
#include "vscript/languages/squirrel_re/squirrel/sqstring.h"
#include "vscript/languages/squirrel_re/squirrel/sqstruct.h"

#include <algorithm>
#include <cmath>

static std::string ScriptAtlasString(HSQUIRRELVM sqvm, size_t index)
{
    const SQObject& value = sqvm->_stackOfCurrentFunction[index];
    const SQString* string = value._VAL.asString;
    return std::string(string->_val, string->length);
}

template <ScriptContext context> static uintptr_t ScriptAtlasOwner()
{
    return reinterpret_cast<uintptr_t>(g_pSquirrel[context]->m_pSQVM);
}

ADD_SQFUNC("int", ScriptAtlasCreate, "string name, int width, int height, array<ScriptAtlasImage> images",
           "Creates a VM-owned image atlas with script-defined names and padded rectangles.", ScriptContext::UI | ScriptContext::CLIENT)
{
    const int width = g_pSquirrel[context]->getinteger(sqvm, 2);
    const int height = g_pSquirrel[context]->getinteger(sqvm, 3);
    const SQObject& argument = sqvm->_stackOfCurrentFunction[4];
    if (width <= 0 || height <= 0 || argument._Type != OT_ARRAY || !argument._VAL.asArray)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "ScriptAtlasCreate requires positive dimensions and an image array");
        return SQRESULT_ERROR;
    }
    const SQArray& values = *argument._VAL.asArray;
    if (values._usedSlots <= 0 || values._usedSlots > RUI_IMAGE_DESCRIPTOR_CAPACITY)
    {
        g_pSquirrel[context]->raiseerror(sqvm, "ScriptAtlasCreate image count exceeds atlas limits");
        return SQRESULT_ERROR;
    }
    std::vector<ScriptAtlasImage> images;
    images.reserve(values._usedSlots);
    for (int index = 0; index < values._usedSlots; ++index)
    {
        const SQObject& value = values._values[index];
        const SQStructInstance* definition = value._VAL.asStructInstance;
        if (value._Type != OT_STRUCT || !definition || definition->size != 6 || definition->data[0]._Type != OT_STRING)
        {
            g_pSquirrel[context]->raiseerror(sqvm, "ScriptAtlasCreate expects ScriptAtlasImage definitions");
            return SQRESULT_ERROR;
        }
        for (size_t field = 1; field < 6; ++field)
        {
            if (definition->data[field]._Type != OT_INTEGER || definition->data[field]._VAL.asInteger < 0)
            {
                g_pSquirrel[context]->raiseerror(sqvm, "Script atlas rectangles and gutters must be nonnegative integers");
                return SQRESULT_ERROR;
            }
        }
        const SQString& name = *definition->data[0]._VAL.asString;
        images.push_back({std::string(name._val, name.length), static_cast<uint32_t>(definition->data[1]._VAL.asInteger),
                          static_cast<uint32_t>(definition->data[2]._VAL.asInteger), static_cast<uint32_t>(definition->data[3]._VAL.asInteger),
                          static_cast<uint32_t>(definition->data[4]._VAL.asInteger), static_cast<uint32_t>(definition->data[5]._VAL.asInteger)});
    }
    std::string error;
    const int atlas =
        CScriptAtlasManager::Get().Create(ScriptAtlasOwner<context>(), ScriptAtlasString(sqvm, 1), width, height, std::move(images), error);
    if (!atlas)
    {
        g_pSquirrel[context]->raiseerror(sqvm, error.c_str());
        return SQRESULT_ERROR;
    }
    g_pSquirrel[context]->pushinteger(sqvm, atlas);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", ScriptAtlasDestroy, "int atlas", "Destroys an atlas owned by this VM and cancels its pending image loads.",
           ScriptContext::UI | ScriptContext::CLIENT)
{
    if (!CScriptAtlasManager::Get().Destroy(ScriptAtlasOwner<context>(), g_pSquirrel[context]->getinteger(sqvm, 1)))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid script atlas handle for this VM");
        return SQRESULT_ERROR;
    }
    return SQRESULT_NULL;
}

ADD_SQFUNC("asset", ScriptAtlasGetImage, "int atlas, int image", "Returns the script-defined asset name of an atlas image.",
           ScriptContext::UI | ScriptContext::CLIENT)
{
    const std::string name = CScriptAtlasManager::Get().ImageName(ScriptAtlasOwner<context>(), g_pSquirrel[context]->getinteger(sqvm, 1),
                                                                  g_pSquirrel[context]->getinteger(sqvm, 2));
    if (name.empty())
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid script atlas handle or image index for this VM");
        return SQRESULT_ERROR;
    }
    g_pSquirrel[context]->pushasset(sqvm, name.c_str());
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("bool", ScriptAtlasLoadImage, "int atlas, int image, string source, string version, int fit",
           "Loads a mod-local image or HTTPS URL asynchronously. Fit: 0 cover, 1 contain, 2 stretch; alpha is preserved.",
           ScriptContext::UI | ScriptContext::CLIENT)
{
    const int fit = g_pSquirrel[context]->getinteger(sqvm, 5);
    if (fit < static_cast<int>(ScriptAtlasFit::Cover) || fit > static_cast<int>(ScriptAtlasFit::Stretch))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "ScriptAtlasLoadImage fit must be 0 (cover), 1 (contain), or 2 (stretch)");
        return SQRESULT_ERROR;
    }
    const bool queued = CScriptAtlasManager::Get().LoadImage(ScriptAtlasOwner<context>(), g_pSquirrel[context]->getinteger(sqvm, 1),
                                                             g_pSquirrel[context]->getinteger(sqvm, 2), ScriptAtlasString(sqvm, 3),
                                                             ScriptAtlasString(sqvm, 4), static_cast<ScriptAtlasFit>(fit));
    g_pSquirrel[context]->pushbool(sqvm, queued);
    return SQRESULT_NOTNULL;
}

ADD_SQFUNC("void", ScriptAtlasClearImage, "int atlas, int image, vector color, float alpha",
           "Cancels pending image work and fills the image and gutter with a normalized RGBA color.", ScriptContext::UI | ScriptContext::CLIENT)
{
    const Vector3D color = g_pSquirrel[context]->getvector(sqvm, 3);
    const float alpha = g_pSquirrel[context]->getfloat(sqvm, 4);
    if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) || !std::isfinite(alpha))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Script atlas colors must be finite");
        return SQRESULT_ERROR;
    }
    const auto channel = [](float value) { return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
    const uint32_t rgba = channel(color.x) | (channel(color.y) << 8) | (channel(color.z) << 16) | (channel(alpha) << 24);
    if (!CScriptAtlasManager::Get().ClearImage(ScriptAtlasOwner<context>(), g_pSquirrel[context]->getinteger(sqvm, 1),
                                               g_pSquirrel[context]->getinteger(sqvm, 2), rgba))
    {
        g_pSquirrel[context]->raiseerror(sqvm, "Invalid script atlas handle or image index for this VM");
        return SQRESULT_ERROR;
    }
    return SQRESULT_NULL;
}

ADD_SQFUNC("int", ScriptAtlasGetImageState, "int atlas, int image", "Returns 0 empty, 1 loading, 2 ready, or 3 failed for an atlas image.",
           ScriptContext::UI | ScriptContext::CLIENT)
{
    const auto state = CScriptAtlasManager::Get().ImageState(ScriptAtlasOwner<context>(), g_pSquirrel[context]->getinteger(sqvm, 1),
                                                             g_pSquirrel[context]->getinteger(sqvm, 2));
    g_pSquirrel[context]->pushinteger(sqvm, static_cast<int>(state));
    return SQRESULT_NOTNULL;
}
