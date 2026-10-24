#include "Settings.h"

// Settings live in Data/SKSE/Plugins/SanguineSlumber.ini. The in-game menu writes changes straight back
// to that file, so the INI keeps working on its own if SKSE Menu Framework is not installed.

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
		logs::info("{}: enabled={} blockDrain={} chanceSafe={}% separateWild={} chanceWild={}% "
				   "requireUninterrupted={} notify={} drainWindow={}s verbose={}",
			a_prefix, Settings::enabled.load(), Settings::blockDrainInfection.load(), Settings::sleepChanceSafe.load(),
			Settings::wildChanceEnabled.load(), Settings::sleepChanceWild.load(), Settings::requireUninterruptedSleep.load(),
			Settings::showNotification.load(), Settings::drainWindowSeconds.load(), Settings::verboseLog.load());
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
			} else if (key == "fSleepChanceSafe" || key == "fSleepInfectionChance") {  // old single-chance key = safe chance
				sleepChanceSafe = std::clamp(std::stof(value), 0.0f, 100.0f);
			} else if (key == "bSeparateWildChance") {
				wildChanceEnabled = ParseBool(value);
			} else if (key == "fSleepChanceWild") {
				sleepChanceWild = std::clamp(std::stof(value), 0.0f, 100.0f);
			} else if (key == "bRequireUninterruptedSleep") {
				requireUninterruptedSleep = ParseBool(value);
			} else if (key == "bShowNotification") {
				showNotification = ParseBool(value);
			} else if (key == "sNotificationText") {
				notificationText = value;
			} else if (key == "fDrainWindowSeconds") {
				drainWindowSeconds = std::clamp(std::stof(value), 0.5f, 60.0f);
			} else if (key == "bVerboseLog") {
				verboseLog = ParseBool(value);
			}
		} catch (const std::exception&) {
			logs::warn("Bad value for {}: '{}', keeping default", key, value);
		}
	}
	LogSettings("Settings loaded");
}

void Settings::Save()
{
	// Written in place (no temp file + rename): under MO2's virtual file system an in-place write
	// lands in the mod's own folder, while a newly created file would be redirected to overwrite/.
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
			   "; true = a vampire's Vampiric Drain can no longer give you Sanguinare Vampiris.\n"
			<< "bBlockDrainInfection = " << Bool(blockDrainInfection) << "\n"
			<< "\n"
			   "; Percent chance (0 - 100) to contract Sanguinare Vampiris each time you sleep. Default 0.5\n"
			<< "fSleepChanceSafe = " << std::format("{:.2f}", sleepChanceSafe.load()) << "\n"
			<< "\n"
			   "; true = sleeping in the wild uses its own chance below. Safe = inns, homes, temples, castles and\n"
			   "; town/city grounds (LocTypeDwelling / LocTypeHabitation / LocTypePlayerHouse, or an interior cell with\n"
			   "; no location). Wild = everywhere else: camps, caves, ruins, forts, wilderness. Defaults: true, 2.0\n"
			<< "bSeparateWildChance = " << Bool(wildChanceEnabled) << "\n"
			<< "fSleepChanceWild = " << std::format("{:.2f}", sleepChanceWild.load()) << "\n"
			<< "\n"
			   "; true = only roll after a full, uninterrupted sleep.\n"
			<< "bRequireUninterruptedSleep = " << Bool(requireUninterruptedSleep) << "\n"
			<< "\n"
			   "; Show a message when you catch it from sleeping.\n"
			<< "bShowNotification = " << Bool(showNotification) << "\n"
			<< "sNotificationText = " << notificationText << "\n"
			<< "\n"
			   "; How long (seconds) after a vampire's drain hits you its infection attempt is blocked.\n"
			   "; The vanilla drain script fires ~0.07s after the hit; 5 leaves room for script lag.\n"
			<< "fDrainWindowSeconds = " << std::format("{:.1f}", drainWindowSeconds.load()) << "\n"
			<< "\n"
			   "; Extra detail in Documents/My Games/Skyrim Special Edition/SKSE/SanguineSlumber.log\n"
			<< "bVerboseLog = " << Bool(verboseLog) << "\n";
		if (!out) {
			logs::error("Write to {} failed", kPath.string());
			return;
		}
	}
	LogSettings("Settings saved");
}

void Settings::ResetToDefaults()
{
	enabled = true;
	blockDrainInfection = true;
	sleepChanceSafe = 0.5f;
	wildChanceEnabled = true;
	sleepChanceWild = 2.0f;
	requireUninterruptedSleep = false;
	showNotification = true;
	drainWindowSeconds = 5.0f;
	verboseLog = true;
	Save();
}
