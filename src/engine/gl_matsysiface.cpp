#include "materialsystem/cmaterialglue.h"

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
		spdlog::info("|-----------------------------------------------");

		if (!pGlue)
		{
			spdlog::info("|-- {} is NULL", szName);
			return;
		}

		spdlog::info("|-- Name: {}", szName);
		spdlog::info("|-- GUID: {:#x}", pGlue->m_GUID);
		spdlog::info("|-- Name: {}", pGlue->m_pszName);
		spdlog::info("|-- Width : {}", pGlue->m_iWidth);
		spdlog::info("|-- Height: {}", pGlue->m_iHeight);
	};

	spdlog::info("|- GUID: {:#x}", pMat->m_GUID);
	spdlog::info("|- Name: {}", pMat->m_pszName);
	spdlog::info("|- Width : {}", pMat->m_iWidth);
	spdlog::info("|- Height: {}", pMat->m_iHeight);

	fnPrintGlue(pMat->m_pDepthShadow, "DepthShadow");
	fnPrintGlue(pMat->m_pDepthPrepass, "DepthPrepass");
	fnPrintGlue(pMat->m_pDepthVSM, "DepthVSM");
	fnPrintGlue(pMat->m_pColPass, "ColPass");
})

ON_DLL_LOAD("engine.dll", GlMatSysIFace, [](CModule module)
{
	DISPATCH_MODULE(GlMatSysIFaceHooks)
	GetMaterialAtCrossHair = module.Offset(0xB37D0).RCast<CMaterialGlue* (*)()>();
})
