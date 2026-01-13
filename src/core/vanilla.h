#pragma once

/// Determines if we are in vanilla-compatibility mode.
class VanillaCompatibility
{
public:
	enum class CompatibilityMode
	{
		Vanilla,
		Northstar,
	};

	void SetCompatabilityMode(CompatibilityMode mode)
	{
		if (m_bLastCompatabilityMode != mode)
		{
			m_bLastCompatabilityMode = mode;
			spdlog::info("Vanilla compatibility mode set to {}", mode == CompatibilityMode::Vanilla ? "Vanilla" : "Northstar");
		}
	}

	bool GetVanillaCompatibility() { return m_bLastCompatabilityMode == CompatibilityMode::Vanilla; }

private:
	CompatibilityMode m_bLastCompatabilityMode = CompatibilityMode::Vanilla;
};

inline VanillaCompatibility* g_pVanillaCompatibility;
