#pragma once

class CUserMessageManager;

extern CUserMessageManager* g_pUserMessageManager;

enum UserMessageType
{
	Geiger = 0,
	Train,
	HudText,
	SayText,
	AnnounceText,
	TextMsg,
	HudMsg,
	ResetHUD,
	GameTitle,
	ItemPickup,
	ShowMenu,
	Shake,
	Tilt,
	Fade,
	VGUIMenu,
	Rumble,
	Damage,
	VoiceMask,
	RequestState,
	CloseCaption,
	CloseCaptionDirect,
	WeapProjFireCB,
	PredSVEvent,
	CreditsMsg,
	LogoTimeMsg,
	AchievementEvent,
	UpdateJalopyRadar,
	CurrentTimescale,
	DesiredTimescale,
	CreditsPortalMsg,
	InventoryFlash,
	IndicatorFlash,
	ControlHelperAnimate,
	TakePhoto,
	Flash,
	HudPingIndicator,
	OpenRadialMenu,
	AddLocator,
	MPMapCompleted,
	MPMapIncomplete,
	MPMapCompletedData,
	MPTauntEarned,
	MPTauntUnlocked,
	MPTauntLocked,
	MPAllTauntsLocked,
	PortalFX_Surface,
	ChangePaintColor,
	StartSurvey,
	ApplyHitBoxDamageEffect,
	SetMixLayerTriggerFactor,
	TransitionFade,
	HudElemSetVisibility,
	HudElemLabelSetText,
	HudElemImageSet,
	HudElemSetColor,
	HudElemSetColorBG,
	RemoteFunctionCall,
	RemoteFunctionCallChecksum,
	PlayerNotifyDidDamage,
	RemoteBulletFired,
	RemoteWeaponReload,
};

class CUserMessageManager
{
private:
	std::vector<std::pair<const char*, unsigned int>> m_UserMessages;

public:
	void Register(const char* pszName, unsigned int uiSize);
	void RegisterUserMessages();
	void HookMessage(const char* pszName, void* pCallback);
};
