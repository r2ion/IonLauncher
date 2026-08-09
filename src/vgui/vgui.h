#pragma once

#include "vgui_controls/Panel.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

enum LevelLoadingProgress_e
{
	PROGRESS_NONE,
	PROGRESS_CHANGELEVEL,
	PROGRESS_SPAWNSERVER,
	PROGRESS_LOADWORLDMODEL,
	PROGRESS_CRCMAP,
	PROGRESS_CRCCLIENTDLL,
	PROGRESS_CREATENETWORKSTRINGTABLES,
	PROGRESS_PRECACHEWORLD,
	PROGRESS_CLEARWORLD,
	PROGRESS_LEVELINIT,
	PROGRESS_PRECACHE,
	PROGRESS_ACTIVATESERVER,
	PROGRESS_BEGINCONNECT,
	PROGRESS_SIGNONCHALLENGE,
	PROGRESS_SIGNONCONNECT,
	PROGRESS_SIGNONCONNECTED,
	PROGRESS_PROCESSSERVERINFO,
	PROGRESS_PROCESSSTRINGTABLE,
	PROGRESS_SIGNONNEW,
	PROGRESS_SENDCLIENTINFO,
	PROGRESS_SENDSIGNONDATA,
	PROGRESS_SIGNONSPAWN,
	PROGRESS_CREATEENTITIES,
	PROGRESS_FULLYCONNECTED,
	PROGRESS_PRECACHELIGHTING,
	PROGRESS_READYTOPLAY,
	PROGRESS_HIGHESTITEM,
	PROGRESS_INVALID = -2,
	PROGRESS_DEFAULT = -1,
};

struct LoadingProgressDescription_t
{
	LevelLoadingProgress_e eProgress;
	int nPercent;
	int nRepeat;
	const char* pszDesc;
};

namespace vgui
{
	// Valve Source establishes the Panel -> ProgressBar -> ContinuousProgressBar
	// hierarchy. Retail client.dll (SHA-256
	// 002b36487fec7c98882929fbccdb506d0146bbf3270cbb964a4d21c5edf7eebc)
	// constructors and build factories prove the Titanfall-specific layout below.
	// Retail ProgressBar adds SetProgress, SetSegmentInfo, and
	// OnDialogVariablesChanged at absolute slots 230-232. ContinuousProgressBar
	// overrides inherited Paint at absolute slot 130. None is called here, so
	// the model deliberately omits dead virtual wrappers.
	class ProgressBar : public Panel
	{
	public:
		enum ProgressDirection : int
		{
			PROGRESS_EAST,
			PROGRESS_WEST,
			PROGRESS_NORTH,
			PROGRESS_SOUTH,
		};

		float GetProgress() const noexcept { return m_Progress; }
		void SetProgressValue(float progress) noexcept { m_Progress = progress; }

		ProgressDirection m_ProgressDirection; // 0x268
		float m_Progress;                     // 0x26C
		int m_SegmentCount;                   // 0x270
		int m_SegmentGap;                     // 0x274
		int m_SegmentWidth;                   // 0x278
		int m_BarInset;                       // 0x27C
		char* m_DialogVariable;               // 0x280
	};

	class ContinuousProgressBar : public ProgressBar
	{
	public:
		// The retail build factory allocates 0x290 bytes, while the constructor
		// and recovered methods leave this bounded derived extension untouched.
		std::uint64_t m_RetailExtension; // 0x288
	};

	static_assert(std::is_base_of_v<Panel, ProgressBar>);
	static_assert(std::is_base_of_v<ProgressBar, ContinuousProgressBar>);
	static_assert(sizeof(ProgressBar) == 0x288);
	static_assert(alignof(ProgressBar) == 0x8);
	static_assert(offsetof(ProgressBar, m_ProgressDirection) == 0x268);
	static_assert(offsetof(ProgressBar, m_Progress) == 0x26C);
	static_assert(offsetof(ProgressBar, m_SegmentCount) == 0x270);
	static_assert(offsetof(ProgressBar, m_SegmentGap) == 0x274);
	static_assert(offsetof(ProgressBar, m_SegmentWidth) == 0x278);
	static_assert(offsetof(ProgressBar, m_BarInset) == 0x27C);
	static_assert(offsetof(ProgressBar, m_DialogVariable) == 0x280);
	static_assert(sizeof(ContinuousProgressBar) == 0x290);
	static_assert(alignof(ContinuousProgressBar) == 0x8);
	static_assert(offsetof(ContinuousProgressBar, m_RetailExtension) == 0x288);


	class Label;
} // namespace vgui

extern void (*vgui_Label_SetText)(vgui::Label* thisptr, const char* text);
