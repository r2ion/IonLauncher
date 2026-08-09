#include "materialsystem/cmaterialglue.h"
#include "rendersystem/schema/texture.g.h"

DECLARE_MODULE(GlMatSysIFaceHooks)

CMaterialGlue* (*GetMaterialAtCrossHair)();

DECLARE_HOOK(CC_mat_crosshair_printmaterial_f, engine.dll + 0xB3C40, [](auto& hook, const CCommand& args)
{
	NOTE_UNUSED(hook);
	NOTE_UNUSED(args);
	CMaterialGlue* pMat = GetMaterialAtCrossHair();

	if (!pMat)
	{
		spdlog::error("Not looking at a material!");
		return;
	}

	std::function<void(CMaterialGlue * pGlue, const char* szName)> fnPrintGlue = [](CMaterialGlue* pGlue, const char* szName)
	{

		if (!pGlue)
		{
			spdlog::info("├ No reference material for {}", szName);
			return;
		}

		spdlog::info("├ {}", szName);
		spdlog::info("│├── GUID: {:#x}", pGlue->guid);
		spdlog::info("│└── Name: {}", pGlue->name);

	};
	spdlog::info("────────────────────────────────────────────────────────────");

	spdlog::info("┌ Name: {}", pMat->name);
	spdlog::info("├ GUID: {:#x}", pMat->guid);
	spdlog::info("├ Width : {}", pMat->width);
	spdlog::info("├ Height: {}", pMat->height);
	spdlog::info("├ Shaderset: {}", pMat->shaderSet->name);

	fnPrintGlue(pMat->DepthShadow_ref, "DepthShadow");
	fnPrintGlue(pMat->DepthPrepass_ref, "DepthPrepass");
	fnPrintGlue(pMat->DepthVSM_ref, "DepthVSM");
	fnPrintGlue(pMat->Colpass_ref, "Colpass");

	if (pMat->shaderSet && pMat->shaderSet->textureInputCount >= 1 && pMat->textureHandles)
	{
		spdlog::info("├ Textures");
		for (size_t slot = 0; slot < pMat->shaderSet->textureInputCount +1; ++slot)
		{
			TextureAsset_s* currentTexture = pMat->textureHandles[slot];
	
			if (currentTexture && currentTexture->debugName)
			{
				if(slot == pMat->shaderSet->textureInputCount)
					spdlog::info("│└[{}][{:#x}]{}", slot, currentTexture->assetGuid, currentTexture->debugName);
				else
					spdlog::info("│├[{}][{:#x}]{}", slot, currentTexture->assetGuid, currentTexture->debugName);
			}
		}
	}
	spdlog::info("├ Glueflags: {:#x}", pMat->flags);
	spdlog::info("└ Glueflags2: {:#x}", pMat->flags2);
	spdlog::info("────────────────────────────────────────────────────────────");

})

ON_DLL_LOAD("engine.dll", GlMatSysIFace, [](CModule module)
{
	DISPATCH_MODULE(GlMatSysIFaceHooks)
	GetMaterialAtCrossHair = module.Offset(0xB37D0).RCast<CMaterialGlue* (*)()>();
})
