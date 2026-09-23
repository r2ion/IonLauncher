//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: User-message registration and client dispatch.
//
//=============================================================================//

#ifndef USERMESSAGES_H
#define USERMESSAGES_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/bitbuf.h"
#include "tier1/utldict.h"
#include "tier1/utlvector.h"

enum UserMessages_t
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

typedef void (*pfnUserMsgHook)(bf_read& msg);

class CUserMessage
{
  public:
    int size;
    const char* name;

    CUtlVector<pfnUserMsgHook> clienthooks;
};

class CUserMessages
{
  public:
    CUserMessages();
    ~CUserMessages();

    int LookupUserMessage(const char* name);
    int GetUserMessageSize(int index);
    const char* GetUserMessageName(int index);
    bool IsValidIndex(int index);

    void Register(const char* name, int size);

    void HookMessage(const char* name, pfnUserMsgHook hook);
    bool DispatchUserMessage(int msgType, bf_read& msgData);

  private:
    CUtlDict<CUserMessage*, int> m_UserMessages;
};

extern CUserMessages* usermessages;

#endif // USERMESSAGES_H
