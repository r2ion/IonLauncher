#include "vgui/basemodui.h"
#include "core/tier0.h"
#include "server/auth/serverauthentication.h"

AUTOHOOK_INIT()

LoadingProgressDescription_t g_ListenServerLoadingProgressDescriptions[] = {
	{PROGRESS_NONE, 0, 0, NULL},
	{PROGRESS_SPAWNSERVER, 5, 0, "#LoadingProgress_SpawningServer"},
	{PROGRESS_LOADWORLDMODEL, 8, 5, "#LoadingProgress_LoadMap"},
	{PROGRESS_CREATENETWORKSTRINGTABLES, 12, 0, NULL},
	{PROGRESS_PRECACHEWORLD, 15, 0, "#LoadingProgress_PrecacheWorld"},
	{PROGRESS_CLEARWORLD, 16, 20, NULL},
	{PROGRESS_LEVELINIT, 20, 200, "#LoadingProgress_LoadResources"},
	{PROGRESS_ACTIVATESERVER, 50, 0, NULL},
	{PROGRESS_SIGNONCHALLENGE, 51, 0, "#LoadingProgress_Connecting"},
	{PROGRESS_SIGNONCONNECT, 55, 0, NULL},
	{PROGRESS_SIGNONCONNECTED, 56, 1, "#LoadingProgress_SignonLocal"},
	{PROGRESS_PROCESSSERVERINFO, 58, 0, NULL},
	{PROGRESS_PROCESSSTRINGTABLE, 60, 3, NULL}, // 16
	{PROGRESS_SIGNONNEW, 63, 200, NULL},
	{PROGRESS_SENDCLIENTINFO, 80, 1, NULL},
	{PROGRESS_SENDSIGNONDATA, 81, 1, "#LoadingProgress_SignonDataLocal"},
	{PROGRESS_SIGNONSPAWN, 83, 10, NULL},
	{PROGRESS_CREATEENTITIES, 85, 3, NULL},
	{PROGRESS_FULLYCONNECTED, 86, 0, NULL},
	{PROGRESS_PRECACHELIGHTING, 87, 50, NULL},
	{PROGRESS_READYTOPLAY, 95, 100, NULL},
	{PROGRESS_HIGHESTITEM, 100, 0, NULL},
};

LoadingProgressDescription_t g_RemoteConnectLoadingProgressDescriptions[] = {
	{PROGRESS_NONE, 0, 0, NULL},
	{PROGRESS_CHANGELEVEL, 1, 0, "#LoadingProgress_Changelevel"},
	{PROGRESS_BEGINCONNECT, 5, 0, "#LoadingProgress_BeginConnect"},
	{PROGRESS_SIGNONCHALLENGE, 10, 0, "#LoadingProgress_Connecting"},
	{PROGRESS_SIGNONCONNECTED, 11, 0, NULL},
	{PROGRESS_PROCESSSERVERINFO, 12, 0, "#LoadingProgress_ProcessServerInfo"},
	{PROGRESS_PROCESSSTRINGTABLE, 15, 3, NULL},
	{PROGRESS_LOADWORLDMODEL, 20, 14, "#LoadingProgress_LoadMap"},
	{PROGRESS_SIGNONNEW, 30, 200, "#LoadingProgress_PrecacheWorld"},
	{PROGRESS_SENDCLIENTINFO, 60, 1, "#LoadingProgress_SendClientInfo"},
	{PROGRESS_SENDSIGNONDATA, 64, 1, "#LoadingProgress_SignonData"},
	{PROGRESS_SIGNONSPAWN, 65, 10, NULL},
	{PROGRESS_CREATEENTITIES, 85, 3, NULL},
	{PROGRESS_FULLYCONNECTED, 86, 0, NULL},
	{PROGRESS_PRECACHELIGHTING, 87, 50, NULL},
	{PROGRESS_READYTOPLAY, 95, 100, NULL},
	{PROGRESS_HIGHESTITEM, 100, 0, NULL},
};

static LoadingProgressDescription_t* g_pLoadingProgressDescriptions = g_RemoteConnectLoadingProgressDescriptions;
float g_flLastUpdateTime = 0.0f;
int m_nLastProgressPointRepeatCount;
LevelLoadingProgress_e m_eLastProgressPoint;
float flPerc = 0.0;

LoadingProgressDescription_t* GetProgressDescription(LevelLoadingProgress_e eProgress)
{
	LevelLoadingProgress_e v1; // ecx
	int v2; // eax

	v1 = g_pLoadingProgressDescriptions->eProgress;
	v2 = 0;
	if (g_pLoadingProgressDescriptions->eProgress >= eProgress)
		return &g_pLoadingProgressDescriptions[v2];
	while (v1 != PROGRESS_HIGHESTITEM)
	{
		v1 = g_pLoadingProgressDescriptions[++v2].eProgress;
		if (v1 >= eProgress)
			return &g_pLoadingProgressDescriptions[v2];
	}
	return g_pLoadingProgressDescriptions;
}

// clang-format off
AUTOHOOK(BaseModUI__LoadingProgress__PaintBackground, client.dll + 0x4CEB20,
void, __fastcall, (__int64 thisptr))
// clang-format on
{
	vgui::Panel* loadingRes = reinterpret_cast<vgui::Panel*>(thisptr);

	if(!loadingRes)
		return BaseModUI__LoadingProgress__PaintBackground(thisptr);

	vgui::ContinuousProgressBar* loadingBar = reinterpret_cast<vgui::ContinuousProgressBar*>(loadingRes->FindChildByName("LoadingProgressBar", false));

	if (loadingBar)
	{
		double t = Plat_FloatTime();
		float dt = t - g_flLastUpdateTime;

		if (flPerc == 1.0)
			*(float*)(loadingBar + 620) = 1.0;
		else
		{
			float currentProgress = *(float*)(loadingBar + 620);
			float maxStep = dt * 0.5f; // Maximum step size proportional to delta time
			float diff = flPerc - currentProgress;

			if (std::abs(diff) > maxStep)
			{
				currentProgress += (diff > 0 ? maxStep : -maxStep); // Move by maxStep towards flPerc
			}
			else
			{
				currentProgress = flPerc; // Snap to target if within maxStep
			}

			*(float*)(loadingBar + 620) = currentProgress;
		}
	}

	BaseModUI__LoadingProgress__PaintBackground(thisptr);
};

void WaitForFadeout()
{
	int waitTime = g_pCVar->FindVar("ui_loadingscreen_fadeout_time")->GetInt();
	std::this_thread::sleep_for(std::chrono::seconds(waitTime + 1));

	vgui::Panel* basemodpanel = reinterpret_cast<vgui::Panel*>(BaseModUI::CBaseModPanel::GetSingleton());
	vgui::Panel* loadingRes = basemodpanel->FindChildByName("LoadingProgress", false);

	if(!loadingRes)
		return;

	vgui::ContinuousProgressBar* loadingBar = reinterpret_cast<vgui::ContinuousProgressBar*>(loadingRes->FindChildByName("LoadingProgressBar", false));

	if(loadingBar)
		*(float*)(loadingBar + 620) = 0.0;
	flPerc = 0;
	m_eLastProgressPoint = PROGRESS_NONE;
	m_nLastProgressPointRepeatCount = 0;
	g_flLastUpdateTime = 0;
}

// clang-format off
AUTOHOOK(FadeOutFunc, client.dll + 0x76FE10,
void, __fastcall, (__int64 thisptr, int a2))
// clang-format on
{
	FadeOutFunc(thisptr, a2);

	vgui::Panel* basemodpanel = reinterpret_cast<vgui::Panel*>(BaseModUI::CBaseModPanel::GetSingleton());
	vgui::Panel* loadingRes = basemodpanel->FindChildByName("LoadingProgress", false);

	if(!loadingRes)
		return;

	vgui::ContinuousProgressBar* loadingBar = reinterpret_cast<vgui::ContinuousProgressBar*>(loadingRes->FindChildByName("LoadingProgressBar", false));

	if(!loadingBar)
		return;

	if (loadingBar && flPerc == 1.0)
	{
		FadeOutFunc((__int64)loadingBar, a2);
		std::thread t1(WaitForFadeout);
		t1.detach();
	}

	vgui::Label* loadingText = reinterpret_cast<vgui::Label*>(loadingRes->FindChildByName("LoadingLabelInfo", false));

	if(!loadingText)
		return;

	if (loadingText && flPerc == 1.0)
	{
		FadeOutFunc((__int64)loadingText, a2);
	}
}

// clang-format off
AUTOHOOK(CEngineVGui__UpdateProgressBar, engine.dll + 0x249ED0,
__int64, __fastcall, (__int64 a1, LevelLoadingProgress_e progress))
// clang-format on
{
	// don't go backwards
	if (progress < m_eLastProgressPoint)
		return CEngineVGui__UpdateProgressBar(a1, progress);

	if (g_pServerAuthentication->m_bStartingLocalSPGame)
		g_pLoadingProgressDescriptions = g_ListenServerLoadingProgressDescriptions;
	else
		g_pLoadingProgressDescriptions = g_RemoteConnectLoadingProgressDescriptions;

	bool bNewCheckpoint = progress != m_eLastProgressPoint;

	// Early time-based throttle for UpdateProgressBar
	double t = Plat_FloatTime();
	float dt = t - g_flLastUpdateTime;
	if ((!bNewCheckpoint && (dt < 0.050000001)))
	{
		return CEngineVGui__UpdateProgressBar(a1, progress);
	}

	// count progress repeats
	if (!bNewCheckpoint)
	{
	}
	else
	{
		m_nLastProgressPointRepeatCount = 0;
	}

	// construct a string describing it
	LoadingProgressDescription_t* desc = GetProgressDescription(progress);

	flPerc = desc->nPercent / 100.0f;
	if (desc->nRepeat > 1 && m_nLastProgressPointRepeatCount)
	{
		m_nLastProgressPointRepeatCount = std::min(m_nLastProgressPointRepeatCount, desc->nRepeat);
		float flNextPerc = GetProgressDescription((LevelLoadingProgress_e)((int)progress + 1))->nPercent / 100.0f;
		flPerc += (flNextPerc - flPerc) * ((float)m_nLastProgressPointRepeatCount / desc->nRepeat);
	}

	m_eLastProgressPoint = progress;
	g_flLastUpdateTime = Plat_FloatTime();

	vgui::Panel* basemodpanel = reinterpret_cast<vgui::Panel*>(BaseModUI::CBaseModPanel::GetSingleton());
	vgui::Panel* loadingRes = basemodpanel->FindChildByName("LoadingProgress", false);

	if(!loadingRes)
		return CEngineVGui__UpdateProgressBar(a1, progress);

	vgui::Label* loadingText = reinterpret_cast<vgui::Label*>(loadingRes->FindChildByName("LoadingLabelInfo", false));

	if(!loadingText)
		return CEngineVGui__UpdateProgressBar(a1, progress);

	if (loadingText && desc->pszDesc)
		vgui_Label_SetText(loadingText, desc->pszDesc);

	return CEngineVGui__UpdateProgressBar(a1, progress);
}

typedef BaseModUI::CBaseModPanel* (*BaseModPanel_GetSingletonType)();
BaseModPanel_GetSingletonType BaseModPanel_GetSingleton;

namespace BaseModUI
{
	CBaseModPanel* CBaseModPanel::GetSingleton() { return BaseModPanel_GetSingleton(); }
}


ON_DLL_LOAD_CLIENT_RELIESON("client.dll", BaseModUIHooks, ConVar, (CModule module))
{
	BaseModPanel_GetSingleton = module.Offset(0x4B1690).RCast<BaseModPanel_GetSingletonType>();

	AUTOHOOK_DISPATCH()
}
