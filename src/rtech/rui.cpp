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

static __m128 xmmword_D3130 = {1.f, 0.216f, 0.051f, 1.f};
static __m128 xmmword_D3120 = {1.f, 0.065f, 0.051f, 1.f};
static __m128 xmmword_D32F0 = {0.242f, 0.831f, 1.f, 1.f};

static __m128 xmmword_D3790 = {
	8.f,
	8.f,
	8.f,
	8.f,
};

static __m128 xmmword_D3360 = {12.000f,1.f,1.f,1.f};
static __m128 xmmword_D3340 = {5.f,1.f,1.f,1.f};
static __m128 xmmword_D2DE0 = {6.f,0.0f,0.f,0.5f};
static __m128 xmmword_D2FD0 = {3.f,0.f,0.f,1.f};

static __m128 enemyColor = {1.000f, 0.188f, 0.014f, 1.f};
static __m128 friendlyColor = {0.095f, 0.309f, 0.708f, 1.f};

using gamestate_info_ffa_t = void (__fastcall*)(RuiFunctions_t*, RuiGlobals*, RuiInstance*, struct_a4*);
gamestate_info_ffa_t pGamestateInfoFFA = nullptr;


static ConVar* Cvar_ion_use_custom_crosshair;
static ConVar* Cvar_ion_crosshair_gap_v;
static ConVar* Cvar_ion_crosshair_length_v;
static ConVar* Cvar_ion_crosshair_inset_v;
static ConVar* Cvar_ion_crosshair_thickness_l;
static ConVar* Cvar_ion_crosshair_thickness_r;
static ConVar* Cvar_ion_crosshair_gap_h;
static ConVar* Cvar_ion_crosshair_length_h;
static ConVar* Cvar_ion_chroma_gameinfo;
static ConVar* Cvar_ion_speedometer_always_show;
static ConVar* Cvar_gauntlet_timer_max_speed_metric;
static ConVar* Cvar_gauntlet_timer_max_speed_imperial;
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
		if (Cvar_ion_chroma_gameinfo->GetBool())
		{
			topColor = RainbowColor(a2->currentTime, 0.5f, 0.0f, 1.0f);
			bottomColor = RainbowColor(a2->currentTime, 0.5f, 0.5f, 1.0f);
		}

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
		if (Cvar_ion_chroma_gameinfo->GetBool())
		{
			bottomColor = RainbowColor(a2->currentTime, 0.5f, 0.0f, 1.0f);
			topColor = RainbowColor(a2->currentTime, 0.5f, 0.5f, 1.0f);
		}
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
	if (Cvar_ion_chroma_gameinfo->GetBool())
	{
		 auto color = RainbowColor(a2->currentTime, 0.5f, 0.0f, 1.0f);
		 a4->leftColor[0] = color.m128_f32[0];
		 a4->leftColor[1] = color.m128_f32[1];
		 a4->leftColor[2] = color.m128_f32[2];

		 color = RainbowColor(a2->currentTime, 0.5f, 0.5f, 1.0f);
		 a4->rightColor[0] = color.m128_f32[0];
		 a4->rightColor[1] = color.m128_f32[1];
		 a4->rightColor[2] = color.m128_f32[2];
	}
	else
	{
		a4->leftColor[0] = friendlyColor.m128_f32[0];
		a4->leftColor[1] = friendlyColor.m128_f32[1];
		a4->leftColor[2] = friendlyColor.m128_f32[2];

		a4->rightColor[0] = enemyColor.m128_f32[0];
		a4->rightColor[1] = enemyColor.m128_f32[1];
		a4->rightColor[2] = enemyColor.m128_f32[2];
	}
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
	if (Cvar_ion_chroma_gameinfo->GetBool())
	{
		auto color = RainbowColor(a2->currentTime, 0.5f, 0.0f, 1.0f);
		a4->leftColor.x = color.m128_f32[0];
		a4->leftColor.y = color.m128_f32[1];
		a4->leftColor.z = color.m128_f32[2];

		color = RainbowColor(a2->currentTime, 0.5f, 0.5f, 1.0f);
		a4->rightColor.x = color.m128_f32[0];
		a4->rightColor.y = color.m128_f32[1];
		a4->rightColor.z = color.m128_f32[2];
	}
	else
	{
		a4->leftColor.x = friendlyColor.m128_f32[0];
		a4->leftColor.y = friendlyColor.m128_f32[1];
		a4->leftColor.z = friendlyColor.m128_f32[2];

		a4->rightColor.x = enemyColor.m128_f32[0];
		a4->rightColor.y = enemyColor.m128_f32[1];
		a4->rightColor.z = enemyColor.m128_f32[2];
	}
	hook.Original(a1, a2, a3, a4);
});


DECLARE_HOOK(hk_crosshair_plus, ui(11).dll + 0x1D560, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, crosshair_plus_struct* a4)
{
	if (Cvar_ion_use_custom_crosshair->GetBool()) {
		return crosshair_plus(a1, a2, a3, a4);
	}
	hook.Original(a1, a2, a3, a4);
})

struct __declspec(align(8)) mastiff_ammo_counter_struct
{
  _BYTE gap0[56];
  int ammo;
  int clipSize;
  _BYTE gap40[4];
  float float44;
  float float48;
  float float4C;
  __m128 m12850;
  __m128 m12860;
  float float70;
  _DWORD dword74;
  float dword78;
  _DWORD dword7C;
  float float80;
  float float84;
  _DWORD dword88;
  float float8C;
  float float90;
  _DWORD dword94;
};



struct  kraber_ammo_counter_struct
{
  _BYTE gap0[40];
  int ammo;
  _DWORD clipSize;
  _BYTE gap30[4];
  float float34;
  float float38;
  float colorScale;
  __m128 color;
  __m128 color2;
  float float60;
  _DWORD dword64;
  float clipSizeTransform;
  _DWORD dword6C;
  float float70;
  _DWORD dword74;
  float float78;
  float float7C;
  _DWORD dword80;
};

DECLARE_HOOK(kraber_ammo_counter, ui(11).dll + 0x58BC0 , [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, kraber_ammo_counter_struct* a4)
{
  int clipSize; // eax
  float v8; // xmm2_4
  __m128 currentTime_low; // xmm6
  __m128 v10; // xmm1
  float v11; // xmm7_4
  __m128 v12; // xmm0
  __m128 v13; // xmm5
  float v14; // xmm6_4
  __m128 v15; // xmm4
  __m128 v16; // xmm1
  float v17; // xmm0_4
  __m128 v18; // xmm1
  float v19; // xmm8_4
  assetHandle v20; // eax
  float v21; // xmm0_4
  __m128 v22; // xmm0

  clipSize = a4->clipSize;
  v8 = a4->ammo / fmaxf(clipSize, 1.0);
  a4->float70 = v8;
  if ( v8 > 0.33000001 )
  {
    a4->colorScale = 1.0;
    v11 = 0.0;
    if ( v8 < 0.60000002 )
    {
      v12 = xmmword_D3130;
      a4->colorScale = 3.0;
    }
    else
    {
      v12 = xmmword_D32F0;
    }
    a4->color = v12;
  }
  else
  {
    a4->colorScale = 4.0;
    a4->color = xmmword_D3120;
    currentTime_low.m128_f32[0] = a2->currentTime * 2.0;
    v10 = _mm_cvtepi32_ps(_mm_cvttps_epi32(currentTime_low));
    v11 = 1.0
        - fminf(
            2.0
          - (fmaxf(
               0.0,
               1.0
             - ((1.0
               - (currentTime_low.m128_f32[0]
                - (_mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(currentTime_low, v10))).m128_f32[0] + v10.m128_f32[0])))
              * 4.0))
           * 2.0),
            1.0);
  }
  v13.m128_f32[0] = a2->currentTime;;
  a4->clipSizeTransform = (clipSize - 0.015625);
  v15 = v13;
  v15.m128_f32[0] = v13.m128_f32[0] * 200.0;
  a4->float78 = v13.m128_f32[0] * 10.0;
  v16 = _mm_cvtepi32_ps(_mm_cvttps_epi32(v15));
  v17 = _mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(v15, v16))).m128_f32[0] + v16.m128_f32[0];
  v18 = _mm_set1_ps(1.0f);
  v19 = fmaxf(0.0, 1.0 - ((1.0 - ((v13.m128_f32[0] * 200.0) - v17)) * 0.2));
  a4->dword64 = a1->LoadAsset(a3, "models/weapons/attachments/50cal_bullet_C_Black");
  
  a4->dword6C = a1->LoadAsset(a3, "models/weapons/attachments/50cal_bullet_C");;
  a4->float38 = a4->colorScale * 0.25;
  a4->dword74 = a1->LoadAsset(a3, "models/weapons/attachments/hemlok_panel_bleed");
  v22 = _mm_set_ss(a4->float78);
  a4->float7C = v22.m128_f32[0] + 1.0;
  *v22.m128_u64 = (a1->unknown_12)(a3);
  v18.m128_f32[0] = v22.m128_f32[0] * 0.15000001;
  v22.m128_f32[0] = v19;
  a4->color2 = _mm_mul_ps(_mm_mul_ps(_mm_shuffle_ps(v22, v22, 0), a4->color2), _mm_shuffle_ps(v18, v18, 0));
  a4->dword80 = a1->LoadAsset(a3, "models/weapons/attachments/whiteMult");
  a4->float60 = ((v11 * 0.5) * a4->colorScale) * a4->float34;
  return (a1->executeTransform)(a3, 82LL);
});

DECLARE_HOOK(mastiff_ammo_counter, ui(11).dll + 0x5D710, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, mastiff_ammo_counter_struct* a4)
{
	float v8; // xmm0_4
  float v9; // xmm2_4
  __m128 currentTime_low; // xmm6
  __m128 v11; // xmm1
  float v12; // xmm7_4
  __m128 v13; // xmm0
  bool extended; // zf
  float v16; // xmm6_4
  __m128 v17; // xmm4
  __m128 v18; // xmm1
  float v19; // xmm0_4
  __m128 v20; // xmm1
  float v21; // xmm8_4
  assetHandle v22; // eax
  float v23; // xmm0_4
  __m128 v24; // xmm0

  a4->float80 = a4->ammo / fmaxf(a4->clipSize, 1.0);
  v8 = a1->unknown_12(a3);
  v9 = a4->float80;
  a4->float84 = v8;
  if ( v9 > 0.33000001 )
  {
    a4->float4C = 1.0;
    v12 = 0.0;
    if ( v9 < 0.60000002 )
    {
      v13 = xmmword_D3130;
      a4->float4C = 3.0;
    }
    else
    {
      v13 = xmmword_D32F0;
    }
    a4->m12850 = v13;
  }
  else
  {
    a4->float4C = 4.0;
    a4->m12850 = xmmword_D3120;
    currentTime_low.m128_f32[0] = a2->currentTime * 2.0;
    v11 = _mm_cvtepi32_ps(_mm_cvttps_epi32(currentTime_low));
    v12 = 1.0
        - fminf(
            2.0
          - (fmaxf(
               0.0,
               1.0
             - ((1.0
               - (currentTime_low.m128_f32[0]
                - (_mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(currentTime_low, v11))).m128_f32[0] + v11.m128_f32[0])))
              * 4.0))
           * 2.0),
            1.0);
  }
  extended = a4->clipSize == 6;
  v16 = a4->float44;
  a4->dword78 = (a4->clipSize - 0.015625);
  v17.m128_f32[0] = a2->currentTime * 200.0;
  a4->float8C = a2->currentTime * 10.0;
  v18 = _mm_cvtepi32_ps(_mm_cvttps_epi32(v17));
  v19 = _mm_cvtepi32_ps(_mm_castps_si128(_mm_cmplt_ps(v17, v18))).m128_f32[0] + v18.m128_f32[0];
  v20 = _mm_set1_ps(1.0f);
  v21 = fmaxf(0.0, 1.0 - ((1.0 - ((a2->currentTime * 200.0) - v19)) * 0.2));
  a4->dword74 = a1->LoadAsset(a3, "models/weapons/attachments/Shotgun_Shell_C");
  v22 = a1->LoadAsset(a3, "rui/noise_uniform");
  v23 = a4->float4C * 0.25;
  a4->dword7C = v22;
  a4->float48 = v23;
  a4->dword88 = a1->LoadAsset(a3, "models/weapons/attachments/hemlok_panel_bleed");
  v24 = _mm_set1_ps(a4->float8C);
  a4->float90 = v24.m128_f32[0] + 1.0;
  *v24.m128_u64 = (a1->unknown_12)(a3);
  v20.m128_f32[0] = v24.m128_f32[0] * 0.15000001;
  v24.m128_f32[0] = v21;
  a4->m12860 = _mm_mul_ps(_mm_mul_ps(_mm_shuffle_ps(v24, v24, 0), a4->m12850), _mm_shuffle_ps(v20, v20, 0));
  a4->dword94 = a1->LoadAsset(a3, "models/weapons/attachments/whiteMult");
  a4->float70 = ((v12 * 0.5) * a4->float4C) * v16;
  return (a1->executeTransform)(a3, 82LL);
});

struct __declspec(align(8)) hcog_lower_struct
{
  _BYTE gap0[76];
  float vis;
  unsigned int ammo;
  _DWORD clipSize;
  _QWORD qword58;
  _DWORD dword60;
  _DWORD dword64;
  const char* qword68;
  __m128 m12870;
  __m128 m12880;
  __m128 m12890;
  __m128 m128A0;
  __m128 m128B0;
  _DWORD dwordC0;
  _DWORD dwordC4;
  _DWORD dwordC8;
  _DWORD dwordCC;
  _DWORD dwordD0;
  float floatD4;
};


void __fastcall hcog_lowerF(RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, hcog_lower_struct* a4)
{
  const char *v7; // rax
  __m128i v8; // xmm0
  float v9; // xmm2_4
  __m128 v10; // xmm7
  __m128 vis_low; // xmm6
  __m128 v12; // xmm0
  __m128 v13; // xmm6
  __m128 v14; // xmm0
  __m128 *v15; // rbx

  v7 = a1->printf(a3, "%i", a4->ammo);
  v8 = _mm_cvtsi32_si128(a4->clipSize);
  a4->qword68 = v7;
  a4->floatD4 = a4->ammo / fmaxf(_mm_cvtepi32_ps(v8).m128_f32[0], 1.0);
  *v8.m128i_i32 = a1->unknown_12(a3);
  v9 = a4->floatD4;
  a4->dwordC4 = v8.m128i_i32[0];
  if ( v9 > 0.33000001 )
  {
    if ( v9 < 0.60000002 )
      v10 = xmmword_D3130;
    else
      v10 = xmmword_D32F0;
  }
  else
  {
    v10 = xmmword_D3120;
  }
  vis_low = _mm_set_ps1(a4->vis);
  v12 = _mm_mul_ps(xmmword_D3790, v10);
  a4->qword58 = v12.m128_i64[0];
  a4->dword60 = _mm_shuffle_epi32(_mm_castps_si128(v12), 170).m128i_u32[0];
  //a4->dword64 = a1->LoadAsset(a3, "models/weapons/attachments/hcog_reticle_front_col");
  v13 = _mm_shuffle_ps(vis_low, vis_low, 0);
  a4->dwordC0 = a1->LoadAsset(a3, "rui/noise_multires");
  a4->m12870 = _mm_mul_ps(xmmword_D3360, v13);
  //a4->dwordC8 = a1->LoadAsset(a3, "models/weapons/attachments/hcog_reticle_front_col_trans");
  a4->dwordC8 = a1->LoadAsset(a3, "models/weapons/attachments/holo_reflex_reticle_small_col");

  a4->m12880 = _mm_mul_ps(xmmword_D3340, v13);
  //a4->dwordCC = a1->LoadAsset(a3, "models/weapons/attachments/hcog_reticle_rear_col");
  v14 = _mm_mul_ps(xmmword_D2DE0, v13);
  a4->m12890 = v14;
  a4->m128A0 = _mm_mul_ps(xmmword_D2FD0, v13);
  a4->dwordD0 = a1->LoadAsset(a3, "models/weapons/attachments/ammo_counter_meter_col");
  a4->m128B0 = _mm_mul_ps(v13, v10);
  v15 = a1->GetTransformSize(a3);
  v15[8] = (a1->GetTextSize)(a3, 0LL);
  a1->executeTransform(a3, 114);
}


DECLARE_HOOK(hcog_lower, ui(11).dll + 0x0529B0, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, hcog_lower_struct* a4) {
	//hcog_lowerF(a1, a2, a3, a4);
	hook.Original(a1, a2, a3, a4);
});


DECLARE_HOOK(pilot_speedometer, ui(11).dll + 0x6AA50, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, hcog_lower_struct* a4)
{
	static CMemory module = GetModuleHandleA("ui(11).dll");
	if (Cvar_ion_speedometer_always_show->GetBool())
	{
		module.Offset(0x6AB7F).Patch({0x90, 0x90});
	}
	else
	{
		module.Offset(0x6AB7F).Patch({0x72,0x2C});
	}
	hook.Original(a1, a2, a3, a4);
});

bool WriteToReadOnly(void* addr, const void* data, size_t size)
{
	DWORD oldProtect;

	if (!VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect))
		return false;

	memcpy(addr, data, size);

	VirtualProtect(addr, size, oldProtect, &oldProtect);
	return true;
}

float* max_speed_imperial;
float* max_speed_metric;
DECLARE_HOOK(gauntlet_hud, ui(11).dll + 0x4E030, [](auto& hook, RuiFunctions_t* a1, RuiGlobals* a2, RuiInstance* a3, void* a4)
{
	float gauntletMaxSpeedMetric = Cvar_gauntlet_timer_max_speed_metric->GetFloat();
	float gauntletMaxSpeedImperial = Cvar_gauntlet_timer_max_speed_imperial->GetFloat();
	WriteToReadOnly((void*)max_speed_metric, &gauntletMaxSpeedMetric, sizeof(float));
	WriteToReadOnly((void*)max_speed_imperial, &gauntletMaxSpeedImperial, sizeof(float));
	
	hook.Original(a1, a2, a3, a4);
});

ON_DLL_LOAD("ui(11).dll", RuiStuff, [](CModule module)
{
	pGamestate_info_ffa = module.Offset(0x3E8E0).RCast<gamestate_info_ffa_t>();
	v_hk_gamestate_info_ffa.Dispatch(reinterpret_cast<void*>(pGamestate_info_ffa));

	xmmword_D2BD0.m128_u64[0] = 0xFFFFFFFFFFFFFFFFULL;
	xmmword_D2BD0.m128_u64[1] = 0xFFFFFFFFULL;
	max_speed_imperial = module.Offset(0xD1AD8).RCast<float*>();
	max_speed_metric = module.Offset(0xD1B1C).RCast<float*>();

	DISPATCH_MODULE(RuiHooks);
})

ON_DLL_LOAD_CLIENT_RELIESON("engine.dll", ClientAuthHooks, ConVar, [](CModule module)
{
	Cvar_ion_use_custom_crosshair = new ConVar("ion_use_custom_crosshair", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Use custom crosshairs. 1 = enabled, 0 = disabled.");
	Cvar_ion_crosshair_gap_v       = new ConVar("ion_crosshair_gap_v",        "0.0074074", FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm gap from center.");
	Cvar_ion_crosshair_length_v    = new ConVar("ion_crosshair_length_v",      "0.01200",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm segment length.");
	Cvar_ion_crosshair_inset_v     = new ConVar("ion_crosshair_inset_v",       "0.00100",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm inner shadow inset.");
	Cvar_ion_crosshair_thickness_l = new ConVar("ion_crosshair_thickness_l",   "0.00185",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm thickness left of center.");
	Cvar_ion_crosshair_thickness_r = new ConVar("ion_crosshair_thickness_r",   "0.00833",   FCVAR_ARCHIVE_PLAYERPROFILE, "Vertical arm thickness right of center.");
	Cvar_ion_crosshair_gap_h       = new ConVar("ion_crosshair_gap_h",         "0.00417",   FCVAR_ARCHIVE_PLAYERPROFILE, "Horizontal arm gap from center.");
	Cvar_ion_crosshair_length_h    = new ConVar("ion_crosshair_length_h",       "0.00700",   FCVAR_ARCHIVE_PLAYERPROFILE, "Horizontal arm segment length.");
	Cvar_ion_chroma_gameinfo =
			new ConVar("ion_chroma_gameinfo", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Rainbow colors for game info. 1 = enabled, 0 = disabled.");

	Cvar_ion_speedometer_always_show =
		new ConVar("ion_speedometer_always_show", "0", FCVAR_ARCHIVE_PLAYERPROFILE, "Always show speedometer. 1 = enabled, 0 = disabled.");


	Cvar_gauntlet_timer_max_speed_metric = new ConVar(
			"gauntlet_timer_max_speed_metric",
			"50.505",
			FCVAR_ARCHIVE_PLAYERPROFILE,
			"Max speed in gauntlet timer (metric).");

	Cvar_gauntlet_timer_max_speed_imperial = new ConVar(
			"gauntlet_timer_max_speed_imperial",
			"31.1",
			FCVAR_ARCHIVE_PLAYERPROFILE, "Max speed in gauntlet timer (imperial).");
	});

