#pragma once

// Settings are edited from the SKSE Menu Framework page (render thread) and read by the game hooks
// (main thread), so every field the hooks read is atomic.
namespace Settings
{
	inline std::atomic<bool>  enabled{ true };
	inline std::atomic<bool>  blockDrainInfection{ true };
	inline std::atomic<float> sleepChanceSafe{ 0.5f };  // inns, homes, towns (LocTypeDwelling / LocTypeHabitation)
	inline std::atomic<bool>  wildChanceEnabled{ true };  // false = every sleep uses sleepChanceSafe
	inline std::atomic<float> sleepChanceWild{ 2.0f };     // camps, caves, ruins, wilderness
	inline std::atomic<bool>  requireUninterruptedSleep{ false };
	inline std::atomic<bool>  showNotification{ true };
	inline std::atomic<float> drainWindowSeconds{ 5.0f };
	inline std::atomic<bool>  verboseLog{ true };
	inline std::string        notificationText{ "You have contracted Sanguinare Vampiris." };

	void Load();
	void Save();
	void ResetToDefaults();
}

// Chance used for a sleep in a safe place or in the wild.
inline float SleepChance(bool a_wild)
{
	return a_wild && Settings::wildChanceEnabled ? Settings::sleepChanceWild.load() : Settings::sleepChanceSafe.load();
}

// Session statistics shown on the menu page.
namespace Stats
{
	inline std::atomic<int>   sleepRolls{ 0 };
	inline std::atomic<int>   sleepInfections{ 0 };
	inline std::atomic<int>   drainBlocks{ 0 };
	inline std::atomic<float> lastRoll{ -1.0f };
	inline std::atomic<float> lastChance{ 0.0f };
	inline std::atomic<bool>  lastRollWild{ false };
	inline std::atomic<bool>  lastRollInfected{ false };
}

namespace UI
{
	void Register();
}
