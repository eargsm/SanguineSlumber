#include "Settings.h"

namespace
{
	const std::filesystem::path kPath{ "Data/SKSE/Plugins/SanguineSlumber.ini" };
	std::mutex                  g_fileLock;

	std::string Trim(const std::string& s)
	{
		const auto first = s.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = s.find_last_not_of(" \t\r\n");
		return s.substr(first, last - first + 1);
	}

	bool ParseBool(const std::string& v) { return v == "1" || v == "true" || v == "True" || v == "TRUE"; }

	const char* Bool(bool v) { return v ? "true" : "false"; }

	void LogSettings(const char* a_prefix)
	{
		logs::info("{}: enabled={} blockDrain={} visitSafe={}% separateWild={} visitWild={}% biteInfection={}% biteMark={} "
				   "drainedHours={} requireUninterrupted={} nightOnly={} night={}-{} notify={} drainWindow={}s verbose={} ostimVisitor={}",
			a_prefix, Settings::enabled.load(), Settings::blockDrainInfection.load(), Settings::visitChanceSafe.load(),
			Settings::wildChanceEnabled.load(), Settings::visitChanceWild.load(), Settings::biteInfectionChance.load(),
			Settings::biteMark.load(), Settings::drainedHours.load(), Settings::requireUninterruptedSleep.load(),
			Settings::nightOnly.load(), Settings::nightStartHour.load(), Settings::nightEndHour.load(),
			Settings::showNotification.load(), Settings::drainWindowSeconds.load(), Settings::verboseLog.load(),
			Settings::ostimVisitorEnabled.load());
	}
}

void Settings::Load()
{
	std::scoped_lock lock{ g_fileLock };
	std::ifstream    file{ kPath };
	if (!file) {
		logs::warn("{} not found, using defaults", kPath.string());
		return;
	}

	std::string line;
	while (std::getline(file, line)) {
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
			continue;
		}
		const auto eq = line.find('=');
		if (eq == std::string::npos) {
			continue;
		}
		const auto key = Trim(line.substr(0, eq));
		auto       value = Trim(line.substr(eq + 1));
		if (const auto comment = value.find(';'); comment != std::string::npos) {
			value = Trim(value.substr(0, comment));
		}

		try {
			if (key == "bEnabled") {
				enabled = ParseBool(value);
			} else if (key == "bBlockDrainInfection") {
				blockDrainInfection = ParseBool(value);
			} else if (key == "fVisitChanceSafe") {
				visitChanceSafe = std::clamp(std::stof(value), 0.0f, 100.0f);
			} else if (key == "bSeparateWildChance") {
				wildChanceEnabled = ParseBool(value);
			} else if (key == "fVisitChanceWild") {
				visitChanceWild = std::clamp(std::stof(value), 0.0f, 100.0f);
			} else if (key == "fBiteInfectionChance") {
				biteInfectionChance = std::clamp(std::stof(value), 0.0f, 100.0f);
			} else if (key == "bBiteMark") {
				biteMark = ParseBool(value);
			} else if (key == "fDrainedHours") {
				drainedHours = std::clamp(std::stof(value), 1.0f, 72.0f);
			} else if (key == "sVisitText") {
				visitText = value;
			} else if (key == "bRequireUninterruptedSleep") {
				requireUninterruptedSleep = ParseBool(value);
			} else if (key == "bNightOnly") {
				nightOnly = ParseBool(value);
			} else if (key == "fNightStartHour") {
				nightStartHour = std::clamp(std::stof(value), 0.0f, 24.0f);
			} else if (key == "fNightEndHour") {
				nightEndHour = std::clamp(std::stof(value), 0.0f, 24.0f);
			} else if (key == "bShowNotification") {
				showNotification = ParseBool(value);
			} else if (key == "sNotificationText") {
				notificationText = value;
			} else if (key == "fDrainWindowSeconds") {
				drainWindowSeconds = std::clamp(std::stof(value), 0.5f, 60.0f);
			} else if (key == "bVerboseLog") {
				verboseLog = ParseBool(value);
			} else if (key == "bOStimVisitorEnabled") {
				ostimVisitorEnabled = ParseBool(value);
			} else if (key == "bOStimVisitorAllowMale") {
				ostimVisitorAllowMale = ParseBool(value);
			} else if (key == "bOStimVisitorAllowFemale") {
				ostimVisitorAllowFemale = ParseBool(value);
			}
		} catch (const std::exception&) {
			logs::warn("Bad value for {}: '{}', keeping default", key, value);
		}
	}
	LogSettings("Settings loaded");
}

void Settings::Save()
{
	std::scoped_lock lock{ g_fileLock };
	{
		std::ofstream out{ kPath, std::ios::trunc };
		if (!out) {
			logs::error("Could not write {}", kPath.string());
			return;
		}
		out << "; Sanguine Slumber - settings\n"
			   "; Also editable in game: Mod Control Panel (SKSE Menu Framework) > Sanguine Slumber.\n"
			   "; The in-game menu rewrites this file whenever you change a setting.\n"
			   "\n"
			   "[General]\n"
			   "; Master switch. false = the mod does nothing (vanilla drain infection, no sleep rolls).\n"
			<< "bEnabled = " << Bool(enabled) << "\n"
			<< "\n"
			   "; true = a vampire's Vampiric Drain can no longer give you Sanguinare Vampiris. Default false\n"
			<< "bBlockDrainInfection = " << Bool(blockDrainInfection) << "\n"
			<< "\n"
			   "; Percent chance (0 - 100) that a vampire visits and feeds on you while you sleep. Default 10\n"
			<< "fVisitChanceSafe = " << std::format("{:.2f}", visitChanceSafe.load()) << "\n"
			<< "\n"
			   "; true = sleeping in the wild uses its own chance below. Safe = inns, homes, temples, castles and\n"
			   "; town/city grounds (LocTypeDwelling / LocTypeHabitation / LocTypePlayerHouse, or an interior cell with\n"
			   "; no location). Wild = everywhere else: camps, caves, ruins, forts, wilderness. Defaults: true, 20\n"
			<< "bSeparateWildChance = " << Bool(wildChanceEnabled) << "\n"
			<< "fVisitChanceWild = " << std::format("{:.2f}", visitChanceWild.load()) << "\n"
			<< "\n"
			   "; Percent chance (0 - 100) that a vampire's bite gives you Sanguinare Vampiris. Default 50\n"
			<< "fBiteInfectionChance = " << std::format("{:.2f}", biteInfectionChance.load()) << "\n"
			<< "\n"
			   "; A visit leaves you Drained (-10 Health, -5 Stamina, both regenerate at half speed) for this many\n"
			   "; in-game hours (1 - 72). Cure Disease removes it early. Default 24\n"
			<< "fDrainedHours = " << std::format("{:.1f}", drainedHours.load()) << "\n"
			<< "\n"
			   "; true = while Drained, a bite mark shows on your neck (needs RaceMenu; uses a free face overlay slot).\n"
			<< "bBiteMark = " << Bool(biteMark) << "\n"
			<< "\n"
			   "; true = only roll after a full, uninterrupted sleep.\n"
			<< "bRequireUninterruptedSleep = " << Bool(requireUninterruptedSleep) << "\n"
			<< "\n"
			   "; true = only roll when you go to sleep at night, between fNightStartHour and fNightEndHour\n"
			   "; (in-game clock, 0 - 24; the window may wrap past midnight). Defaults: true, 20, 6\n"
			<< "bNightOnly = " << Bool(nightOnly) << "\n"
			<< "fNightStartHour = " << std::format("{:.1f}", nightStartHour.load()) << "\n"
			<< "fNightEndHour = " << std::format("{:.1f}", nightEndHour.load()) << "\n"
			<< "\n"
			   "; Show a message when a vampire fed on you, and when you catch Sanguinare Vampiris from the bite.\n"
			<< "bShowNotification = " << Bool(showNotification) << "\n"
			<< "sVisitText = " << visitText << "\n"
			<< "sNotificationText = " << notificationText << "\n"
			<< "\n"
			   "; How long (seconds) after a vampire's drain hits you its infection attempt is blocked.\n"
			   "; The vanilla drain script fires ~0.07s after the hit; 5 leaves room for script lag.\n"
			<< "fDrainWindowSeconds = " << std::format("{:.1f}", drainWindowSeconds.load()) << "\n"
			<< "\n"
			   "; Extra detail in Documents/My Games/Skyrim Special Edition/SKSE/SanguineSlumber.log\n"
			<< "bVerboseLog = " << Bool(verboseLog) << "\n"
			<< "\n"
			   "[Addons]\n"
			   "; Sanguine Slumber - OStim Addon (separate download). Ignored when the add-on is not installed.\n"
			<< "bOStimVisitorEnabled = " << Bool(ostimVisitorEnabled) << "\n"
			   "; Which visitors may appear. Both false = no visitor.\n"
			<< "bOStimVisitorAllowMale = " << Bool(ostimVisitorAllowMale) << "\n"
			<< "bOStimVisitorAllowFemale = " << Bool(ostimVisitorAllowFemale) << "\n";
		if (!out) {
			logs::error("Write to {} failed", kPath.string());
			return;
		}
	}
	LogSettings("Settings saved");
}

void Addons::Detect()
{
	auto dataHandler = RE::TESDataHandler::GetSingleton();
	ostimVisitorPresent = dataHandler && dataHandler->LookupModByName(kOStimVisitorPlugin) != nullptr;
	logs::info("Add-on {}: {}", kOStimVisitorPlugin, ostimVisitorPresent.load() ? "installed" : "not installed");
}

void Addons::ApplyToggles()
{
	if (!ostimVisitorPresent) {
		return;
	}
	SKSE::GetTaskInterface()->AddTask([]() {
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		auto set = [dataHandler](RE::FormID a_id, bool a_value) {
			auto global = dataHandler ? dataHandler->LookupForm<RE::TESGlobal>(a_id, kOStimVisitorPlugin) : nullptr;
			if (!global) {
				logs::warn("Add-on {} has no global {:03X} (older add-on version?)", kOStimVisitorPlugin, a_id);
				return;
			}
			global->value = a_value ? 1.0f : 0.0f;
		};
		set(kOStimVisitorEnabledGlobal, Settings::ostimVisitorEnabled);
		set(kOStimVisitorAllowMaleGlobal, Settings::ostimVisitorAllowMale);
		set(kOStimVisitorAllowFemaleGlobal, Settings::ostimVisitorAllowFemale);
		if (Settings::verboseLog) {
			logs::info("OStim visitor add-on {} (male {}, female {})", Settings::ostimVisitorEnabled.load() ? "enabled" : "disabled",
				Settings::ostimVisitorAllowMale.load() ? "on" : "off", Settings::ostimVisitorAllowFemale.load() ? "on" : "off");
		}
	});
}

void Settings::ResetToDefaults()
{
	enabled = true;
	blockDrainInfection = false;
	visitChanceSafe = 10.0f;
	wildChanceEnabled = true;
	visitChanceWild = 20.0f;
	biteInfectionChance = 50.0f;
	drainedHours = 24.0f;
	biteMark = true;
	requireUninterruptedSleep = false;
	nightOnly = true;
	nightStartHour = 20.0f;
	nightEndHour = 6.0f;
	showNotification = true;
	drainWindowSeconds = 5.0f;
	verboseLog = true;
	ostimVisitorEnabled = true;
	ostimVisitorAllowMale = true;
	ostimVisitorAllowFemale = true;
	Save();
}
