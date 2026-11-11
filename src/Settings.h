#pragma once

namespace Settings
{
	inline std::atomic<bool>  enabled{ true };
	inline std::atomic<bool>  blockDrainInfection{ false };
	inline std::atomic<float> visitChanceSafe{ 10.0f };
	inline std::atomic<bool>  wildChanceEnabled{ true };
	inline std::atomic<float> visitChanceWild{ 20.0f };
	inline std::atomic<float> biteInfectionChance{ 50.0f };
	inline std::atomic<bool>  biteMark{ true };
	inline std::atomic<float> drainedHours{ 24.0f };
	inline std::atomic<bool>  requireUninterruptedSleep{ false };
	inline std::atomic<bool>  nightOnly{ true };
	inline std::atomic<float> nightStartHour{ 20.0f };
	inline std::atomic<float> nightEndHour{ 6.0f };
	inline std::atomic<bool>  showNotification{ true };
	inline std::atomic<float> drainWindowSeconds{ 5.0f };
	inline std::atomic<bool>  verboseLog{ true };
	inline std::atomic<bool>  ostimVisitorEnabled{ true };
	inline std::atomic<bool>  ostimVisitorAllowMale{ true };
	inline std::atomic<bool>  ostimVisitorAllowFemale{ true };
	inline std::string        notificationText{ "You have contracted Sanguinare Vampiris." };
	inline std::string        visitText{ "You wake up weak and pale. Two small wounds mark your neck." };

	void Load();
	void Save();
	void ResetToDefaults();
}

inline float VisitChance(bool a_wild)
{
	return a_wild && Settings::wildChanceEnabled ? Settings::visitChanceWild.load() : Settings::visitChanceSafe.load();
}

inline bool IsNightHour(float a_hour)
{
	const float start = Settings::nightStartHour.load();
	const float end = Settings::nightEndHour.load();
	return start > end ? (a_hour >= start || a_hour < end) : (a_hour >= start && a_hour < end);
}

namespace Stats
{
	inline std::atomic<int>   sleepRolls{ 0 };
	inline std::atomic<int>   visits{ 0 };
	inline std::atomic<int>   sleepInfections{ 0 };
	inline std::atomic<int>   drainBlocks{ 0 };
	inline std::atomic<float> lastRoll{ -1.0f };
	inline std::atomic<float> lastChance{ 0.0f };
	inline std::atomic<bool>  lastRollWild{ false };
	inline std::atomic<bool>  lastRollVisited{ false };
	inline std::atomic<bool>  lastRollInfected{ false };
}

namespace Addons
{
	inline constexpr std::string_view kOStimVisitorPlugin = "SanguineSlumber_OStimVisitor.esp";
	inline constexpr RE::FormID       kOStimVisitorEnabledGlobal = 0x800;
	inline constexpr RE::FormID       kOStimVisitorAllowMaleGlobal = 0x804;
	inline constexpr RE::FormID       kOStimVisitorAllowFemaleGlobal = 0x805;
	inline std::atomic<bool>          ostimVisitorPresent{ false };

	void Detect();
	void ApplyToggles();
}

namespace Feeding
{
	inline constexpr std::string_view kPlugin = "SanguineSlumber.esp";
	inline constexpr RE::FormID       kDrainedSpell = 0x800;
	inline constexpr RE::FormID       kDrainedEffect = 0x801;
	inline constexpr RE::FormID       kDrainedUntil = 0x805;
	inline std::atomic<bool>          raceMenuPresent{ false };

	void Init();
	void OnGameLoaded();
	bool Available();
	bool IsDrained(RE::Actor* a_actor);
	void Feed(bool a_wild, std::function<void()> a_then);
	void RefreshMark();
	float RemainingHours();
}

namespace UI
{
	void Register();
}
