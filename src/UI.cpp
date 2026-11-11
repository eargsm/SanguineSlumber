#include "Settings.h"

#include "SKSEMenuFramework.h"

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

	void SettingCheckbox(const char* a_label, std::atomic<bool>& a_value, const char* a_help, void (*a_onChange)() = nullptr)
	{
		bool v = a_value.load();
		if (ImGuiMCP::Checkbox(a_label, &v)) {
			a_value = v;
			Settings::Save();
			if (a_onChange) {
				a_onChange();
			}
		}
		if (a_help) {
			HelpMarker(a_help);
		}
	}

	void SettingSlider(const char* a_label, std::atomic<float>& a_value, float a_min, float a_max, const char* a_format,
		ImGuiMCP::ImGuiSliderFlags a_flags, const char* a_help)
	{
		float v = a_value.load();
		ImGuiMCP::SetNextItemWidth(260.0f);
		if (ImGuiMCP::SliderFloat(a_label, &v, a_min, a_max, a_format, a_flags | ImGuiMCP::ImGuiSliderFlags_AlwaysClamp)) {
			a_value = v;
		}
		if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
			Settings::Save();
		}
		if (a_help) {
			HelpMarker(a_help);
		}
	}

	void SettingPercent(const char* a_label, std::atomic<float>& a_value, const char* a_help)
	{
		SettingSlider(a_label, a_value, 0.0f, 100.0f, "%.2f%%", ImGuiMCP::ImGuiSliderFlags_Logarithmic, a_help);
	}

	void __stdcall Render()
	{
		ImGuiMCP::SeparatorText("Sanguine Slumber");
		ImGuiMCP::TextWrapped("Each time you sleep at night a vampire may visit and feed on you - more likely in the wild than in safe places. You wake up Drained with a bite on your neck, and the bite may give you Sanguinare Vampiris. Optionally, vampires' Vampiric Drain can stop spreading it.");
		ImGuiMCP::Spacing();

		SettingCheckbox("Mod enabled", Settings::enabled,
			"Master switch. When off, vampires infect you the vanilla way and sleeping is safe.");

		ImGuiMCP::BeginDisabled(!Settings::enabled.load());

		SettingCheckbox("Block Vampiric Drain infection", Settings::blockDrainInfection,
			"Off by default. When on, vampires' Vampiric Drain can no longer give you Sanguinare Vampiris.\n"
			"The Redwater Den bloodspring and quest infections are never blocked.");

		ImGuiMCP::SeparatorText("Vampire visits");
		SettingPercent("Visit chance", Settings::visitChanceSafe,
			"Chance per night's sleep that a vampire visits and feeds on you. Default 10%.\n"
			"With a separate wild chance on, this applies to safe places only:\n"
			"inns, player homes, houses, temples, castles and town/city grounds.\n"
			"Ctrl+click a slider to type an exact value.");
		SettingCheckbox("Separate chance in the wild", Settings::wildChanceEnabled,
			"When on, sleeping in camps, bedrolls, tents, caves, ruins, forts or the open wilderness\n"
			"uses its own chance below. When off, every sleep uses the chance above.");
		ImGuiMCP::BeginDisabled(!Settings::wildChanceEnabled.load());
		SettingPercent("Wild visit chance", Settings::visitChanceWild, "Default 20%.");
		ImGuiMCP::EndDisabled();
		SettingPercent("Infection chance per bite", Settings::biteInfectionChance,
			"Chance that a visit also gives you Sanguinare Vampiris. Default 50%.\n"
			"With the defaults that is 5% per night in a safe place and 10% in the wild.");
		SettingSlider("Drained for", Settings::drainedHours, 1.0f, 72.0f, "%.0f h", 0,
			"In-game hours the Drained debuff lasts after a visit: -10 Health, -5 Stamina,\n"
			"both regenerate at half speed. Cure Disease removes it early. Default 24 h.\n"
			"A new length applies to the next visit.");
		ImGuiMCP::BeginDisabled(!Feeding::raceMenuPresent.load());
		SettingCheckbox("Bite mark on the neck", Settings::biteMark, nullptr, Feeding::RefreshMark);
		ImGuiMCP::EndDisabled();
		HelpMarker(Feeding::raceMenuPresent.load() ?
					   "While Drained, two small fang marks show on your neck (a RaceMenu face overlay).\n"
					   "Human, elf and orc heads only." :
					   "Needs RaceMenu, which is not installed.");

		ImGuiMCP::SeparatorText("Night only");
		SettingCheckbox("Only roll when sleeping at night", Settings::nightOnly,
			"When on, vampires only visit a sleep that starts during the night window.\n"
			"Naps during the day are always safe. Default on, 20:00 - 06:00.");
		ImGuiMCP::BeginDisabled(!Settings::nightOnly.load());
		SettingSlider("Night starts", Settings::nightStartHour, 0.0f, 24.0f, "%.1f h", 0, nullptr);
		SettingSlider("Night ends", Settings::nightEndHour, 0.0f, 24.0f, "%.1f h", 0,
			"In-game clock hour. The window may wrap past midnight (e.g. 20 -> 6).");
		ImGuiMCP::EndDisabled();

		ImGuiMCP::SeparatorText("Other");
		SettingCheckbox("Only roll after uninterrupted sleep", Settings::requireUninterruptedSleep,
			"When on, waking up early (e.g. attacked in your sleep) means no visit.");
		SettingCheckbox("Show notifications", Settings::showNotification,
			"Shows a message when a vampire fed on you, and\n"
			"\"You have contracted Sanguinare Vampiris.\" when the bite infects you.");

		if (Addons::ostimVisitorPresent) {
			ImGuiMCP::SeparatorText("Add-ons");
			SettingCheckbox("OStim visitor enabled", Settings::ostimVisitorEnabled,
				"Sanguine Slumber - OStim Addon: when a vampire feeds on you in your sleep, it appears\n"
				"as you wake and offers an OStim scene (Accept / Resist). Off = the add-on does nothing.",
				Addons::ApplyToggles);

			ImGuiMCP::BeginDisabled(!Settings::ostimVisitorEnabled.load());
			ImGuiMCP::Indent();
			SettingCheckbox("Male visitors", Settings::ostimVisitorAllowMale, nullptr, Addons::ApplyToggles);
			SettingCheckbox("Female visitors", Settings::ostimVisitorAllowFemale,
				"Which vampires may visit. The visitor is a random vanilla vampire of an allowed gender.\n"
				"Both off = no visitor appears (the feeding still happens).",
				Addons::ApplyToggles);
			ImGuiMCP::Unindent();
			ImGuiMCP::EndDisabled();
		}

		ImGuiMCP::EndDisabled();

		ImGuiMCP::Spacing();
		if (ImGuiMCP::Button("Reset to defaults")) {
			Settings::ResetToDefaults();
			Addons::ApplyToggles();
		}
		HelpMarker("Enabled, drain blocking off, visit chance 10%, separate wild chance on at 20%, infection per bite 50%,\n"
				   "Drained for 24 h, bite mark on, night only (20:00 - 06:00), notifications on.");

		ImGuiMCP::SeparatorText("This session");
		ImGuiMCP::Text("Sleep rolls: %d", Stats::sleepRolls.load());
		const float last = Stats::lastRoll.load();
		if (last >= 0.0f) {
			ImGuiMCP::Text("Last roll: %.2f vs %.2f%% (%s) -> %s", last, Stats::lastChance.load(),
				Stats::lastRollWild.load() ? "wild" : "safe place",
				!Stats::lastRollVisited.load() ? "no visit" : Stats::lastRollInfected.load() ? "visited, infected" : "visited, not infected");
		} else {
			ImGuiMCP::Text("Last roll: none yet");
		}
		ImGuiMCP::Text("Vampire visits: %d", Stats::visits.load());
		ImGuiMCP::Text("Infections from bites: %d", Stats::sleepInfections.load());
		if (const float hours = Feeding::RemainingHours(); hours > 0.0f) {
			ImGuiMCP::Text("Drained: %.1f h left", hours);
		}
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
