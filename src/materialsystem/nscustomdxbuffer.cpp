#include <atomic>
#include <array>
#include <cassert>
#include <condition_variable>
#include <fstream>
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
#include "materialsystem/nscustomdxbuffer.h"
#include "tier0/frametask.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#pragma comment(lib, "d3dcompiler.lib")
#include "rtech/pakfilesystem.h"
#include "vscript/squirrel/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqarray.h"

struct Ns_Constant_Buffer
{
	float data[320];
};

static constexpr size_t kMaxCustomTextureBindings = 60;
static constexpr uint32_t kCompileWaterGlueFlag2 = 0x00080000;
static constexpr size_t kMaterialShaderDataOffset = 0x10;
static_assert(offsetof(CMaterialGlue, guid) == kMaterialShaderDataOffset);
static constexpr const char* kWaterReflectionTextureName = "_rt_WaterReflection";
static constexpr const char* kWaterRefractionTextureName = "_rt_WaterRefraction";
static constexpr uint16_t kDrawWaterRefraction = 0x0001;
static constexpr uint16_t kDrawWaterReflection = 0x0002;
static constexpr size_t kViewWaterPlaneOffset = 0x50890;
static constexpr size_t kViewWaterDrawFlagsOffset = 0x508A4;

struct MaterialTextureMappings_t
{
	std::array<uint64_t, kMaxCustomTextureBindings> m_Slots {};
};

struct MaterialNamedTextureMappings_t
{
	std::array<std::string, kMaxCustomTextureBindings> m_Slots {};
	std::array<int8_t, kMaxCustomTextureBindings> m_SamplerSlots;

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
static std::mutex s_NamedTextureBindingsMutex;

// map to later on associate guid > buffer
static std::map<uint64_t, Ns_Constant_Buffer> NSCustomBuffersPerMaterial = {};
static std::map<uint64_t, MaterialTextureMappings_t> NSMaterialTextureSlotBindings = {};
static std::map<uint64_t, MaterialNamedTextureMappings_t> s_MaterialNamedTextureSlotBindings = {};
// Tracks only flags applied by the named-texture binding system so unbinding
// does not clear a compileWater flag authored in the RPAK material itself.
static std::unordered_set<uint64_t> s_AutoCompileWaterMaterials = {};
static std::atomic<uint16_t> s_RequestedWaterPasses {};
static std::unordered_set<uint64_t> NSRegisteredCustomBufferMaterials = {};
static std::unordered_set<uint64_t> NSRegisteredTextureOverrides = {};
static FindNamedTextureFn s_FindNamedTexture = nullptr;
static BindPixelTextureHandleFn s_BindPixelTextureHandle = nullptr;
static ResolvePixelTextureAndSamplerFn s_ResolvePixelTextureAndSampler = nullptr;
static SetupWaterTextureBindingsFn s_SetupWaterTextureBindings = nullptr;
static ID3D11ShaderResourceView** s_StagedPixelTextures = nullptr;
static ID3D11SamplerState** s_StagedPixelSamplers = nullptr;
static uint64_t* s_StagedTextureBindingState = nullptr;
static std::mutex s_TextureSamplerResolveMutex;
// map of material guid -> custom pixel shader
static std::map<uint64_t, Microsoft::WRL::ComPtr<ID3D11PixelShader>> NSMaterialPixelShaders = {};
static std::mutex NSMaterialPixelShadersMutex;

static uint16_t GetRequestedWaterPasses(const MaterialNamedTextureMappings_t& mappings)
{
	uint16_t requestedPasses = 0;
	for (const std::string& textureName : mappings.m_Slots)
	{
		if (_stricmp(textureName.c_str(), kWaterRefractionTextureName) == 0)
			requestedPasses |= kDrawWaterRefraction;
		else if (_stricmp(textureName.c_str(), kWaterReflectionTextureName) == 0)
			requestedPasses |= kDrawWaterReflection;
	}

	return requestedPasses;
}

// Must be called while s_NamedTextureBindingsMutex is held.
static void RefreshRequestedWaterPasses()
{
	uint16_t requestedPasses = 0;
	for (const auto& materialMappings : s_MaterialNamedTextureSlotBindings)
		requestedPasses |= GetRequestedWaterPasses(materialMappings.second);

	s_RequestedWaterPasses.store(requestedPasses, std::memory_order_release);
}

// CMaterialGlue::IsWater reads flags2 bit 19, which is the RPAK equivalent of
// VMT %compileWater. The client render-target producer still needs a separate
// hook because engine.dll only creates its water records from BSP leafwaterdata.
static void UpdateCompileWaterFlag(
	CMaterialGlue* material, const MaterialNamedTextureMappings_t& mappings)
{
	if (GetRequestedWaterPasses(mappings) != 0)
	{
		if ((material->flags2 & kCompileWaterGlueFlag2) == 0)
			s_AutoCompileWaterMaterials.insert(material->guid);

		material->flags2 |= kCompileWaterGlueFlag2;
		return;
	}

	if (s_AutoCompileWaterMaterials.erase(material->guid) != 0)
		material->flags2 &= ~kCompileWaterGlueFlag2;
}

struct FXCWatcher_t
{
	std::shared_ptr<std::atomic<bool>> m_Active;
	std::jthread m_Thread;
};

// Watchers remain joinable so map teardown can quiesce them before unloading
// the materials their GUIDs refer to.
static std::unordered_map<uint64_t, FXCWatcher_t> NSFXCWatchers = {};
static std::mutex NSFXCWatchersMutex;

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


static void QueuePixelShaderHotReload(
	std::vector<uint8_t> bytecode,
	std::wstring filePath,
	uint64_t materialGUID,
	std::shared_ptr<std::atomic<bool>> active)
{
	RunInMainThread(
		[bytecode = std::move(bytecode), filePath = std::move(filePath), materialGUID, active = std::move(active)]()
		{
			if (!active->load(std::memory_order_acquire))
				return;

			const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
			if (!dx11)
				return;

			Microsoft::WRL::ComPtr<ID3D11PixelShader> newShader;
			const HRESULT result = dx11.m_pDevice->CreatePixelShader(
				bytecode.data(), bytecode.size(), nullptr, newShader.GetAddressOf());
			if (FAILED(result))
			{
				spdlog::error(
					"Failed to hotload pixel shader from {} for GUID {:016X} (HRESULT 0x{:08X})",
					std::string(filePath.begin(), filePath.end()),
					materialGUID,
					static_cast<uint32_t>(result));
				return;
			}

			std::lock_guard<std::mutex> lock(NSMaterialPixelShadersMutex);
			if (!active->load(std::memory_order_acquire))
				return;

			NSMaterialPixelShaders[materialGUID] = std::move(newShader);
			spdlog::info(
				"Hotloaded pixel shader from {} for GUID {:016X}",
				std::string(filePath.begin(), filePath.end()),
				materialGUID);
		});
}

static bool WatchFXCAndHotReload(std::wstring filePath, uint64_t materialGUID)
{
	std::lock_guard<std::mutex> lock(NSFXCWatchersMutex);
	if (NSFXCWatchers.contains(materialGUID))
		return false;

	auto active = std::make_shared<std::atomic<bool>>(true);
	std::jthread watcher([filePath = std::move(filePath), materialGUID, active](std::stop_token stopToken)
	{
		using namespace std::chrono_literals;
		std::condition_variable_any wakeCondition;
		std::mutex wakeMutex;
		std::error_code ec;
		auto lastWrite = std::filesystem::last_write_time(filePath, ec);

		while (!stopToken.stop_requested())
		{
			std::unique_lock waitLock(wakeMutex);
			wakeCondition.wait_for(waitLock, stopToken, 500ms, [] { return false; });
			waitLock.unlock();
			if (stopToken.stop_requested())
				break;

			auto curr = std::filesystem::last_write_time(filePath, ec);
			if (ec)
				continue;
			if (curr != lastWrite)
			{
				lastWrite = curr;
				std::vector<uint8_t> bytecode;
				const std::wstring ext = std::filesystem::path(filePath).extension().wstring();
				try
				{
					if (_wcsicmp(ext.c_str(), L".cso") == 0 || _wcsicmp(ext.c_str(), L".bin") == 0)
					{
						Microsoft::WRL::ComPtr<ID3DBlob> blob;
						if (SUCCEEDED(D3DReadFileToBlob(filePath.c_str(), blob.GetAddressOf())) && blob)
						{
							const auto* begin = static_cast<const uint8_t*>(blob->GetBufferPointer());
							bytecode.assign(begin, begin + blob->GetBufferSize());
						}
					}
					else
					{
						Microsoft::WRL::ComPtr<ID3DBlob> code;
						Microsoft::WRL::ComPtr<ID3DBlob> error;
						const UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
						const HRESULT hr = D3DCompileFromFile(
							filePath.c_str(),
							nullptr,
							D3D_COMPILE_STANDARD_FILE_INCLUDE,
							"main",
							"ps_5_0",
							flags,
							0,
							code.GetAddressOf(),
							error.GetAddressOf());
						if (FAILED(hr))
						{
							if (error)
								spdlog::error(
									"Shader compile error: {}",
									std::string(
										static_cast<const char*>(error->GetBufferPointer()),
										error->GetBufferSize()));
						}
						else if (code)
						{
							const auto* begin = static_cast<const uint8_t*>(code->GetBufferPointer());
							bytecode.assign(begin, begin + code->GetBufferSize());
						}
					}
				}
				catch (...)
				{
				}

				if (!bytecode.empty() && !stopToken.stop_requested())
					QueuePixelShaderHotReload(std::move(bytecode), filePath, materialGUID, active);
			}
		}
	});

	NSFXCWatchers.emplace(materialGUID, FXCWatcher_t {std::move(active), std::move(watcher)});
	return true;
}

void StopFXCAndHotReloadWatchers()
{
	std::vector<std::jthread> watcherThreads;
	{
		std::lock_guard<std::mutex> lock(NSFXCWatchersMutex);
		watcherThreads.reserve(NSFXCWatchers.size());
		for (auto& [materialGUID, watcher] : NSFXCWatchers)
		{
			NOTE_UNUSED(materialGUID);
			watcher.m_Active->store(false, std::memory_order_release);
			watcher.m_Thread.request_stop();
			watcherThreads.push_back(std::move(watcher.m_Thread));
		}
		NSFXCWatchers.clear();
	}

	// Join outside the bookkeeping lock. The stop-aware wait wakes immediately.
	watcherThreads.clear();

	std::lock_guard<std::mutex> shaderLock(NSMaterialPixelShadersMutex);
	NSMaterialPixelShaders.clear();
}

template <ScriptContext context> SQRESULT NSWatchFXCAndHotReload_SQ(HSQUIRRELVM sqvm)
{
    // 1: optional guid string
    const char* maybeGuid = nullptr;
    try
    {
        maybeGuid = g_pSquirrel[ScriptContext::CLIENT]->getstring(sqvm, 1);
    }
    catch (...)
    {
        maybeGuid = nullptr;
    }

    // Open file dialog to select fxc file
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"FXC or FX\0*.fxc;*.fx\0All\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"fxc";

    if (!GetOpenFileNameW(&ofn))
    {
        g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "File selection cancelled or failed");
        return SQRESULT_ERROR;
    }

    std::wstring filePath(szFile);

    std::string guidStr;
    if (maybeGuid && strlen(maybeGuid) > 0)
    {
        guidStr = maybeGuid;
    }
    else
    {
        MessageBoxA(NULL, "Please copy the material GUID (hex, e.g. 0xABC...) to the clipboard and press OK", "Enter Material GUID",
                    MB_OK | MB_ICONINFORMATION);
        //if (!ReadClipboardString(guidStr) || guidStr.empty())
        //{
        //    g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "No GUID found in clipboard");
        //    return SQRESULT_ERROR;
        //}
    }

    if (!isValidMaterialGUID(guidStr))
    {
        g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "Malformed Material GUID");
        return SQRESULT_ERROR;
    }

    uint64_t matGUID = std::stoull(guidStr, nullptr, 16);

	if (!WatchFXCAndHotReload(std::move(filePath), matGUID))
	{
		g_pSquirrel[ScriptContext::CLIENT]->raiseerror(sqvm, "A watcher is already running for this material GUID");
		return SQRESULT_ERROR;
	}

	return SQRESULT_NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Resolve the sampler state encoded in a material-system texture
//          handle without leaving changes in the engine's staging arrays.
//-----------------------------------------------------------------------------
static ID3D11SamplerState* ResolveTextureSampler(int16_t textureHandle)
{
	assert(s_ResolvePixelTextureAndSampler);
	assert(s_StagedPixelTextures);
	assert(s_StagedPixelSamplers);
	assert(s_StagedTextureBindingState);

	if (!s_ResolvePixelTextureAndSampler || !s_StagedPixelTextures || !s_StagedPixelSamplers
		|| !s_StagedTextureBindingState || textureHandle == 0)
	{
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(s_TextureSamplerResolveMutex);
	ID3D11ShaderResourceView* const previousTexture = s_StagedPixelTextures[0];
	ID3D11SamplerState* const previousSampler = s_StagedPixelSamplers[0];
	const uint64_t previousBindingState = *s_StagedTextureBindingState;

	s_ResolvePixelTextureAndSampler(0, textureHandle);
	ID3D11SamplerState* const sampler = s_StagedPixelSamplers[0];

	s_StagedPixelTextures[0] = previousTexture;
	s_StagedPixelSamplers[0] = previousSampler;
	*s_StagedTextureBindingState = previousBindingState;
	return sampler;
}

//-----------------------------------------------------------------------------
// Purpose: Bind one material-system texture handle and its engine-authored
//          sampler state to the requested pixel-shader register.
//-----------------------------------------------------------------------------
static void BindTextureHandleToPixelShader(uint32_t textureSlot, uint32_t samplerSlot, int16_t textureHandle)
{
	s_BindPixelTextureHandle(textureSlot, textureHandle);

	ID3D11SamplerState* sampler = ResolveTextureSampler(textureHandle);
	const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
	if (sampler && dx11)
		dx11.m_pContext->PSSetSamplers(samplerSlot, 1, &sampler);
}

//-----------------------------------------------------------------------------
// Purpose: Resolve a material-system named texture and bind its color/depth
//          handles to tN/tN+1 and its samplers to sM/sM+1.
//-----------------------------------------------------------------------------
static bool BindNamedTextureToPixelShader(uint32_t textureSlot, uint32_t samplerSlot, const char* textureName)
{
	assert(s_FindNamedTexture);
	assert(s_BindPixelTextureHandle);
	assert(textureSlot < kMaxCustomTextureBindings - 1);
	assert(samplerSlot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1);
	assert(textureName && textureName[0]);

	if (!s_FindNamedTexture || !s_BindPixelTextureHandle || textureSlot >= kMaxCustomTextureBindings - 1
		|| samplerSlot >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 || !textureName || !textureName[0])
	{
		return false;
	}

	ITextureInternal* const texture = s_FindNamedTexture(textureName);
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

DECLARE_HOOK(Water_Execute, materialsystem_dx11.dll + 0x41AC0, [](auto& hook, __int64 a1, __int64 a2, __int64 a3, __int64 a4) -> __int64
{
	NOTE_UNUSED(hook);
	NOTE_UNUSED(a1);
	NOTE_UNUSED(a2);
	NOTE_UNUSED(a3);

	const CDx11Device::Snapshot dx11 = CDx11Device::GetSnapshot();
	if (!dx11)
		return *reinterpret_cast<unsigned int*>(a4 + 8);

	static bool shadersLoaded = false;
	static ID3D11VertexShader* vertexShader = nullptr;
	static ID3D11PixelShader* pixelShader = nullptr;

	if (!shadersLoaded)
	{
		shadersLoaded = true;

		const auto readShader = [](const char* fileName, std::vector<uint8_t>& buffer) -> bool
		{
			std::ifstream file(fileName, std::ios::binary | std::ios::ate);
			if (!file)
			{
				spdlog::error("Failed to open compiled shader '{}'", fileName);
				return false;
			}

			const std::streamoff fileSize = file.tellg();
			if (fileSize <= 0)
				return false;

			buffer.resize(static_cast<size_t>(fileSize));
			file.seekg(0, std::ios::beg);
			return static_cast<bool>(
				file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize)));
		};

		std::vector<uint8_t> vertexShaderBuffer;
		std::vector<uint8_t> pixelShaderBuffer;
		if (readShader("vertex.fxc", vertexShaderBuffer) && readShader("pixel.fxc", pixelShaderBuffer))
		{
			HRESULT result = dx11.m_pDevice->CreateVertexShader(
				vertexShaderBuffer.data(), vertexShaderBuffer.size(), nullptr, &vertexShader);
			if (FAILED(result))
				spdlog::error("Failed to create water vertex shader (HRESULT 0x{:08X})", static_cast<uint32_t>(result));

			result = dx11.m_pDevice->CreatePixelShader(
				pixelShaderBuffer.data(), pixelShaderBuffer.size(), nullptr, &pixelShader);
			if (FAILED(result))
				spdlog::error("Failed to create water pixel shader (HRESULT 0x{:08X})", static_cast<uint32_t>(result));
		}
	}

	if (vertexShader && pixelShader)
	{
		dx11.m_pContext->VSSetShader(vertexShader, nullptr, 0);
		dx11.m_pContext->PSSetShader(pixelShader, nullptr, 0);
	}

	ID3D11Buffer* const* constantBuffer = reinterpret_cast<ID3D11Buffer* const*>(a4 + 16);
	dx11.m_pContext->VSSetConstantBuffers(0, 1, constantBuffer);
	dx11.m_pContext->PSSetConstantBuffers(0, 1, constantBuffer);

	assert(s_SetupWaterTextureBindings);
	if (s_SetupWaterTextureBindings)
		s_SetupWaterTextureBindings(*reinterpret_cast<__int64*>(a4 + 0x78), 14);

	return *reinterpret_cast<unsigned int*>(a4 + 8);
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
		std::lock_guard<std::mutex> lock(s_NamedTextureBindingsMutex);
		const auto mappings = s_MaterialNamedTextureSlotBindings.find(material->guid);
		if (mappings != s_MaterialNamedTextureSlotBindings.end())
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

// The normal water path reaches this function with flags produced from
// engine.dll's BSP leafwaterdata records. RPAK materials have no leaf record,
// so add the pass bits requested by their named render-target bindings just
// before the client decides which water render targets to update.
DECLARE_HOOK(UpdateWaterRenderTargets, client.dll + 0x3756A0, [](auto& hook, __int64 viewRender, uint8_t* view, void* originalView, unsigned int clearFlags, __int64 finalRenderTarget) -> __int64
{
	const uint16_t requestedPasses = s_RequestedWaterPasses.load(std::memory_order_acquire);
	if (view && requestedPasses != 0)
	{
		auto* waterDrawFlags = reinterpret_cast<uint16_t*>(view + kViewWaterDrawFlagsOffset);
		*waterDrawFlags |= requestedPasses;

		if ((requestedPasses & kDrawWaterReflection) != 0)
		{
			auto* waterPlane = reinterpret_cast<float*>(view + kViewWaterPlaneOffset);
			if (waterPlane[0] == 0.0f && waterPlane[1] == 0.0f && waterPlane[2] == 0.0f)
			{
				// A material has no geometry or transform from which to recover a
				// reflection plane. Use a valid horizontal fallback so the target
				// is populated; real BSP water keeps its authored plane above.
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

//-----------------------------------------------------------------------------
// Purpose: Bind a material-system named texture to adjacent RPAK pixel-shader
//          slots. Resolution is deferred until the material is drawn.
//-----------------------------------------------------------------------------
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

	if (textureBindingSlot < 0 || textureBindingSlot >= kMaxCustomTextureBindings - 1)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture bindings only support base shader slots 0-{} because depth uses the adjacent slot",
				kMaxCustomTextureBindings - 2)
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
		std::lock_guard<std::mutex> lock(s_NamedTextureBindingsMutex);
		auto& mappings = s_MaterialNamedTextureSlotBindings[material->guid];
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

	if (shaderBindingSlot < 0 || shaderBindingSlot >= kMaxCustomTextureBindings - 1)
	{
		g_pSquirrel[context]->raiseerror(
			sqvm,
			fmt::format(
				"Named texture bindings only support base shader slots 0-{} because depth uses the adjacent slot",
				kMaxCustomTextureBindings - 2)
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
		std::lock_guard<std::mutex> lock(s_NamedTextureBindingsMutex);
		const auto mappings = s_MaterialNamedTextureSlotBindings.find(material->guid);
		if (mappings != s_MaterialNamedTextureSlotBindings.end())
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
	s_FindNamedTexture = module.Offset(0x96F00).RCast<FindNamedTextureFn>();
	s_BindPixelTextureHandle = module.Offset(0x267D0).RCast<BindPixelTextureHandleFn>();
	s_ResolvePixelTextureAndSampler = module.Offset(0x26430).RCast<ResolvePixelTextureAndSamplerFn>();
	s_SetupWaterTextureBindings = module.Offset(0x264F0).RCast<SetupWaterTextureBindingsFn>();
	s_StagedPixelTextures = module.Offset(0x19ACA70).RCast<ID3D11ShaderResourceView**>();
	s_StagedPixelSamplers = module.Offset(0x19AC9F0).RCast<ID3D11SamplerState**>();
	s_StagedTextureBindingState = module.Offset(0x19ACB30).RCast<uint64_t*>();
	assert(s_FindNamedTexture);
	assert(s_BindPixelTextureHandle);
	assert(s_ResolvePixelTextureAndSampler);
	assert(s_SetupWaterTextureBindings);
	assert(s_StagedPixelTextures);
	assert(s_StagedPixelSamplers);
	assert(s_StagedTextureBindingState);

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
    auto clientWatchFXC = NSWatchFXCAndHotReload_SQ<ScriptContext::CLIENT>;
	g_pSquirrel[ScriptContext::CLIENT]->AddFuncRegistration("void", "NSWatchFXCAndHotReload", "string optionalMaterialGUID", "", clientWatchFXC);
})

ON_DLL_LOAD_CLIENT("client.dll", CustomWaterRenderTargets, [](CModule module)
{
	NOTE_UNUSED(module);
	DISPATCH_HOOK(NSCustomDXBufferHooks, UpdateWaterRenderTargets)
})
