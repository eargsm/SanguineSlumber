// Sanguine Slumber - SKSE plugin for Skyrim (SE / AE / VR)
// Copyright (C) 2026 Rain (eargsm)
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program
// (LICENSE / LICENSE.txt). If not, see <https://www.gnu.org/licenses/>.
// Additional permissions apply; see EXCEPTIONS.
// Source code: https://github.com/eargsm/SanguineSlumber

// Sanguine Slumber
//  1. Vampiric Drain can no longer infect the player with Sanguinare Vampiris.
//  2. Instead, every time the player sleeps there is a small chance to contract it.
//
// Vanilla reference (verified in Skyrim.esm / Dawnguard.esm):
//  - Every vampire Drain Health MGEF carries VampireAbsorbHealthScript, whose OnEffectStart does
//    Game.GetPlayer().DoCombatSpellApply(DiseaseSanguinareVampiris [SPEL Skyrim.esm:0B8780], Player).
//    That apply lands in MagicTarget::AddTarget on the player, a few frames after the drain effect itself.
//  - The Redwater Den bloodspring and DLC1TrapInfectedBones use a DIFFERENT spell
//    (TrapDiseaseSanguinareVampiris, Dawnguard.esm:0037E9), so they are never touched here.
//  - Vanilla sleep bonuses (PlayerSleepQuestScript) are granted with AddSpell from OnSleepStop;
//    the sleep infection mirrors that, and the disease's own VampireDiseaseEffectScript
//    then runs the normal 3-day progression to vampirism.
//
// Block rule: the 0B8780 disease is refused only while a vampire drain hit the player within the last
// fDrainWindowSeconds. Save loads, quest scripts and our own sleep infection are outside that window.

#include "Settings.h"

namespace RE
{
	// CommonLibSSE-NG only forward-declares this one. Layout matches Papyrus
	// OnSleepStart(float afSleepStartTime, float afDesiredSleepEndTime); we only use it as a signal.
	struct TESSleepStartEvent
	{
		float startTime;  // game days passed
		float endTime;
	};
}

namespace
{
	RE::SpellItem*                  g_sanguinare{ nullptr };          // Skyrim.esm:0B8780 DiseaseSanguinareVampiris
	RE::EffectSetting*              g_sanguinareEffect{ nullptr };    // Skyrim.esm:0B8778 (its Reduced Health effect)
	RE::EffectSetting*              g_trapSanguinareEffect{ nullptr };  // Dawnguard.esm:0037EC (bloodspring / trap version)
	RE::BGSKeyword*                 g_vampireKeyword{ nullptr };      // Skyrim.esm:0A82BB Vampire
	std::vector<RE::EffectSetting*> g_drainEffects;
	// Safe-place keywords. Verified on vanilla LCTN records: every inn/home/temple/guild/castle carries
	// LocTypeDwelling; town, city and settlement grounds carry LocTypeHabitation; dungeons, caves,
	// vampire lairs and forts carry neither. LocTypePlayerHouse covers player homes from other masters.
	std::vector<RE::BGSKeyword*>    g_safeLocationKeywords;

	using clock = std::chrono::steady_clock;
	clock::time_point g_lastDrainHit{};
	bool              g_drainSeen{ false };
	bool              g_grantingFromSleep{ false };
	float             g_sleepStartHour{ -1.0f };  // in-game hour (0-24) when the current sleep began, -1 = unknown

	bool IsVampireDrain(const RE::EffectSetting* a_effect, RE::TESObjectREFR* a_caster)
	{
		if (!a_effect) {
			return false;
		}
		if (std::ranges::find(g_drainEffects, a_effect) != g_drainEffects.end()) {
			return true;
		}
		// Modded vampires: any Absorb-Health effect cast by an actor whose race is a vampire race.
		if (a_effect->GetArchetype() != RE::EffectSetting::Archetype::kAbsorb ||
			a_effect->data.primaryAV != RE::ActorValue::kHealth) {
			return false;
		}
		const auto caster = a_caster ? a_caster->As<RE::Actor>() : nullptr;
		const auto race = caster ? caster->GetRace() : nullptr;
		return race && g_vampireKeyword && race->HasKeyword(g_vampireKeyword);
	}

	// Safe = settled place (inn, home, temple, town grounds); wild = camps, caves, ruins, forts, wilderness.
	// Vanilla analogue: PlayerSleepQuestScript decides Well Rested from the location's LocTypeInn/LocTypePlayerHouse.
	bool IsSafeSleepPlace(RE::PlayerCharacter* a_player, std::string& a_where)
	{
		const auto location = a_player->GetCurrentLocation();
		const auto cell = a_player->GetParentCell();
		if (location) {
			a_where = location->GetName();
			for (const auto keyword : g_safeLocationKeywords) {
				if (location->HasKeyword(keyword)) {
					return true;
				}
			}
			return false;
		}
		// Many modded player homes have no location: treat an interior with no location as safe,
		// an exterior with no location (open wilderness) as wild.
		a_where = cell && cell->GetName() && *cell->GetName() ? cell->GetName() : "no location";
		return cell && cell->IsInteriorCell();
	}

	// A caught disease is an ACTIVE EFFECT, not a spell-list entry: DoCombatSpellApply leaves HasSpell()==0.
	// (Verified in-game: AddSpell on a disease only adds an inert spell-list entry, its effect never starts.)
	bool IsInfected(RE::PlayerCharacter* a_player)
	{
		auto target = a_player->AsMagicTarget();
		return (g_sanguinareEffect && target->HasMagicEffect(g_sanguinareEffect)) ||
		       (g_trapSanguinareEffect && target->HasMagicEffect(g_trapSanguinareEffect));
	}

	void CastSanguinareOnPlayer(RE::PlayerCharacter* a_player)
	{
		auto caster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		if (!caster) {
			logs::warn("Player has no instant magic caster - cannot apply Sanguinare Vampiris");
			return;
		}
		g_grantingFromSleep = true;
		caster->CastSpellImmediate(g_sanguinare, true, a_player, 1.0f, false, 0.0f, a_player);
		g_grantingFromSleep = false;
	}

	// Tell add-ons (Papyrus: RegisterForModEvent("SanguineSlumber_SleepInfected", ...)) that a sleep roll infected
	// the player. strArg = "safe" or "wild", numArg = 1.0 for wild, sender = the Sanguinare Vampiris spell.
	void SendSleepInfectedEvent(bool a_wild)
	{
		SKSE::ModCallbackEvent event{ "SanguineSlumber_SleepInfected", a_wild ? "wild" : "safe", a_wild ? 1.0f : 0.0f, g_sanguinare };
		if (auto source = SKSE::GetModCallbackEventSource()) {
			source->SendEvent(&event);
			logs::info("Sent SanguineSlumber_SleepInfected ({})", a_wild ? "wild" : "safe");
		}
	}

	// Casting a disease carries the engine's own contraction chance on every cast (vanilla
	// VampireAbsorbHealthScript: "percent chance exists on the disease"). Measured in game: ~3 successes in
	// ~20 casts, independent of bed/menu/CommonLib. Our roll already decided the player is infected, so keep
	// casting until the effect takes: 60 casts x 0.4 s gives > 99.99% at ~15% per cast; failed casts show
	// nothing. We also wait until the game is unpaused and the player is standing, like vanilla's
	// VampireDiseaseEffectScript waits for controls before acting.
	constexpr int   kMaxWaitPolls = 120;  // x 0.5 s = 60 s to get up
	constexpr int   kMaxCasts = 60;
	constexpr auto  kPollInterval = std::chrono::milliseconds(500);
	constexpr auto  kRecastInterval = std::chrono::milliseconds(400);

	bool PlayerIsFree(RE::PlayerCharacter* a_player)
	{
		auto ui = RE::UI::GetSingleton();
		if (ui && ui->GameIsPaused()) {
			return false;
		}
		auto state = a_player->AsActorState();
		return state && state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
	}

	void ScheduleInfectStep(int a_poll, int a_cast, bool a_wild, std::chrono::milliseconds a_delay);

	// One main-thread step: wait for the player to be free, cast, then confirm the effect started.
	void InfectStep(int a_poll, int a_cast, bool a_wild)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !g_sanguinare) {
			return;
		}
		if (a_cast > 0 && IsInfected(player)) {
			logs::info("Sanguinare Vampiris effect is ACTIVE on the player (cast {}, after {} waits)", a_cast, a_poll);
			if (Settings::showNotification && !Settings::notificationText.empty()) {
				RE::SendHUDMessage::ShowHUDMessage(Settings::notificationText.c_str(), nullptr, false);
			}
			SendSleepInfectedEvent(a_wild);
			return;
		}
		if (!PlayerIsFree(player)) {
			if (a_poll >= kMaxWaitPolls) {
				logs::warn("Player was not free to receive Sanguinare Vampiris within {} s - giving up", kMaxWaitPolls / 2);
				return;
			}
			ScheduleInfectStep(a_poll + 1, a_cast, a_wild, kPollInterval);
			return;
		}
		if (a_cast >= kMaxCasts) {
			logs::warn("Sanguinare Vampiris effect still not active after {} casts - giving up", a_cast);
			return;
		}
		if (a_cast > 0 && Settings::verboseLog && a_cast % 10 == 0) {
			logs::info("Sanguinare Vampiris not contracted after {} casts - still trying", a_cast);
		}
		CastSanguinareOnPlayer(player);
		ScheduleInfectStep(a_poll, a_cast + 1, a_wild, kRecastInterval);
	}

	void ScheduleInfectStep(int a_poll, int a_cast, bool a_wild, std::chrono::milliseconds a_delay)
	{
		std::thread([a_poll, a_cast, a_wild, a_delay]() {
			std::this_thread::sleep_for(a_delay);
			SKSE::GetTaskInterface()->AddTask([a_poll, a_cast, a_wild]() { InfectStep(a_poll, a_cast, a_wild); });
		}).detach();
	}

	void InfectFromSleep(bool a_wild)
	{
		ScheduleInfectStep(0, 0, a_wild, kPollInterval);
	}

	struct PlayerAddTarget
	{
		static bool thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data)
		{
			const auto effect = a_data.effect ? a_data.effect->baseEffect : nullptr;

			if (Settings::enabled && Settings::blockDrainInfection && g_sanguinare) {
				if (IsVampireDrain(effect, a_data.caster)) {
					// Concentration drain re-applies every ~0.16s; only log the start of each drain episode.
					const auto                         now = clock::now();
					const std::chrono::duration<float> gap = now - g_lastDrainHit;
					if (Settings::verboseLog && (!g_drainSeen || gap.count() > 1.0f)) {
						logs::info("Vampire drain started on the player (effect {:08X})", effect->GetFormID());
					}
					g_lastDrainHit = now;
					g_drainSeen = true;
				} else if (a_data.magicItem == g_sanguinare && !g_grantingFromSleep) {
					const std::chrono::duration<float> since = clock::now() - g_lastDrainHit;
					if (g_drainSeen && since.count() <= Settings::drainWindowSeconds) {
						logs::info("BLOCKED Sanguinare Vampiris from vampiric drain ({:.2f}s after drain hit)", since.count());
						++Stats::drainBlocks;
						return false;
					}
					if (Settings::verboseLog) {
						logs::info("Sanguinare Vampiris applied with no recent drain - allowed");
					}
				}
			}
			return func(a_this, a_data);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install()
		{
			// PlayerCharacter vtable [4] = its MagicTarget base; AddTarget is MagicTarget vfunc 0x01 (SE/AE/VR).
			REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[4] };
			func = vtbl.write_vfunc(0x1, thunk);
			logs::info("Hooked PlayerCharacter MagicTarget::AddTarget");
		}
	};

	// Remembers the in-game hour the player lay down, for the night-only rule.
	class SleepStartHandler : public RE::BSTEventSink<RE::TESSleepStartEvent>
	{
	public:
		static SleepStartHandler* GetSingleton()
		{
			static SleepStartHandler singleton;
			return &singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStartEvent*, RE::BSTEventSource<RE::TESSleepStartEvent>*) override
		{
			const auto calendar = RE::Calendar::GetSingleton();
			g_sleepStartHour = calendar ? calendar->GetHour() : -1.0f;
			if (Settings::verboseLog) {
				logs::info("Sleep started at hour {:.2f}", g_sleepStartHour);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	class SleepHandler : public RE::BSTEventSink<RE::TESSleepStopEvent>
	{
	public:
		static SleepHandler* GetSingleton()
		{
			static SleepHandler singleton;
			return &singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent* a_event, RE::BSTEventSource<RE::TESSleepStopEvent>*) override
		{
			if (!a_event || !g_sanguinare || !Settings::enabled) {
				return RE::BSEventNotifyControl::kContinue;
			}
			if (a_event->interrupted && Settings::requireUninterruptedSleep) {
				logs::info("Sleep interrupted - no infection roll");
				return RE::BSEventNotifyControl::kContinue;
			}
			const float startHour = std::exchange(g_sleepStartHour, -1.0f);
			if (Settings::nightOnly) {
				if (startHour < 0.0f) {
					logs::warn("Sleep start hour unknown - no infection roll (night-only is on)");
					return RE::BSEventNotifyControl::kContinue;
				}
				if (!IsNightHour(startHour)) {
					logs::info("Slept during the day (hour {:.2f}) - no infection roll", startHour);
					return RE::BSEventNotifyControl::kContinue;
				}
			}

			auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto race = player->GetRace();
			if (race && g_vampireKeyword && race->HasKeyword(g_vampireKeyword)) {
				logs::info("Player is already a vampire - no infection roll");
				return RE::BSEventNotifyControl::kContinue;
			}
			if (IsInfected(player)) {
				logs::info("Player already has Sanguinare Vampiris - no infection roll");
				return RE::BSEventNotifyControl::kContinue;
			}
			// Same immunity the game respects (e.g. werewolf beast blood, 100% disease resist).
			if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kResistDisease) >= 100.0f) {
				logs::info("Player is immune to disease - no infection roll");
				return RE::BSEventNotifyControl::kContinue;
			}

			static std::mt19937                   rng{ std::random_device{}() };
			std::uniform_real_distribution<float> roll{ 0.0f, 100.0f };
			std::string                           where;
			const bool                            wild = !IsSafeSleepPlace(player, where);
			const float                           chance = SleepChance(wild);
			const float                           result = roll(rng);
			const bool                            infected = result < chance;
			logs::info("Sleep infection roll in {} ({}): {:.3f} vs chance {:.2f}% (interrupted={}) -> {}",
				where, wild ? "wild" : "safe", result, chance, a_event->interrupted, infected ? "INFECTED" : "safe");

			++Stats::sleepRolls;
			Stats::lastRoll = result;
			Stats::lastChance = chance;
			Stats::lastRollWild = wild;
			Stats::lastRollInfected = infected;
			if (infected) {
				++Stats::sleepInfections;
				InfectFromSleep(wild);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void OnDataLoaded()
	{
		g_sanguinare = RE::TESForm::LookupByID<RE::SpellItem>(0x000B8780);
		g_vampireKeyword = RE::TESForm::LookupByID<RE::BGSKeyword>(0x000A82BB);
		g_sanguinareEffect = RE::TESForm::LookupByID<RE::EffectSetting>(0x000B8778);

		auto dataHandler = RE::TESDataHandler::GetSingleton();
		g_trapSanguinareEffect = dataHandler ? dataHandler->LookupForm<RE::EffectSetting>(0x0037EC, "Dawnguard.esm") : nullptr;
		const std::pair<RE::FormID, std::string_view> drains[] = {
			{ 0x08D5C5, "Skyrim.esm" },     // crVampireAbsorbHealthConcAimed
			{ 0x011F2B, "Dawnguard.esm" },  // DLC1SeranaAbsorbHealthConcAimed
			{ 0x00FC75, "Dawnguard.esm" },  // crdunHarkonVampireAbsorbHealthConcAimed
			{ 0x008B31, "Dawnguard.esm" },  // crVampireAbsorbHealthConcAimedCOPY0000
		};
		for (const auto& [id, mod] : drains) {
			if (auto mgef = dataHandler ? dataHandler->LookupForm<RE::EffectSetting>(id, mod) : nullptr) {
				g_drainEffects.push_back(mgef);
			}
		}

		for (const RE::FormID id : { 0x000130DCu /* LocTypeDwelling */, 0x00039793u /* LocTypeHabitation */,
				 0x000FC1A3u /* LocTypePlayerHouse */ }) {
			if (auto keyword = RE::TESForm::LookupByID<RE::BGSKeyword>(id)) {
				g_safeLocationKeywords.push_back(keyword);
			}
		}

		logs::info("Sanguinare Vampiris spell: {}  effect: {}  trap effect: {}  Vampire keyword: {}  drain effects found: {}/4  "
				   "safe-place keywords: {}/3",
			g_sanguinare ? "found" : "MISSING", g_sanguinareEffect ? "found" : "MISSING", g_trapSanguinareEffect ? "found" : "MISSING",
			g_vampireKeyword ? "found" : "MISSING", g_drainEffects.size(), g_safeLocationKeywords.size());

		if (auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESSleepStartEvent>(SleepStartHandler::GetSingleton());
			holder->AddEventSink<RE::TESSleepStopEvent>(SleepHandler::GetSingleton());
			logs::info("Registered sleep listener");
		}

		Addons::Detect();
		Addons::ApplyToggles();
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	logs::info("Sanguine Slumber loading");
	Settings::Load();
	PlayerAddTarget::Install();

	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			// After every SKSE plugin has loaded, so SKSEMenuFramework.dll is guaranteed to be present:
			// the framework header caches each function lookup on first call, including a failed one.
			UI::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			OnDataLoaded();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			// A save restores the add-on globals' saved values; put the INI toggles back on top.
			Addons::ApplyToggles();
			break;
		default:
			break;
		}
	});

	return true;
}
