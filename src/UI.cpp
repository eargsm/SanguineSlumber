#include "Settings.h"

#include "SKSEMenuFramework.h"

// In-game settings page: Mod Control Panel (SKSE Menu Framework) > Sanguine Slumber > Settings.
// Render() runs on the menu's render thread; it only touches the atomic Settings/Stats values.

namespace
{
	void HelpMarker(const char* a_text)
	{
		ImGuiMCP::SameLine();
		ImGuiMCP::TextDisabled("(?)");
		if (ImGuiMCP::IsItemHovered()) {
			ImGuiMCP::SetTooltip("%s", a_text);
		}
	}

	// Checkbox bound to an atomic setting; saves the INI immediately when toggled.
	void SettingCheckbox(const char* a_label, std::atomic<bool>& a_value, const char* a_help)
	{
		bool v = a_value.load();
		if (ImGuiMCP::Checkbox(a_label, &v)) {
			a_value = v;
			Settings::Save();
		}
		HelpMarker(a_help);
	}

	// Logarithmic percentage slider bound to an atomic setting; saves the INI when the user lets go.
	void SettingPercent(const char* a_label, std::atomic<float>& a_value, float a_max, const char* a_help)
	{
		float v = a_value.load();
		ImGuiMCP::SetNextItemWidth(260.0f);
		if (ImGuiMCP::SliderFloat(a_label, &v, 0.0f, a_max, "%.2f%%",
				ImGuiMCP::ImGuiSliderFlags_Logarithmic | ImGuiMCP::ImGuiSliderFlags_AlwaysClamp)) {
			a_value = v;
		}
		if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
			Settings::Save();
		}
		HelpMarker(a_help);
	}

	void __stdcall Render()
	{
		ImGuiMCP::SeparatorText("Sanguine Slumber");
		ImGuiMCP::TextWrapped("Vampiric Drain no longer spreads Sanguinare Vampiris. Instead, each time you sleep there is a small chance to catch it - higher in the wild than in safe places.");
		ImGuiMCP::Spacing();

		SettingCheckbox("Mod enabled", Settings::enabled,
			"Master switch. When off, vampires infect you the vanilla way and sleeping is safe.");

		ImGuiMCP::BeginDisabled(!Settings::enabled.load());

		SettingCheckbox("Block Vampiric Drain infection", Settings::blockDrainInfection,
			"Vampires' Vampiric Drain can no longer give you Sanguinare Vampiris.\n"
			"The Redwater Den bloodspring and quest infections are never blocked.");

		ImGuiMCP::SeparatorText("Sleep infection chance");
		SettingPercent("Sleep chance", Settings::sleepChanceSafe, 100.0f,
			"Chance per sleep to contract Sanguinare Vampiris. Default 0.50%.\n"
			"With a separate wild chance on, this applies to safe places only:\n"
			"inns, player homes, houses, temples, castles and town/city grounds.\n"
			"Ctrl+click a slider to type an exact value.");
		SettingCheckbox("Separate chance in the wild", Settings::wildChanceEnabled,
			"When on, sleeping in camps, bedrolls, tents, caves, ruins, forts or the open wilderness\n"
			"uses its own chance below. When off, every sleep uses the chance above.");
		ImGuiMCP::BeginDisabled(!Settings::wildChanceEnabled.load());
		SettingPercent("Wild sleep chance", Settings::sleepChanceWild, 100.0f, "Default 2.00%.");
		ImGuiMCP::EndDisabled();

		ImGuiMCP::SeparatorText("Other");
		SettingCheckbox("Only roll after uninterrupted sleep", Settings::requireUninterruptedSleep,
			"When on, waking up early (e.g. attacked in your sleep) never infects you.");
		SettingCheckbox("Show notification when infected", Settings::showNotification,
			"Shows \"You have contracted Sanguinare Vampiris.\" when a sleep roll infects you.");

		ImGuiMCP::EndDisabled();

		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button("Reset to defaults")) {
			Settings::ResetToDefaults();
		}
		HelpMarker("Enabled, drain blocking on, sleep chance 0.50%, separate wild chance on at 2.00%, notification on.");

		ImGuiMCP::SeparatorText("This session");
		ImGuiMCP::Text("Sleep rolls: %d", Stats::sleepRolls.load());
		const float last = Stats::lastRoll.load();
		if (last >= 0.0f) {
			ImGuiMCP::Text("Last roll: %.2f vs %.2f%% (%s) -> %s", last, Stats::lastChance.load(),
				Stats::lastRollWild.load() ? "wild" : "safe place", Stats::lastRollInfected.load() ? "infected" : "not infected");
		} else {
			ImGuiMCP::Text("Last roll: none yet");
		}
		ImGuiMCP::Text("Infections from sleep: %d", Stats::sleepInfections.load());
		ImGuiMCP::Text("Drain infections blocked: %d", Stats::drainBlocks.load());
		ImGuiMCP::TextDisabled("Settings are saved to SKSE/Plugins/SanguineSlumber.ini");
	}
}

void UI::Register()
{
	if (!SKSEMenuFramework::IsInstalled()) {
		logs::info("SKSE Menu Framework not installed - settings are INI-only");
		return;
	}
	SKSEMenuFramework::SetSection("Sanguine Slumber");
	SKSEMenuFramework::AddSectionItem("Settings", Render);
	logs::info("Registered SKSE Menu Framework page");
}
