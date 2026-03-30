#include "rui.h"

DECLARE_MODULE(RuiHooks)


struct struct_a4
{
    _BYTE gap0[68];
    float endTime;
    const char* statusText;
    float leftTeamScore;
    float rightTeamScore;
    _DWORD maxTeamScore;
    const char* factionImage;
    const char* friendlyPlayerCardImage;
    const char* enemyPlayerCardImage;
    _BYTE gap78[64];
    _DWORD whiteAssetHandle;
    _DWORD playerCardImageAssetHandle;
    uint64_t owordC0[2];   // was __m128
    uint64_t owordD0[2];   // was __m128
    uint64_t topColor[2];  // was __m128
    uint64_t bottomColor[2]; // was __m128
    const char* formattedTimeString;
    const char* gameModeName;
    _DWORD otherPlayerCardAssetHandle;
    _DWORD leftFillAsset;
    _DWORD rightFillAsset;
    float leftTeamScoreDiff;
    float otherTeamScoreDiff;
    _DWORD factionImageHandle;
    const char* scoreString;
    const char* rightTeamScoreString;
};
static_assert(offsetof(struct_a4, endTime) == 0x44);

typedef uint32_t assetHandle;


static __m128 xmmword_D3240 = {0.305f,0.956f,0.578f,1.f};
__m128 xmmword_D2BD0;
static __m128 ammpedColor_D2850 = {0.965f, 0.525f, 0.157f, 1.f };

using gamestate_info_ffa_t = void (__fastcall*)(RuiFunctions_t*, RuiGlobals*, RuiInstance*, struct_a4*);
gamestate_info_ffa_t pGamestateInfoFFA = nullptr;

static inline __m128 MakeColor(float r, float g, float b, float a)
{
    return _mm_set_ps(a, b, g, r);
}

// Full-saturation HSV->RGB, hue driven by time
// speed    - how fast the hue cycles (full cycle per second at 1.0)
// hueOffset - phase offset in [0,1] to shift the hue
static __m128 RainbowColor(float time, float speed = 0.5f, float hueOffset = 0.0f, float alpha = 1.0f)
{
    float hue = fmodf(time * speed + hueOffset, 1.0f);
    if (hue < 0.0f) hue += 1.0f;

    float h = hue * 6.0f;
    int   i = (int)h;
    float f = h - (float)i;
    // saturation = 1, value = 1
    float q = 1.0f - f;  // falling edge
    float t = f;          // rising edge

    float r, g, b;
    switch (i % 6)
    {
        case 0: r = 1.f; g = t;   b = 0.f; break;
        case 1: r = q;   g = 1.f; b = 0.f; break;
        case 2: r = 0.f; g = 1.f; b = t;   break;
        case 3: r = 0.f; g = q;   b = 1.f; break;
        case 4: r = t;   g = 0.f; b = 1.f; break;
        case 5: r = 1.f; g = 0.f; b = q;   break;
        default: r = g = b = 0.0f;          break;
    }

    return MakeColor(r, g, b, alpha);
}

void __fastcall gamestate_info_ffa(RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, struct_a4* a4)
{

	static __m128 xmmword_D3C20 = { 25.866, 25.866f, 40.000f, 40.000f };
	static __m128 xmmword_D3C40 = { 128.000f, 128.000f, 40.000f, 40.f };
	static __m128 xmmword_D3C50 = { 200.000f, 200.000f,40.000f,40.000f };
	static __m128 xmmword_D4A00 = { 1920.000f, 1920.000f, 1080.000f, 1080.000f };
	static __m128 xmmword_D40E0 = { 2.000f, 2.000f, 84.000f, 84.000f };
	static __m128 xmmword_D3CE0 = { 48.000f,48.000f,48.000f,48.000f };
	static __m128 enemyColor = { 1.000f, 0.188f, 0.014f, 1.f };
	static __m128 friendlyColor = { 0.095f, 0.309f, 0.708f, 1.f };


    float endTime = a4->endTime;
	uint64_t globals = (uint64_t)a2;
    float timeLeft = endTime - a2->currentTime;
	char* buffer = (char*)alloca(2048);
	strcpy(buffer, a4->statusText);
	if (strcmp(buffer,"") == 0) {
		strcpy(buffer, "#PL_ffa");
	}


    if (timeLeft < 0.0f || endTime == -1.0e30f)
    {
        a4->formattedTimeString = "--:--";
    }
    else if (timeLeft > 30.0f)
    {
        a4->formattedTimeString = a1->printf(
            a3, "%i:%02i",
            (unsigned int)((int)timeLeft / 60),
            (unsigned int)((int)timeLeft % 60)
        );
    }
    else
    {
        a4->formattedTimeString = a1->printf(a3, "%05.2f", timeLeft);
    }

    a4->rightFillAsset = a1->LoadAsset(a3, "rui/hud/gamestate/score_fill_right");
    assetHandle v9 = a1->LoadAsset(a3, "rui/hud/gamestate/score_fill_right");
    bool teamScoreDiff = a4->leftTeamScore < a4->rightTeamScore;
    a4->leftFillAsset = v9;

    __m128 topColor, bottomColor;
    const char* scoreString;
    const char* v23;
    float otherScore;

    if (teamScoreDiff)
    {
        assetHandle enemyPlayerCardImage_1 = a1->LoadAsset(a3, a4->enemyPlayerCardImage);
        const char* friendlyPlayerCardImage = a4->friendlyPlayerCardImage;
        a4->playerCardImageAssetHandle = enemyPlayerCardImage_1;
        assetHandle v26 = a1->LoadAsset(a3, friendlyPlayerCardImage);
        __m128i v27 = _mm_cvtsi32_si128(a4->maxTeamScore);
        a4->otherPlayerCardAssetHandle = v26;
        float v28 = _mm_cvtepi32_ps(v27).m128_f32[0];
        if (v28 == 0.0f) {
             return (a1->SetErrorWithReason)(
                a3,
                "content\\r2\\ui\\hud\\gamemode_ffa.rui (83,46): divide by zero.\n"
            );
		}

        float rightTeamScore = a4->rightTeamScore;
        a4->leftTeamScoreDiff = rightTeamScore / v28;
        topColor    = enemyColor;
        bottomColor = friendlyColor;

		//topColor = RainbowColor(a2->currentTime, 0.5f, 0.0f);
		//bottomColor = RainbowColor(a2->currentTime, 0.5f, 0.0f);

        float v30 = a4->leftTeamScore / v28;
        *(__m128*)a4->topColor    = topColor;
        *(__m128*)a4->bottomColor = bottomColor;
        a4->otherTeamScoreDiff = v30;
        scoreString = a1->printf(a3, "%.8g", rightTeamScore);
        otherScore  = a4->leftTeamScore;
        v23 = "%.8g";
    }
    else
    {
        assetHandle v11 = a1->LoadAsset(a3, a4->friendlyPlayerCardImage);
        const char* enemyPlayerCardImage = a4->enemyPlayerCardImage;
        a4->playerCardImageAssetHandle = v11;
        assetHandle enemyPlayerCardImageAssetHandle_2 = a1->LoadAsset(a3, enemyPlayerCardImage);
        __m128i maxTeamScore = _mm_cvtsi32_si128(a4->maxTeamScore);
        a4->otherPlayerCardAssetHandle = enemyPlayerCardImageAssetHandle_2;
        float maxTeamScore_1 = _mm_cvtepi32_ps(maxTeamScore).m128_f32[0];
        if (maxTeamScore_1 == 0.0f) {
            return (a1->SetErrorWithReason)(
                a3,
                "content\\r2\\ui\\hud\\gamemode_ffa.rui (83,46): divide by zero.\n"
            );
		}

        float leftTeamScore_1 = a4->leftTeamScore;
        a4->leftTeamScoreDiff = leftTeamScore_1 / maxTeamScore_1;
        topColor    = friendlyColor;
        bottomColor = enemyColor;

        float v20 = a4->rightTeamScore / maxTeamScore_1;
        *(__m128*)a4->topColor    = topColor;
        *(__m128*)a4->bottomColor = bottomColor;
        a4->otherTeamScoreDiff = v20;
        scoreString = a1->printf(a3, "%.8g", leftTeamScore_1);
        otherScore  = a4->rightTeamScore;
        v23 = "%.8g";
    }

    a4->scoreString = scoreString;
    const char* v31 = a1->printf(a3, v23, otherScore);
    *(__m128*)a4->owordD0 = bottomColor;
    *(__m128*)a4->owordC0 = topColor;
    a4->rightTeamScoreString = v31;
    a4->whiteAssetHandle = a1->LoadAsset(a3, "white");
    const char* factionImage = a4->factionImage;
	a4->gameModeName    = a1->localize(a3, buffer);
    a4->factionImageHandle = a1->LoadAsset(a3, factionImage);

    __m128* transformSizes = a1->GetTransformSize(a3);
    transformSizes[3] = (__m128)xmmword_D3C50;
    a1->executeTransform(a3, 1);
    transformSizes[4] = (__m128)xmmword_D3C50;
    a1->executeTransform(a3, 2);
    transformSizes[5]  = (__m128)xmmword_D4A00;
    transformSizes[6]  = (__m128)xmmword_D40E0;

    __m128 v35;
    v35.m128_u64[1] = 0x4220000042200000ULL;
    transformSizes[7]  = (__m128)xmmword_D4A00;
    transformSizes[9]  = (__m128)xmmword_D3C20;
    transformSizes[8]  = (__m128)xmmword_D3C40;
    transformSizes[10] = (__m128)xmmword_D3C20;

    *(double*)v35.m128_u64 = (a1->unknown_5)(a3, 3LL, 4LL);
    transformSizes[11] = v35;
    *(double*)v35.m128_u64 = (a1->unknown_5)(a3, 4LL, 5LL);
    transformSizes[12] = v35;
    transformSizes[13] = (a1->GetTextSize)(a3, 42LL);
    transformSizes[14] = (a1->GetTextSize)(a3, 60LL);
    transformSizes[15] = (a1->GetTextSize)(a3, 78LL);
    transformSizes[16] = (a1->GetTextSize)(a3, 348LL);
    transformSizes[17] = (a1->GetTextSize)(a3, 366LL);
    transformSizes[18] = xmmword_D3CE0;

    return (a1->executeTransform)(a3, 0x9ELL);
}

struct crosshair_plus_struct
{
  _BYTE gap0[12];
  float adjustedSpread;
  float adsFrac;
  _DWORD isSprinting;
  _DWORD isReloading;
  _DWORD isGrappleInRange;
  float teamColor[3];
  float crosshairMovementX;
  float crosshairMovementY;
  _DWORD isAmped;
  _QWORD qword38;
  _QWORD qword40;
  _QWORD qword48;
  _QWORD qword50;
  _QWORD qword58;
  _QWORD qword60;
  _QWORD qword68;
  _QWORD qword70;
  _QWORD qword78;
  _QWORD qword80;
  _QWORD qword88;
  _QWORD qword90;
  _QWORD qword98;
  _QWORD qwordA0;
  _QWORD qwordA8;
  _QWORD qwordB0;
  _DWORD dwordB8;
  __m128 m128C0;
};


static ConVar* Cvar_ion_use_custom_crosshair;
static ConVar* Cvar_ion_crosshair_gap_v;
static ConVar* Cvar_ion_crosshair_length_v;
static ConVar* Cvar_ion_crosshair_inset_v;
static ConVar* Cvar_ion_crosshair_thickness_l;
static ConVar* Cvar_ion_crosshair_thickness_r;
static ConVar* Cvar_ion_crosshair_gap_h;
static ConVar* Cvar_ion_crosshair_length_h;

void __fastcall crosshair_plus(RuiFunctions_t *a1, RuiGlobals *a2, RuiInstance *a3, crosshair_plus_struct *a4)
{
    if (a2->dword_B0)
    {
        a1->setNoRender(a3);
        return;
    }

    float spreadScaled = a4->adjustedSpread * 540.0f;
    float spreadY      = spreadScaled / 1080.0f;
    float spreadX      = spreadScaled / 1920.0f;

    float alphaFrac = (0.75f - a4->adsFrac) - (a4->isSprinting || a4->isReloading ? 0.55000001f : 0.0f);

    if (a4->isGrappleInRange)
    {
        a4->m128C0 = xmmword_D3240;
    }
    else
    {
        __m128 baseColor = a4->isAmped ? ammpedColor_D2850 : _mm_and_ps(*(__m128*)(a4->teamColor), xmmword_D2BD0);
        __m128 alphaVec;
        alphaVec.m128_u32[0] = 0x3F400000u; // 0.75f
        alphaVec.m128_f32[0] = alphaFrac;
        a4->m128C0 = _mm_shuffle_ps(baseColor, _mm_shuffle_ps(alphaVec, baseColor, 228), 36);
    }

    a4->dwordB8 = a1->LoadAsset(a3, "white");

    __m128 movX = _mm_set_ss(a4->crosshairMovementX);
    __m128 movY = _mm_set_ss(a4->crosshairMovementY);
    __m128 movXY = _mm_unpacklo_ps(movX, movY);

    // Packs (movX + offX, movY + offY) into a uint64 for qword fields
    // offX and offY are passed as __m128 scalars (raw hex bits preserved)
    auto PackXY = [&](uint32_t rawOffX, uint32_t rawOffY) -> uint64_t
    {
        __m128 offX, offY;
        offX.m128_u32[0] = rawOffX;
        offY.m128_u32[0] = rawOffY;
        return _mm_add_ps(movXY, _mm_unpacklo_ps(offX, offY)).m128_u64[0];
    };

    // Encode spread-adjusted Y offsets as raw bits for PackXY
    auto FloatBits = [](float f) -> uint32_t {
        uint32_t bits;
        memcpy(&bits, &f, sizeof(bits));
        return bits;
    };


	float kGapV        = Cvar_ion_crosshair_gap_v->GetFloat();
	float kLengthV     = Cvar_ion_crosshair_length_v->GetFloat();
	float kInsetV      = Cvar_ion_crosshair_inset_v->GetFloat();
	float kThicknessVL = Cvar_ion_crosshair_thickness_l->GetFloat();
	float kThicknessVR = Cvar_ion_crosshair_thickness_r->GetFloat();
	float kGapH        = Cvar_ion_crosshair_gap_h->GetFloat();
	float kLengthH     = Cvar_ion_crosshair_length_h->GetFloat();
	float kInsetH      = kLengthH * 0.15f;

	const float Vx_left   = 0.5f - kThicknessVL;
	const float Vx_right  = 0.5f + kThicknessVR;
	const float Vx_left2  = 0.5f - (kThicknessVL * 0.5f);
	const float Vx_right2 = 0.5f + (kThicknessVR * 0.5f);

	const float Vtop_near = 0.5f - kGapV;
	const float Vtop_far  = Vtop_near - kLengthV;
	const float Vbot_near = 0.5f + kGapV;
	const float Vbot_far  = Vbot_near + kLengthV;

	const float Hleft_far   = 0.5f - kGapH - kLengthH;
	const float Hleft_near  = 0.5f - kGapH;
	const float Hright_near = 0.5f + kGapH;
	const float Hright_far  = 0.5f + kGapH + kLengthH;



	// Vertical top
	a4->qword38 = PackXY(0x3EFEEEEFu, FloatBits(Vtop_far  - spreadY));
	a4->qword40 = PackXY(0x3F008889u, FloatBits(Vtop_near - spreadY));
	a4->qword48 = PackXY(0x3EFF7777u, FloatBits(Vtop_far  + kInsetV - spreadY));
	a4->qword50 = PackXY(0x3F004444u, FloatBits(Vtop_near - kInsetV - spreadY));

	// Vertical bottom
	a4->qword58 = PackXY(0x3EFEEEEFu, FloatBits(Vbot_near + spreadY));
	a4->qword60 = PackXY(0x3F008889u, FloatBits(Vbot_far  + spreadY));
	a4->qword68 = PackXY(0x3EFF7777u, FloatBits(Vbot_near + kInsetV + spreadY));
	a4->qword70 = PackXY(0x3F004444u, FloatBits(Vbot_far  - kInsetV + spreadY));

	// Horizontal left
	a4->qword78 = PackXY(FloatBits(Hleft_far  - spreadX), 0x3EFE1A8Cu);
	a4->qword80 = PackXY(FloatBits(Hleft_near - spreadX), 0x3F00F2BAu);
	a4->qword88 = PackXY(FloatBits(Hleft_far  + kInsetH - spreadX), 0x3EFF0D46u);
	a4->qword90 = PackXY(FloatBits(Hleft_near - kInsetH - spreadX), 0x3F00795Du);

	// Horizontal right
	a4->qword98 = PackXY(FloatBits(Hright_near + spreadX), 0x3EFE1A8Cu);
	a4->qwordA0 = PackXY(FloatBits(Hright_far  + spreadX), 0x3F00F2BAu);
	a4->qwordA8 = PackXY(FloatBits(Hright_near + kInsetH + spreadX), 0x3EFF0D46u);
	a4->qwordB0 = PackXY(FloatBits(Hright_far  - kInsetH + spreadX), 0x3F00795Du);

    a1->executeTransform(a3, 162);
}



using gamestate_info_ffa_t = void(__fastcall*)(RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, struct_a4* a4);
gamestate_info_ffa_t pGamestate_info_ffa = nullptr;

HOOK(v_hk_gamestate_info_ffa, o_hk_gamestate_info_ffa, void, __fastcall, (RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, struct_a4* a4))
{
	gamestate_info_ffa(a1, a2, a3, a4);
}

struct gamestate_info_struct
{
  float xPos;
  float yPos;
  float zPos;
  float leftTextPos;
  float rightTextPos;
  float statusYPos;
  float timerSize;
  float shadowAmmount;
  float textFillAmmountMaybe;
  float unk;
  float leftColor[3];
  float rightColor[3];
  float textSize;
  int64_t unk2;
  float gameTime;
  const char *statusText;
  float leftTeamScore;
  float rightTeamScore;
  _DWORD maxTeamScore;
  int maxTeamPlayers;
  _BYTE gap68[8];
};


DECLARE_HOOK(gamestate_info, ui(11).dll + 0x33AE0, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, gamestate_info_struct* a4)
{
	//auto color = RainbowColor(a2->currentTime, 0.5f, 0.0f, 1.0f);
	//a4->leftColor[0] = color.m128_f32[0];
	//a4->leftColor[1] = color.m128_f32[1];
	//a4->leftColor[2] = color.m128_f32[2];

	//color = RainbowColor(a2->currentTime, 0.5f, 0.5f, 1.0f);
	//a4->rightColor[0] = color.m128_f32[0];
	//a4->rightColor[1] = color.m128_f32[1];
	//a4->rightColor[2] = color.m128_f32[2];
		
	hook.Original(a1, a2, a3, a4);
});

struct gamestate_info_ps_struct
{

	float xpos; //0x0000
	float yPos; //0x0004
	float zPos; //0x0008
	float leftTextPos; //0x000C
	float rightTextPos; //0x0010
	float timerScale; //0x0014
	float shadowAmmount; //0x0018
	float textFill; //0x001C
	float unk1; //0x0020
	Vector3 leftColor; //0x0024
	Vector3 rightColor; //0x0030
	float textSize; //0x003C
	int64_t unk2; //0x0040
	int64_t unk3; //0x0048
	float gameEndTime;
	float leftTeamScore;
	float rightTeamScore;
	_DWORD maxTeamScore;
	int maxTeamPlayers;
	const char *factionImage;
	_BYTE gap70[60];
	_DWORD factionImageHandle;
	_QWORD formattedTimeString;
	_DWORD leftScoreBarAsset;
	_DWORD fillLeftAsset;
	float floatC0;
	_DWORD barRightAsset;
	_DWORD rightFillAsset;
	float floatCC;
	_QWORD leftTeamScoreText;
	_QWORD rightTeamScoreText;
};


DECLARE_HOOK(gamestate_info_ps, ui(11).dll + 0x46280, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, gamestate_info_ps_struct* a4)
{
	//func(a1,a2,a3,(targetinfo_pilot_struct*)a4);
	//auto color = RainbowColor(a2->currentTime, 0.5f, 0.0f, 1.0f);
	//a4->leftColor.x = color.m128_f32[0];
	//a4->leftColor.y = color.m128_f32[1];
	//a4->leftColor.z = color.m128_f32[2];

	//color = RainbowColor(a2->currentTime, 0.5f, 0.5f, 1.0f);
	//a4->rightColor.x = color.m128_f32[0];
	//a4->rightColor.y = color.m128_f32[1];
	//a4->rightColor.z = color.m128_f32[2];
	hook.Original(a1, a2, a3, a4);
});


DECLARE_HOOK(hk_crosshair_plus, ui(11).dll + 0x1D560, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, crosshair_plus_struct* a4)
{
	if (Cvar_ion_use_custom_crosshair->GetBool()) {
		return crosshair_plus(a1, a2, a3, a4);
	}
	hook.Original(a1, a2, a3, a4);
})


ON_DLL_LOAD("ui(11).dll", RuiStuff, [](CModule module)
{

	pGamestate_info_ffa = module.Offset(0x3E8E0).RCast<gamestate_info_ffa_t>();
	v_hk_gamestate_info_ffa.Dispatch(reinterpret_cast<void*>(pGamestate_info_ffa));

	xmmword_D2BD0.m128_u64[0] = 0xFFFFFFFFFFFFFFFFULL;
	xmmword_D2BD0.m128_u64[1] = 0xFFFFFFFFULL;
	Cvar_ion_use_custom_crosshair = new ConVar("ion_use_custom_crosshair", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Use custom crosshairs. 1 = enabled, 0 = disabled.");
	Cvar_ion_crosshair_gap_v       = new ConVar("ion_crosshair_gap_v",        "0.0074074", FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm gap from center.");
	Cvar_ion_crosshair_length_v    = new ConVar("ion_crosshair_length_v",      "0.01200",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm segment length.");
	Cvar_ion_crosshair_inset_v     = new ConVar("ion_crosshair_inset_v",       "0.00100",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm inner shadow inset.");
	Cvar_ion_crosshair_thickness_l = new ConVar("ion_crosshair_thickness_l",   "0.00185",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm thickness left of center.");
	Cvar_ion_crosshair_thickness_r = new ConVar("ion_crosshair_thickness_r",   "0.00833",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm thickness right of center.");
	Cvar_ion_crosshair_gap_h       = new ConVar("ion_crosshair_gap_h",         "0.00417",   FCVAR_ARCHIVE_PLAYERPROFILE, "Horizontal arm gap from center.");
	Cvar_ion_crosshair_length_h    = new ConVar("ion_crosshair_length_h",       "0.00700",   FCVAR_ARCHIVE_PLAYERPROFILE, "Horizontal arm segment length.");
	DISPATCH_MODULE(RuiHooks);
})
