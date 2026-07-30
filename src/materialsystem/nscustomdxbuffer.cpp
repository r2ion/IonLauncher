#include <atomic>
#include <array>
#include <cassert>
#include "core/tier0.h"
#include <d3d11.h>
#include <map>
#include <mutex>
#include <winternl.h>
#include <cctype>
#include <string>
#include <cstdint>
#include "cmaterialglue.h"
#include "materialsystem/dx11_device.h"
#include "rtech/pakfilesystem.h"

struct Ns_Constant_Buffer
{
	float data[320];
};

static constexpr size_t kMaxCustomTextureBindings = 60;

struct MaterialTextureMappings_t
{
	std::array<uint64_t, kMaxCustomTextureBindings> m_Slots {};
};

struct MaterialNamedTextureMappings_t
{
	std::array<std::string, kMaxCustomTextureBindings> m_Slots {};
};

using FindNamedTextureFn = void* (__fastcall*)(const char* textureName);
using GetTextureHandleFn = uint64_t(__fastcall*)(void* texture, uint32_t frame);
using BindPixelTextureHandleFn = int64_t(__fastcall*)(uint32_t slot, int16_t textureHandle);

DECLARE_MODULE(NSCustomDXBufferHooks)

static Ns_Constant_Buffer NSCustomDXBuffer;
static std::mutex NSCustomDXBufferMutex;
static std::mutex s_NamedTextureBindingsMutex;

// map to later on associate guid > buffer
static std::map<uint64_t, Ns_Constant_Buffer> NSCustomBuffersPerMaterial = {};
static std::map<uint64_t, MaterialTextureMappings_t> NSMaterialTextureSlotBindings = {};
static std::map<uint64_t, MaterialNamedTextureMappings_t> s_MaterialNamedTextureSlotBindings = {};
static std::unordered_set<uint64_t> NSRegisteredCustomBufferMaterials = {};
static std::unordered_set<uint64_t> NSRegisteredTextureOverrides = {};
static FindNamedTextureFn s_FindNamedTexture = nullptr;
static BindPixelTextureHandleFn s_BindPixelTextureHandle = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Resolve a material-system named texture and bind its current frame
//          to a pixel-shader resource slot.
//-----------------------------------------------------------------------------
static bool BindNamedTextureToPixelShader(uint32_t slot, const char* textureName)
{
	assert(s_FindNamedTexture);
	assert(s_BindPixelTextureHandle);
	assert(slot < kMaxCustomTextureBindings);
	assert(textureName && textureName[0]);

	if (!s_FindNamedTexture || !s_BindPixelTextureHandle || slot >= kMaxCustomTextureBindings || !textureName
		|| !textureName[0])
	{
		return false;
	}

	void* texture = s_FindNamedTexture(textureName);
	if (!texture)
		return false;

	auto* vtable = *reinterpret_cast<void***>(texture);
	if (!vtable)
		return false;

	const auto getTextureHandle = reinterpret_cast<GetTextureHandleFn>(vtable[0x170 / sizeof(void*)]);
	if (!getTextureHandle)
		return false;

	const int16_t textureHandle = static_cast<int16_t>(getTextureHandle(texture, 0));
	if (!textureHandle)
		return false;

	s_BindPixelTextureHandle(slot, textureHandle);
	return true;
}

DECLARE_HOOK(ShaderExecute, materialsystem_dx11.dll + 0x511D0, [](auto& hook, __int64 a1, __int64 a2, __int64 a3, CMaterialGlue_short* a4) -> __int64
{
	CMaterialGlue_short* internal_logic_material = a4;

	auto subResult = hook.Original(a1, a2, a3, internal_logic_material); // IMPORTANT DO NOT REPLACE, NEEDS TO RUN BEFORE ANYTHING ELSE HAPPENS IN SHADEREXECUTE

	const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
	if (!dx11)
		return subResult;

	//bind textures to slots if existing
	if (NSMaterialTextureSlotBindings.contains(internal_logic_material->guid))
	{

		auto& mappings = NSMaterialTextureSlotBindings[internal_logic_material->guid];

		for (size_t slot = 0; slot < mappings.m_Slots.size(); ++slot)
		{
			uint64_t textureGUID = mappings.m_Slots[slot];

			if (textureGUID == 0)
				continue;

			char* texturePointer = g_pakLoadApi->GetAssetBinding(textureGUID);
			if (!texturePointer)
				continue;

			RpakTextureHeader* TextureHeader = (RpakTextureHeader*)texturePointer;
			ID3D11ShaderResourceView* TextureSRV = TextureHeader->shaderResourceView;

			if (!TextureSRV)
				continue;

			dx11.m_pContext->PSSetShaderResources(slot, 1, &TextureSRV);

		}
	}

	MaterialNamedTextureMappings_t namedTextureMappings;
	{
		std::lock_guard<std::mutex> lock(s_NamedTextureBindingsMutex);
		const auto mappings = s_MaterialNamedTextureSlotBindings.find(internal_logic_material->guid);
		if (mappings != s_MaterialNamedTextureSlotBindings.end())
			namedTextureMappings = mappings->second;
	}

	// Named textures intentionally bind after RPAK textures so they win when
	// both mappings target the same shader-resource slot.
	for (size_t slot = 0; slot < namedTextureMappings.m_Slots.size(); ++slot)
	{
		const std::string& textureName = namedTextureMappings.m_Slots[slot];
		if (!textureName.empty())
			BindNamedTextureToPixelShader(static_cast<uint32_t>(slot), textureName.c_str());
	}

	//update custom buffer if material is registered in NSRegisteredCustomBufferMaterials
	if(NSRegisteredCustomBufferMaterials.contains(internal_logic_material->guid))
	{
		D3D11_BUFFER_DESC desc {};
		desc.ByteWidth = sizeof(Ns_Constant_Buffer);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.StructureByteStride = 0;
		desc.MiscFlags = 0;

		static ID3D11Buffer* resource = nullptr;
		if (!resource)
		{
			if (HRESULT res = dx11.m_pDevice->CreateBuffer(&desc, nullptr, &resource); !SUCCEEDED(res))
			{
				spdlog::error("Failed to create buffer {:X}", (uint32_t)res);
				return subResult;
			}
		}

		D3D11_MAPPED_SUBRESOURCE mappedSubResource;
		if (!SUCCEEDED(dx11.m_pContext->Map(resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubResource)))
		{
			spdlog::error("failed to map data");
			return subResult;
		}

		auto pData = (Ns_Constant_Buffer*)mappedSubResource.pData;

		std::lock_guard<std::mutex> lock(NSCustomDXBufferMutex);

		memcpy(pData, &NSCustomBuffersPerMaterial[internal_logic_material->guid], sizeof(Ns_Constant_Buffer));

		dx11.m_pContext->Unmap(resource, 0);
		dx11.m_pContext->PSSetConstantBuffers(4, 1, &resource);
	}
	return subResult;
})

bool isValidMaterialGUID(const std::string& str)
{
	// Must start with "0x" or "0X"
	if (str.size() != 18 || str[0] != '0' || (str[1] != 'x' && str[1] != 'X'))
		return false;

	// Must contain exactly 16 hex digits after 0x
	for (size_t i = 2; i < str.size(); ++i)
	{
		if (!std::isxdigit(static_cast<unsigned char>(str[i])))
			return false;
	}

	return true;
}

template <ScriptContext context> SQRESULT NSRegisterCustomDXBufferForGUID(HSQUIRRELVM sqvm)
{

	auto rPakMaterialGUIDString = (g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1));
	uint64_t rPakMaterialGUID = std::stoull(rPakMaterialGUIDString, nullptr, 16);

	char* AssetFromGUID = g_pakLoadApi->GetAssetBinding(rPakMaterialGUID);

	if (!AssetFromGUID)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm, fmt::format("Asset with GUID {} Doesnt Exist", rPakMaterialGUIDString).c_str());
		return SQRESULT_ERROR;
	}

	auto* base = reinterpret_cast<uint8_t*>(AssetFromGUID);
	auto* GUIDMaterialGlue_short = reinterpret_cast<CMaterialGlue_short*>(base + 16);
	// We need to add 16 to the pointer; Pak_GetAssetBinding returns the full material glue.
	if(!NSRegisteredCustomBufferMaterials.contains( GUIDMaterialGlue_short->guid))
	{
		NS::log::SCRIPT_CL->info("Registered GUID: {} to use the NSCustomDXBuffer system", GUIDMaterialGlue_short->guid);

		NSRegisteredCustomBufferMaterials.insert(GUIDMaterialGlue_short->guid);
	}
	else
	{
		NS::log::SCRIPT_CL->warn("Attempted to register GUID: {} to the NSCustomDXBuffer system, GUID was already registered", GUIDMaterialGlue_short->guid);
	}
	return SQRESULT_NULL;
}

template <ScriptContext context> SQRESULT NSDeregisterCustomDXBufferForGUID(HSQUIRRELVM sqvm)
{

	auto rPakMaterialGUIDString = (g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1));
	uint64_t rPakMaterialGUID = std::stoull(rPakMaterialGUIDString, nullptr, 16);

	char* AssetFromGUID = g_pakLoadApi->GetAssetBinding(rPakMaterialGUID);

	if (!AssetFromGUID)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm, fmt::format("Asset with GUID {} Doesnt Exist", rPakMaterialGUIDString).c_str());
		return SQRESULT_ERROR;
	}

	auto* base = reinterpret_cast<uint8_t*>(AssetFromGUID);
	auto* GUIDMaterialGlue_short = reinterpret_cast<CMaterialGlue_short*>(base + 16);
	// We need to add 16 to the pointer; Pak_GetAssetBinding returns the full material glue.
	if (NSRegisteredCustomBufferMaterials.contains(GUIDMaterialGlue_short->guid))
	{
		NS::log::SCRIPT_CL->info("Deregistered GUID: {} from the NSCustomDXBuffer system", GUIDMaterialGlue_short->guid);

		NSRegisteredCustomBufferMaterials.erase(GUIDMaterialGlue_short->guid);
	}
	else
	{
		NS::log::SCRIPT_CL->warn("Attempted to deregister GUID: {} from the NSCustomDXBuffer system, GUID was not registered", GUIDMaterialGlue_short->guid);
	}
	return SQRESULT_NULL;
}

template <ScriptContext context> SQRESULT NSUpdateCustomDXBufferForGUID(HSQUIRRELVM sqvm)
{

	// get the guid as a string to later conv
	auto rPakMaterialGUIDString = (g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1));

	std::vector<float> NSCustomBufferVector;
	SQArray* NSCustomBufferDataArray = sqvm->_stackOfCurrentFunction[2]._VAL.asArray;

	for (int vIdx = 0; vIdx < NSCustomBufferDataArray->_usedSlots; ++vIdx)
	{
		if (NSCustomBufferDataArray->_values[vIdx]._Type == OT_FLOAT)
		{
			NSCustomBufferVector.push_back(NSCustomBufferDataArray->_values[vIdx]._VAL.asFloat);
		}
	}

	const std::string& guidString = rPakMaterialGUIDString;

	if (!isValidMaterialGUID(guidString))
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm, fmt::format("Malformed Material GUID", (NSCustomBufferDataArray->_usedSlots) * 4, sizeof(Ns_Constant_Buffer)).c_str());
		return SQRESULT_ERROR;
	}

	uint64_t rPakMaterialGUID = std::stoull(rPakMaterialGUIDString, nullptr, 16);

	std::lock_guard<std::mutex> lock(NSCustomDXBufferMutex);

	if ((NSCustomBufferDataArray->_usedSlots) * 4 > sizeof(Ns_Constant_Buffer))
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm,
			fmt::format(
				"Size of Squirrel array exceeds NSCustomDXBuffer size\n\nSquirrel "
				"array in bytes: {}\nNSCustomDXBuffer in bytes: {}",
				(NSCustomBufferDataArray->_usedSlots) * 4,
				sizeof(Ns_Constant_Buffer))
				.c_str());
		return SQRESULT_ERROR;
	}
	for (int vIdx = 0; vIdx < NSCustomBufferVector.size(); ++vIdx)
	{
		// assign buffer to the map using the guid as key
		NSCustomBuffersPerMaterial[rPakMaterialGUID].data[vIdx] = NSCustomBufferVector.at(vIdx);
	}
	return SQRESULT_NULL;
}

template <ScriptContext context> SQRESULT NSBindTextureToMaterial(HSQUIRRELVM sqvm)
{
	
	auto rPakMaterialGUIDString = (g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1));
	auto rPakTextureGUIDString = (g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 2));
	auto rPakShaderSlotBindingInt = (g_pSquirrel[ScriptContext::CLIENT]->getinteger(sqvm, 3));

	uint64_t rPakMaterialGUID = std::stoull(rPakMaterialGUIDString, nullptr, 16);
	char* MatAssetFromGUID = g_pakLoadApi->GetAssetBinding(rPakMaterialGUID);

	if (rPakShaderSlotBindingInt < 0 || rPakShaderSlotBindingInt >= kMaxCustomTextureBindings)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm,
			fmt::format(
				"TextureOverrides only support shader binding slots 0-{}",
				kMaxCustomTextureBindings - 1)
				.c_str());
		return SQRESULT_ERROR;
	}

	if (!MatAssetFromGUID)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm, fmt::format("Material with GUID {} Doesnt Exist", rPakMaterialGUIDString).c_str());
		return SQRESULT_ERROR;
	}

	auto* base = reinterpret_cast<uint8_t*>(MatAssetFromGUID);
	auto* GUIDMaterialGlue_short = reinterpret_cast<CMaterialGlue_short*>(base + 16);

	if (!rPakTextureGUIDString == 0)
	{
		uint64_t rPakTextureGUID = std::stoull(rPakTextureGUIDString, nullptr, 16);
		char* TexAssetFromGUID = g_pakLoadApi->GetAssetBinding(rPakTextureGUID);

		if (!TexAssetFromGUID)
		{
			g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
				sqvm, fmt::format("Texture with GUID {} Doesnt Exist", rPakTextureGUIDString).c_str());
			return SQRESULT_ERROR;
		}
		NSMaterialTextureSlotBindings[GUIDMaterialGlue_short->guid].m_Slots[rPakShaderSlotBindingInt] = rPakTextureGUID;
	}
	else
		NSMaterialTextureSlotBindings[GUIDMaterialGlue_short->guid].m_Slots[rPakShaderSlotBindingInt] = 0;

		

	return SQRESULT_NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Bind a material-system named texture to an RPAK material's pixel
//          shader slot. Resolution is deferred until the material is drawn.
//-----------------------------------------------------------------------------
template <ScriptContext context> SQRESULT NSBindNamedTextureToMaterial(HSQUIRRELVM sqvm)
{
	const char* materialGUIDString = g_pSquirrel[context]->getstring(sqvm, 1);
	const char* textureName = g_pSquirrel[context]->getstring(sqvm, 2);
	const int shaderBindingSlot = g_pSquirrel[context]->getinteger(sqvm, 3);

	if (!isValidMaterialGUID(materialGUIDString))
	{
		g_pSquirrel[context]->raiseerror(sqvm, "Malformed material GUID");
		return SQRESULT_ERROR;
	}

	if (!textureName || !textureName[0])
	{
		g_pSquirrel[context]->raiseerror(sqvm, "Named texture cannot be empty");
		return SQRESULT_ERROR;
	}

	if (shaderBindingSlot < 0 || shaderBindingSlot >= kMaxCustomTextureBindings)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture bindings only support shader binding slots 0-{}",
				kMaxCustomTextureBindings - 1)
				.c_str());
		return SQRESULT_ERROR;
	}

	const uint64_t materialGUID = std::stoull(materialGUIDString, nullptr, 16);
	char* materialAsset = g_pakLoadApi->GetAssetBinding(materialGUID);
	if (!materialAsset)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm, fmt::format("Material with GUID {} does not exist", materialGUIDString).c_str());
		return SQRESULT_ERROR;
	}

	auto* materialBase = reinterpret_cast<uint8_t*>(materialAsset);
	auto* material = reinterpret_cast<CMaterialGlue_short*>(materialBase + 16);

	{
		std::lock_guard<std::mutex> lock(s_NamedTextureBindingsMutex);
		s_MaterialNamedTextureSlotBindings[material->guid].m_Slots[shaderBindingSlot] = textureName;
	}

	NS::log::SCRIPT_CL->info(
		"Bound named texture '{}' to material GUID {:016X} at pixel shader slot {}",
		textureName,
		material->guid,
		shaderBindingSlot);
	return SQRESULT_NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a material-system named texture override from an RPAK
//          material's pixel-shader slot.
//-----------------------------------------------------------------------------
template <ScriptContext context> SQRESULT NSUnbindNamedTextureFromMaterial(HSQUIRRELVM sqvm)
{
	const char* materialGUIDString = g_pSquirrel[context]->getstring(sqvm, 1);
	const int shaderBindingSlot = g_pSquirrel[context]->getinteger(sqvm, 2);

	if (!isValidMaterialGUID(materialGUIDString))
	{
		g_pSquirrel[context]->raiseerror(sqvm, "Malformed material GUID");
		return SQRESULT_ERROR;
	}

	if (shaderBindingSlot < 0 || shaderBindingSlot >= kMaxCustomTextureBindings)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture bindings only support shader binding slots 0-{}",
				kMaxCustomTextureBindings - 1)
				.c_str());
		return SQRESULT_ERROR;
	}

	const uint64_t materialGUID = std::stoull(materialGUIDString, nullptr, 16);
	char* materialAsset = g_pakLoadApi->GetAssetBinding(materialGUID);
	if (!materialAsset)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm, fmt::format("Material with GUID {} does not exist", materialGUIDString).c_str());
		return SQRESULT_ERROR;
	}

	auto* materialBase = reinterpret_cast<uint8_t*>(materialAsset);
	auto* material = reinterpret_cast<CMaterialGlue_short*>(materialBase + 16);

	{
		std::lock_guard<std::mutex> lock(s_NamedTextureBindingsMutex);
		const auto mappings = s_MaterialNamedTextureSlotBindings.find(material->guid);
		if (mappings != s_MaterialNamedTextureSlotBindings.end())
			mappings->second.m_Slots[shaderBindingSlot].clear();
	}

	NS::log::SCRIPT_CL->info(
		"Unbound named texture from material GUID {:016X} at pixel shader slot {}",
		material->guid,
		shaderBindingSlot);
	return SQRESULT_NULL;
}


ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", CustomDXShaders, [](CModule module)
{
	s_FindNamedTexture = module.Offset(0x96F00).RCast<FindNamedTextureFn>();
	s_BindPixelTextureHandle = module.Offset(0x267D0).RCast<BindPixelTextureHandleFn>();
	assert(s_FindNamedTexture);
	assert(s_BindPixelTextureHandle);

	DISPATCH_MODULE(NSCustomDXBufferHooks)

	auto clientUpdateCustomDXBuffer = NSUpdateCustomDXBufferForGUID<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(
		"void",
		"NSUpdateCustomDXBufferForGUID",
		"string rPakMaterialGUID array NSCustomBufferPerMaterialData",
		"",
		clientUpdateCustomDXBuffer);

	auto clientRegisterCustomDXBuffer = NSRegisterCustomDXBufferForGUID<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(
		"void",
		"NSRegisterCustomDXBufferForGUID",
		"string rPakMaterialGUID",
		"",
		clientRegisterCustomDXBuffer);

	auto clientDeregisterCustomDXBuffer = NSDeregisterCustomDXBufferForGUID<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(
		"void",
		"NSDeregisterCustomDXBufferForGUID",
		"string rPakMaterialGUID",
		"",
		clientDeregisterCustomDXBuffer);

	auto clientregisterTexOverride = NSBindTextureToMaterial<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(
		"void",
		"NSBindTextureToMaterial",
		"string rPakMaterialGUID string rPakTextureGUID int shaderBindingSlot",
		"",
		clientregisterTexOverride);

	auto clientBindNamedTexture = NSBindNamedTextureToMaterial<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(
		"void",
		"NSBindNamedTextureToMaterial",
		"string rPakMaterialGUID string textureName int shaderBindingSlot",
		"",
		clientBindNamedTexture);

	auto clientUnbindNamedTexture = NSUnbindNamedTextureFromMaterial<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration(
		"void",
		"NSUnbindNamedTextureFromMaterial",
		"string rPakMaterialGUID int shaderBindingSlot",
		"",
		clientUnbindNamedTexture);
})
