#include <atomic>
#include <array>
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "core/tier0.h"
#include <d3d11.h>
#include <map>
#include <mutex>
#include <winternl.h>
#include <cctype>
#include <string>
#include <cstdint>
#include <vector>
#include <commdlg.h>
#include "cmaterialglue.h"
#include "materialsystem/itextureinternal.h"
#include "rendersystem/schema/texture.g.h"
#include "materialsystem/dx11_device.h"
#include "tier0/frametask.h"
#include <wrl/client.h>
#include "rtech/pakfilesystem.h"
#include "vscript/languages/squirrel_re/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqarray.h"

struct Ns_Constant_Buffer
{
	float data[320];
};
static constexpr size_t kMaterialShaderDataOffset = 0x10;
static_assert(offsetof(CMaterialGlue, guid) == kMaterialShaderDataOffset);


struct MaterialTextureMappings_t
{
	std::array<uint64_t, 60> m_Slots {};
};

struct MaterialNamedTextureMappings_t
{
	std::array<std::string, 60> m_Slots {};
	std::array<int8_t, 60> m_SamplerSlots;

	MaterialNamedTextureMappings_t()
	{
		m_SamplerSlots.fill(-1);
	}
};

using FindNamedTextureFn = ITextureInternal* (__fastcall*)(const char* textureName);
using BindPixelTextureHandleFn = int64_t(__fastcall*)(uint32_t slot, int16_t textureHandle);
using ResolvePixelTextureAndSamplerFn = int64_t(__fastcall*)(uint32_t slot, int16_t textureHandle);
using SetupWaterTextureBindingsFn = void(__fastcall*)(__int64 textureHandles, uint64_t textureCount);

DECLARE_MODULE(NSCustomDXBufferHooks)

static Ns_Constant_Buffer NSCustomDXBuffer;
static std::mutex NSCustomDXBufferMutex;
static std::mutex NamedTextureBindingsMutex;

// map to later on associate guid > buffer
static std::map<uint64_t, Ns_Constant_Buffer> NSCustomBuffersPerMaterial = {};
static std::map<uint64_t, MaterialTextureMappings_t> NSMaterialTextureSlotBindings = {};
static std::map<uint64_t, MaterialNamedTextureMappings_t> MaterialNamedTextureSlotBindings = {};
// Tracks only flags applied by the named-texture binding system so unbinding
// does not clear a compileWater flag authored in the RPAK material itself.
static std::unordered_set<uint64_t> AutoCompileWaterMaterials = {};
static std::atomic<uint16_t> RequestedWaterPasses {};
static std::unordered_set<uint64_t> NSRegisteredCustomBufferMaterials = {};
static std::unordered_set<uint64_t> NSRegisteredTextureOverrides = {};
static FindNamedTextureFn FindNamedTexture = nullptr;
static BindPixelTextureHandleFn BindPixelTextureHandle = nullptr;
static ResolvePixelTextureAndSamplerFn ResolvePixelTextureAndSampler = nullptr;
static SetupWaterTextureBindingsFn SetupWaterTextureBindings = nullptr;
static ID3D11ShaderResourceView** StagedPixelTextures = nullptr;
static ID3D11SamplerState** StagedPixelSamplers = nullptr;
static uint64_t* StagedTextureBindingState = nullptr;
static std::mutex TextureSamplerResolveMutex;
// map of material guid -> custom pixel shader
static std::map<uint64_t, Microsoft::WRL::ComPtr<ID3D11PixelShader>> NSMaterialPixelShaders = {};
static std::mutex NSMaterialPixelShadersMutex;

static uint16_t GetRequestedWaterPasses(const MaterialNamedTextureMappings_t& mappings)
{
	uint16_t requestedPasses = 0;
	for (const std::string& textureName : mappings.m_Slots)
	{
		if (_stricmp(textureName.c_str(), "_rt_WaterRefraction") == 0)
			requestedPasses |= 0x0001;
		else if (_stricmp(textureName.c_str(), "_rt_WaterReflection") == 0)
			requestedPasses |= 0x0002;
	}

	return requestedPasses;
}

static void RefreshRequestedWaterPasses()
{
	uint16_t requestedPasses = 0;
	for (const auto& materialMappings : MaterialNamedTextureSlotBindings)
		requestedPasses |= GetRequestedWaterPasses(materialMappings.second);

	RequestedWaterPasses.store(requestedPasses, std::memory_order_release);
}

static void UpdateCompileWaterFlag(
	CMaterialGlue* material, const MaterialNamedTextureMappings_t& mappings)
{
	if (GetRequestedWaterPasses(mappings) != 0)
	{
		if ((material->flags2 & 0x00080000) == 0)
			AutoCompileWaterMaterials.insert(material->guid);

		material->flags2 |= 0x00080000;
		return;
	}

	if (AutoCompileWaterMaterials.erase(material->guid) != 0)
		material->flags2 &= ~0x00080000;
}

struct FXCWatcher_t
{
	std::shared_ptr<std::atomic<bool>> m_Active;
	std::jthread m_Thread;
};


static std::unordered_map<uint64_t, FXCWatcher_t> NSFXCWatchers = {};
static std::mutex NSFXCWatchersMutex;

bool isValidMaterialGUID(const std::string& str)
{
    if (str.size() != 18 || str[0] != '0' || (str[1] != 'x' && str[1] != 'X'))
        return false;

    for (size_t i = 2; i < str.size(); ++i)
    {
        if (!std::isxdigit(static_cast<unsigned char>(str[i])))
            return false;
    }

    return true;
}

static ID3D11SamplerState* ResolveTextureSampler(int16_t textureHandle)
{
	if (!ResolvePixelTextureAndSampler || !StagedPixelTextures || !StagedPixelSamplers
		|| !StagedTextureBindingState || textureHandle == 0)
	{
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(TextureSamplerResolveMutex);
	ID3D11ShaderResourceView* const previousTexture = StagedPixelTextures[0];
	ID3D11SamplerState* const previousSampler = StagedPixelSamplers[0];
	const uint64_t previousBindingState = *StagedTextureBindingState;

	ResolvePixelTextureAndSampler(0, textureHandle);
	ID3D11SamplerState* const sampler = StagedPixelSamplers[0];

	StagedPixelTextures[0] = previousTexture;
	StagedPixelSamplers[0] = previousSampler;
	*StagedTextureBindingState = previousBindingState;
	return sampler;
}

static void BindTextureHandleToPixelShader(uint32_t textureSlot, uint32_t samplerSlot, int16_t textureHandle)
{
	BindPixelTextureHandle(textureSlot, textureHandle);

	ID3D11SamplerState* sampler = ResolveTextureSampler(textureHandle);
	const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
	if (sampler && dx11)
		dx11.m_pContext->PSSetSamplers(samplerSlot, 1, &sampler);
}


static bool BindNamedTextureToPixelShader(uint32_t textureSlot, uint32_t samplerSlot, const char* textureName)
{
	if (!FindNamedTexture || !BindPixelTextureHandle || textureSlot >= 59
		|| samplerSlot >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 || !textureName || !textureName[0])
	{
		return false;
	}
	ITextureInternal* const texture = FindNamedTexture(textureName);
	if (!texture)
		return false;

	const int16_t textureHandle = static_cast<int16_t>(texture->GetTextureHandle(0));
	if (!textureHandle)
		return false;

	BindTextureHandleToPixelShader(textureSlot, samplerSlot, textureHandle);

	const int16_t depthTextureHandle = static_cast<int16_t>(texture->GetDepthTextureHandle());
	if (depthTextureHandle)
		BindTextureHandleToPixelShader(textureSlot + 1, samplerSlot + 1, depthTextureHandle);

	return true;
}

static constexpr const char* waterVcsPaths[] = {
	"platform/shaders/fxc/water_%s.vcs",
};

struct VcsSet_t
{
	bool m_Loaded = false;
	uint32_t m_SlotCount = 1;
	std::vector<std::pair<uint32_t, uint32_t>> m_Dictionary;
	std::vector<std::pair<uint32_t, uint32_t>> m_Aliases;    
	std::map<uint32_t, std::vector<uint8_t>> m_Blocks; 

	int32_t FindIndex(uint32_t comboId) const
	{
		if (!m_Loaded || m_SlotCount == 0)
			return -1;

		uint32_t key = comboId / m_SlotCount;

		const auto alias = std::lower_bound(m_Aliases.begin(), m_Aliases.end(), key,
			[](const std::pair<uint32_t, uint32_t>& entry, uint32_t value) { return entry.first < value; });
		if (alias != m_Aliases.end() && alias->first == key)
			key = alias->second;

		const auto entry = std::lower_bound(m_Dictionary.begin(), m_Dictionary.end(), key,
			[](const std::pair<uint32_t, uint32_t>& candidate, uint32_t value) { return candidate.first < value; });
		if (entry == m_Dictionary.end() || entry->first != key)
			return -1;

		return static_cast<int32_t>(entry - m_Dictionary.begin());
	}

	const std::vector<uint8_t>* ByteCodeFor(uint32_t comboId) const
	{
		const int32_t index = FindIndex(comboId);
		if (index < 0)
			return nullptr;

		const auto block = m_Blocks.find(m_Dictionary[index].first);
		return block == m_Blocks.end() ? nullptr : &block->second;
	}

	bool Load(const char* path);
};


static VcsSet_t g_WaterPsSet;
static VcsSet_t g_WaterVsSet;
static std::map<__int64, std::pair<uint32_t, uint32_t>> WaterCombos;
static std::map<uint32_t, Microsoft::WRL::ComPtr<ID3D11PixelShader>> WaterVcsPixelShaders;
static std::map<uint32_t, Microsoft::WRL::ComPtr<ID3D11VertexShader>> WaterVcsVertexShaders;

bool VcsSet_t::Load(const char* path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
		return false;

	const std::streamoff size = file.tellg();
	if (size < 0x20)
		return false;

	std::vector<uint8_t> data(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
		return false;

	const auto readU32 = [&data](size_t offset) -> uint32_t
	{
		uint32_t value = 0;
		if (offset + sizeof(uint32_t) <= data.size())
			memcpy(&value, data.data() + offset, sizeof(uint32_t));
		return value;
	};

	if (readU32(0) != 6)
		return false;

	m_SlotCount = readU32(8);
	if (m_SlotCount == 0)
		m_SlotCount = 1;

	const uint32_t entryCount = readU32(20);
	const size_t aliasCountOffset = 28 + static_cast<size_t>(entryCount) * 8;
	const uint32_t aliasCount = readU32(aliasCountOffset);
	const size_t aliasOffset = aliasCountOffset + 4;

	for (uint32_t i = 0; i < entryCount; ++i)
	{
		const uint32_t key = readU32(28 + static_cast<size_t>(i) * 8);
		const uint32_t offset = readU32(28 + static_cast<size_t>(i) * 8 + 4);
		if (offset == 0xFFFFFFFF)
			continue;

		const uint32_t header = readU32(offset);
		const uint32_t byteCodeLength = readU32(offset + 8);
		if ((header >> 31) == 0 || byteCodeLength == 0 || offset + 12 + byteCodeLength > data.size())
			continue;

		const uint8_t* byteCode = data.data() + offset + 12;
		if (memcmp(byteCode, "DXBC", 4) != 0)
			continue;

		m_Dictionary.emplace_back(key, offset);

		m_Blocks.emplace(key, std::vector<uint8_t>(byteCode, byteCode + byteCodeLength));
	}

	for (uint32_t i = 0; i < aliasCount; ++i)
	{
		const uint32_t key = readU32(aliasOffset + static_cast<size_t>(i) * 8);
		const uint32_t value = readU32(aliasOffset + static_cast<size_t>(i) * 8 + 4);
		m_Aliases.emplace_back(key, value);
	}

	m_Loaded = !m_Dictionary.empty();
	return m_Loaded;
}

static bool LoadWaterVcsSet(VcsSet_t& set, const char* stage)
{
	if (set.m_Loaded)
		return true;

	for (const char* pattern : waterVcsPaths)
	{
		std::string path = pattern;
		path.replace(path.find("%s"), 2, stage);
		if (set.Load(path.c_str()))
			return true;
	}

	spdlog::warn("water_{}.vcs not found; the engine's own water shaders will be used", stage);
	return false;
}

DECLARE_HOOK(InitWaterShader, materialsystem_dx11.dll + 0x41B50, [](auto& hook, __int64 a1, __int64 a2, __int64 a3, __int64 a4) -> __int64
{
	const __int64 result = hook.Original(a1, a2, a3, a4);

	if (!a2 || !a4)
		return result;

	auto* const shaderParams = *reinterpret_cast<uint8_t**>(a2);
	if (!shaderParams)
		return result;
	
	if (!g_WaterPsSet.m_Loaded)
		LoadWaterVcsSet(g_WaterPsSet, "ps40");
	if (!g_WaterVsSet.m_Loaded)
		LoadWaterVcsSet(g_WaterVsSet, "vs40");
	

	if (!g_WaterPsSet.m_Loaded || !g_WaterVsSet.m_Loaded)
		return result;


	const uint32_t pixelId = *reinterpret_cast<uint32_t*>(a4 + 0x60);
	const uint32_t vertexId = *reinterpret_cast<uint32_t*>(a4 + 0x64);
	WaterCombos[a3] = std::make_pair(pixelId, vertexId);
	
	return result;
})


DECLARE_HOOK(Water_Execute, materialsystem_dx11.dll + 0x41AC0, [](auto& hook, __int64 a1, __int64 a2, __int64 a3, __int64 a4) -> __int64
{

	if (!a4)
		return 0;

	const __int64 result = *reinterpret_cast<const __int64*>(a4 + 8);

	const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
	if (!dx11)
		return result;

	uint32_t pixelId = 0;
	uint32_t vertexId = 0;
    auto entry = WaterCombos.find(a3);
    if (entry == WaterCombos.end())
        entry = WaterCombos.find(a4);

    if (entry != WaterCombos.end())
    {
        pixelId = entry->second.first;
        vertexId = entry->second.second;
    }
        
	const std::vector<uint8_t>* const vertexByteCode =
		vertexId != 0 ? g_WaterVsSet.ByteCodeFor(vertexId) : nullptr;
	const std::vector<uint8_t>* const pixelByteCode =
		pixelId != 0 ? g_WaterPsSet.ByteCodeFor(pixelId) : nullptr;

	ID3D11VertexShader* vertexShader = nullptr;
	ID3D11PixelShader* pixelShader = nullptr;

	if (vertexByteCode)
	{
		const auto cached = WaterVcsVertexShaders.find(vertexId);
		if (cached != WaterVcsVertexShaders.end())
		{
			vertexShader = cached->second.Get();
		}
		else if (SUCCEEDED(dx11.m_pDevice->CreateVertexShader(
				vertexByteCode->data(), vertexByteCode->size(), nullptr, &vertexShader)))
		{
			WaterVcsVertexShaders.emplace(vertexId, vertexShader);
		}
		else
		{
			spdlog::error("failed to create the water vertex shader for id {}", vertexId);
		}
	}

	if (pixelByteCode)
	{
		const auto cached = WaterVcsPixelShaders.find(pixelId);
		if (cached != WaterVcsPixelShaders.end())
		{
			pixelShader = cached->second.Get();
		}
		else if (SUCCEEDED(dx11.m_pDevice->CreatePixelShader(
				pixelByteCode->data(), pixelByteCode->size(), nullptr, &pixelShader)))
		{
			WaterVcsPixelShaders.emplace(pixelId, pixelShader);
		}
		else
		{
			spdlog::error("failed to create the water pixel shader for id {}", pixelId);
		}
	}

	
	if (vertexShader && pixelShader)
	{
		dx11.m_pContext->VSSetShader(vertexShader, nullptr, 0);
		dx11.m_pContext->PSSetShader(pixelShader, nullptr, 0);
	}
	

		struct TonemapGlobals_t
		{
			float m_LightScale;
			uint32_t m_HistoryPos;
			float m_TargetHistory[10];
		};
		static_assert(sizeof(TonemapGlobals_t) == 48, "must match the shader's declaration");

		static ID3D11Buffer* tonemapBuffer = nullptr;
		static ID3D11ShaderResourceView* tonemapView = nullptr;

	if (!tonemapView && !tonemapBuffer)
	{
		TonemapGlobals_t globals {};
		globals.m_LightScale = 1.0f;

		D3D11_BUFFER_DESC bufferDesc {};
		bufferDesc.ByteWidth = sizeof(TonemapGlobals_t);
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = sizeof(TonemapGlobals_t);

		D3D11_SUBRESOURCE_DATA initial {};
		initial.pSysMem = &globals;

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc {};
		viewDesc.Format = DXGI_FORMAT_UNKNOWN;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		viewDesc.Buffer.FirstElement = 0;
		viewDesc.Buffer.NumElements = 1;

		if (FAILED(dx11.m_pDevice->CreateBuffer(&bufferDesc, &initial, &tonemapBuffer))
			|| FAILED(dx11.m_pDevice->CreateShaderResourceView(tonemapBuffer, &viewDesc, &tonemapView)))
		{
			spdlog::error("failed to create the tonemapGlobals buffer for t16");
			if (tonemapBuffer)
			{
				tonemapBuffer->Release();
				tonemapBuffer = nullptr;
			}
		}
		

		if (tonemapView)
			dx11.m_pContext->PSSetShaderResources(16, 1, &tonemapView);
	}

	// The water constant buffer, at the offset the engine's own execute uses.
	ID3D11Buffer* const* const constantBuffer = reinterpret_cast<ID3D11Buffer* const*>(a4 + 0x10);
	dx11.m_pContext->VSSetConstantBuffers(0, 1, constantBuffer);
	dx11.m_pContext->PSSetConstantBuffers(0, 1, constantBuffer);
	if (SetupWaterTextureBindings)
		SetupWaterTextureBindings(*reinterpret_cast<const __int64*>(a4 + 0x78), 14);

	return result;
})

DECLARE_HOOK(ShaderExecute, materialsystem_dx11.dll + 0x511D0, [](auto& hook, __int64 a1, __int64 a2, __int64 a3, void* rawMaterialData) -> __int64
{
	auto subResult = hook.Original(a1, a2, a3, rawMaterialData); // IMPORTANT DO NOT REPLACE, NEEDS TO RUN BEFORE ANYTHING ELSE HAPPENS IN SHADEREXECUTE
	auto* const material = reinterpret_cast<CMaterialGlue*>(
		reinterpret_cast<std::uint8_t*>(rawMaterialData) - kMaterialShaderDataOffset);

	const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
	if (!dx11)
		return subResult;

	//bind textures to slots if existing
	if (NSMaterialTextureSlotBindings.contains(material->guid))
	{

		auto& mappings = NSMaterialTextureSlotBindings[material->guid];

		for (size_t slot = 0; slot < mappings.m_Slots.size(); ++slot)
		{
			uint64_t textureGUID = mappings.m_Slots[slot];

			if (textureGUID == 0)
				continue;

			char* texturePointer = g_pakLoadApi->GetAssetBinding(textureGUID);
			if (!texturePointer)
				continue;

			auto* textureHeader = reinterpret_cast<TextureAsset_s*>(texturePointer);
			ID3D11ShaderResourceView* TextureSRV = textureHeader->shaderResourceView;

			if (!TextureSRV)
				continue;

			dx11.m_pContext->PSSetShaderResources(slot, 1, &TextureSRV);

		}
	}

	MaterialNamedTextureMappings_t namedTextureMappings;
	{
		std::lock_guard<std::mutex> lock(NamedTextureBindingsMutex);
		const auto mappings = MaterialNamedTextureSlotBindings.find(material->guid);
		if (mappings != MaterialNamedTextureSlotBindings.end())
			namedTextureMappings = mappings->second;
	}

	// Named textures intentionally bind after RPAK textures so they win when
	// both mappings target the same shader-resource slot.
	for (size_t slot = 0; slot < namedTextureMappings.m_Slots.size(); ++slot)
	{
		const std::string& textureName = namedTextureMappings.m_Slots[slot];
		const int samplerSlot = namedTextureMappings.m_SamplerSlots[slot];
		if (!textureName.empty() && samplerSlot >= 0)
		{
			BindNamedTextureToPixelShader(
				static_cast<uint32_t>(slot), static_cast<uint32_t>(samplerSlot), textureName.c_str());
		}
	}

	//update custom buffer if material is registered in NSRegisteredCustomBufferMaterials
	if(NSRegisteredCustomBufferMaterials.contains(material->guid))
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

		memcpy(pData, &NSCustomBuffersPerMaterial[material->guid], sizeof(Ns_Constant_Buffer));

		dx11.m_pContext->Unmap(resource, 0);
		dx11.m_pContext->PSSetConstantBuffers(4, 1, &resource);
	}


    // If a custom pixel shader has been registered for this material, set it now
    {
        std::lock_guard<std::mutex> lock(NSMaterialPixelShadersMutex);
        auto it = NSMaterialPixelShaders.find(material->guid);
        if (it != NSMaterialPixelShaders.end() && it->second != nullptr)
        {
            dx11.m_pContext->PSSetShader(it->second.Get(), nullptr, 0);
        }
    }

	return subResult;
})


DECLARE_HOOK(UpdateWaterRenderTargets, client.dll + 0x3756A0, [](auto& hook, __int64 viewRender, uint8_t* view, void* originalView, unsigned int clearFlags, __int64 finalRenderTarget) -> __int64
{
	const uint16_t requestedPasses = RequestedWaterPasses.load(std::memory_order_acquire);
	if (view && requestedPasses != 0)
	{
		auto* waterDrawFlags = reinterpret_cast<uint16_t*>(view + 0x508A4);
		*waterDrawFlags |= requestedPasses;

		if ((requestedPasses & 0x0002) != 0)
		{
			auto* waterPlane = reinterpret_cast<float*>(view + 0x50890);
			if (waterPlane[0] == 0.0f && waterPlane[1] == 0.0f && waterPlane[2] == 0.0f)
			{
				waterPlane[2] = 1.0f;
				waterPlane[3] = reinterpret_cast<float*>(view)[2] - 64.0f;
			}
		}
	}

	return hook.Original(viewRender, view, originalView, clearFlags, finalRenderTarget);
})

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

	auto* material = reinterpret_cast<CMaterialGlue*>(AssetFromGUID);
	if(!NSRegisteredCustomBufferMaterials.contains(material->guid))
	{
		NS::log::SCRIPT_CL->info("Registered GUID: {} to use the NSCustomDXBuffer system", material->guid);

		NSRegisteredCustomBufferMaterials.insert(material->guid);
	}
	else
	{
		NS::log::SCRIPT_CL->warn("Attempted to register GUID: {} to the NSCustomDXBuffer system, GUID was already registered", material->guid);
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

	auto* material = reinterpret_cast<CMaterialGlue*>(AssetFromGUID);
	if (NSRegisteredCustomBufferMaterials.contains(material->guid))
	{
		NS::log::SCRIPT_CL->info("Deregistered GUID: {} from the NSCustomDXBuffer system", material->guid);

		NSRegisteredCustomBufferMaterials.erase(material->guid);
	}
	else
	{
		NS::log::SCRIPT_CL->warn("Attempted to deregister GUID: {} from the NSCustomDXBuffer system, GUID was not registered", material->guid);
	}
	return SQRESULT_NULL;
}

template <ScriptContext context> SQRESULT NSUpdateCustomDXBufferForGUID(HSQUIRRELVM sqvm)
{
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

	if (rPakShaderSlotBindingInt < 0 || rPakShaderSlotBindingInt >= 60)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm,
			fmt::format(
				"TextureOverrides only support shader binding slots 0-{}",
				59)
				.c_str());
		return SQRESULT_ERROR;
	}

	if (!MatAssetFromGUID)
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(
			sqvm, fmt::format("Material with GUID {} Doesnt Exist", rPakMaterialGUIDString).c_str());
		return SQRESULT_ERROR;
	}

	auto* material = reinterpret_cast<CMaterialGlue*>(MatAssetFromGUID);

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
		NSMaterialTextureSlotBindings[material->guid].m_Slots[rPakShaderSlotBindingInt] = rPakTextureGUID;
	}
	else
		NSMaterialTextureSlotBindings[material->guid].m_Slots[rPakShaderSlotBindingInt] = 0;



	return SQRESULT_NULL;
}


template <ScriptContext context> SQRESULT NSBindNamedTextureToMaterial(HSQUIRRELVM sqvm)
{
	const char* materialGUIDString = g_pSquirrel[context]->getstring(sqvm, 1);
	const char* textureName = g_pSquirrel[context]->getstring(sqvm, 2);
	const int textureBindingSlot = g_pSquirrel[context]->getinteger(sqvm, 3);
	const int samplerBindingSlot = g_pSquirrel[context]->getinteger(sqvm, 4);

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

	if (textureBindingSlot < 0 || textureBindingSlot >= 59)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture bindings only support base shader slots 0-{} because depth uses the adjacent slot",
				58)
				.c_str());
		return SQRESULT_ERROR;
	}

	if (samplerBindingSlot < 0 || samplerBindingSlot >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture sampler bindings only support base slots 0-{} because color/depth samplers use adjacent slots",
				D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 2)
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

	auto* material = reinterpret_cast<CMaterialGlue*>(materialAsset);

	{
		std::lock_guard<std::mutex> lock(NamedTextureBindingsMutex);
		auto& mappings = MaterialNamedTextureSlotBindings[material->guid];
		mappings.m_Slots[textureBindingSlot] = textureName;
		mappings.m_SamplerSlots[textureBindingSlot] = static_cast<int8_t>(samplerBindingSlot);
		UpdateCompileWaterFlag(material, mappings);
		RefreshRequestedWaterPasses();
	}

	NS::log::SCRIPT_CL->info(
		"Bound named texture '{}' to material GUID {:016X} at t{}/t{} with samplers s{}/s{}",
		textureName,
		material->guid,
		textureBindingSlot,
		textureBindingSlot + 1,
		samplerBindingSlot,
		samplerBindingSlot + 1);
	return SQRESULT_NULL;
}

template <ScriptContext context> SQRESULT NSUnbindNamedTextureFromMaterial(HSQUIRRELVM sqvm)
{
	const char* materialGUIDString = g_pSquirrel[context]->getstring(sqvm, 1);
	const int shaderBindingSlot = g_pSquirrel[context]->getinteger(sqvm, 2);

	if (!isValidMaterialGUID(materialGUIDString))
	{
		g_pSquirrel[context]->raiseerror(sqvm, "Malformed material GUID");
		return SQRESULT_ERROR;
	}

	if (shaderBindingSlot < 0 || shaderBindingSlot >= 59)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture bindings only support base shader slots 0-{} because depth uses the adjacent slot",
				58)
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

	auto* material = reinterpret_cast<CMaterialGlue*>(materialAsset);

	{
		std::lock_guard<std::mutex> lock(NamedTextureBindingsMutex);
		const auto mappings = MaterialNamedTextureSlotBindings.find(material->guid);
		if (mappings != MaterialNamedTextureSlotBindings.end())
		{
			mappings->second.m_Slots[shaderBindingSlot].clear();
			mappings->second.m_SamplerSlots[shaderBindingSlot] = -1;
			UpdateCompileWaterFlag(material, mappings->second);
			RefreshRequestedWaterPasses();
		}
	}

	NS::log::SCRIPT_CL->info(
		"Unbound named texture from material GUID {:016X} at pixel shader slot {}",
		material->guid,
		shaderBindingSlot);
	return SQRESULT_NULL;
}

ON_DLL_LOAD_CLIENT("materialsystem_dx11.dll", CustomDXShaders, [](CModule module)
{
	FindNamedTexture = module.Offset(0x96F00).RCast<FindNamedTextureFn>();
	BindPixelTextureHandle = module.Offset(0x267D0).RCast<BindPixelTextureHandleFn>();
	ResolvePixelTextureAndSampler = module.Offset(0x26430).RCast<ResolvePixelTextureAndSamplerFn>();
	SetupWaterTextureBindings = module.Offset(0x264F0).RCast<SetupWaterTextureBindingsFn>();
	StagedPixelTextures = module.Offset(0x19ACA70).RCast<ID3D11ShaderResourceView**>();
	StagedPixelSamplers = module.Offset(0x19AC9F0).RCast<ID3D11SamplerState**>();
	StagedTextureBindingState = module.Offset(0x19ACB30).RCast<uint64_t*>();

	DISPATCH_HOOK(NSCustomDXBufferHooks, InitWaterShader)
	DISPATCH_HOOK(NSCustomDXBufferHooks, Water_Execute)
	DISPATCH_HOOK(NSCustomDXBufferHooks, ShaderExecute)

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
		"string rPakMaterialGUID string textureName int textureBindingSlot int samplerBindingSlot",
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

ON_DLL_LOAD_CLIENT("client.dll", CustomWaterRenderTargets, [](CModule module)
{
	NOTE_UNUSED(module);
	DISPATCH_HOOK(NSCustomDXBufferHooks, UpdateWaterRenderTargets)
})
