#include "Cast.h"
#include "Settings.h"

namespace RE
{
	struct TESSleepStartEvent
	{
		float startTime;
		float endTime;
	};
}

namespace
{
	RE::SpellItem*                  g_sanguinare{ nullptr };
	RE::EffectSetting*              g_sanguinareEffect{ nullptr };
	RE::EffectSetting*              g_trapSanguinareEffect{ nullptr };
	RE::BGSKeyword*                 g_vampireKeyword{ nullptr };
	std::vector<RE::EffectSetting*> g_drainEffects;
	std::vector<RE::BGSKeyword*>    g_safeLocationKeywords;

	using clock = std::chrono::steady_clock;
	clock::time_point g_lastDrainHit{};
	bool              g_drainSeen{ false };
	bool              g_grantingFromSleep{ false };
	float             g_sleepStartHour{ -1.0f };

	bool IsVampireDrain(const RE::EffectSetting* a_effect, RE::TESObjectREFR* a_caster)
	{
		if (!a_effect) {
			return false;
		}
		if (std::ranges::find(g_drainEffects, a_effect) != g_drainEffects.end()) {
			return true;
		}
		if (a_effect->GetArchetype() != RE::EffectSetting::Archetype::kAbsorb ||
			a_effect->data.primaryAV != RE::ActorValue::kHealth) {
			return false;
		}
		const auto caster = a_caster ? a_caster->As<RE::Actor>() : nullptr;
		const auto race = caster ? caster->GetRace() : nullptr;
		return race && g_vampireKeyword && race->HasKeyword(g_vampireKeyword);
	}

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
		a_where = cell && cell->GetName() && *cell->GetName() ? cell->GetName() : "no location";
		return cell && cell->IsInteriorCell();
	}

	bool IsInfected(RE::PlayerCharacter* a_player)
	{
		auto target = a_player->AsMagicTarget();
		return (g_sanguinareEffect && target->HasMagicEffect(g_sanguinareEffect)) ||
		       (g_trapSanguinareEffect && target->HasMagicEffect(g_trapSanguinareEffect));
	}

	void CastSanguinareOnPlayer(RE::PlayerCharacter* a_player)
	{
		g_grantingFromSleep = true;
		Cast::OnPlayer(a_player, g_sanguinare);
		g_grantingFromSleep = false;
	}

	void SendSleepInfectedEvent(bool a_wild)
	{
		SKSE::ModCallbackEvent event{ "SanguineSlumber_SleepInfected", a_wild ? "wild" : "safe", a_wild ? 1.0f : 0.0f, g_sanguinare };
		if (auto source = SKSE::GetModCallbackEventSource()) {
			source->SendEvent(&event);
			logs::info("Sent SanguineSlumber_SleepInfected ({})", a_wild ? "wild" : "safe");
		}
	}

	void InfectFromSleep(bool a_wild)
	{
		auto job = std::make_shared<Cast::Job>();
		job->name = "Sanguinare Vampiris";
		job->cast = CastSanguinareOnPlayer;
		job->isActive = IsInfected;
		job->onActive = [a_wild](int) {
			if (Settings::showNotification && !Settings::notificationText.empty()) {
				RE::SendHUDMessage::ShowHUDMessage(Settings::notificationText.c_str(), nullptr, false);
			}
			SendSleepInfectedEvent(a_wild);
		};
		Cast::Start(job);
	}

	float Roll()
	{
		static std::mt19937                   rng{ std::random_device{}() };
		std::uniform_real_distribution<float> roll{ 0.0f, 100.0f };
		return roll(rng);
	}

	void Visit(bool a_wild, std::optional<bool> a_forceInfect)
	{
		++Stats::visits;
		auto        player = RE::PlayerCharacter::GetSingleton();
		bool        infect = false;
		if (player && IsInfected(player)) {
			logs::info("Already has Sanguinare Vampiris - no bite infection roll");
		} else if (a_forceInfect) {
			infect = *a_forceInfect;
			logs::info("Test visit: bite {}", infect ? "infects" : "does not infect");
		} else {
			const float chance = Settings::biteInfectionChance.load();
			const float result = Roll();
			infect = result < chance;
			logs::info("Bite infection roll: {:.3f} vs chance {:.2f}% -> {}", result, chance, infect ? "INFECTED" : "not infected");
		}
		Stats::lastRollInfected = infect;
		if (infect) {
			++Stats::sleepInfections;
		}
		Feeding::Feed(a_wild, [a_wild, infect]() {
			if (infect) {
				InfectFromSleep(a_wild);
			}
		});
	}

	void PapyrusTestVisit(RE::StaticFunctionTag*, bool a_infect)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		std::string where;
		Visit(player && !IsSafeSleepPlace(player, where), a_infect);
	}

	RE::BSFixedString PapyrusGetStatus(RE::StaticFunctionTag*)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		return std::format("drained={} remainingHours={:.2f} infected={} raceMenu={} plugin={}", player && Feeding::IsDrained(player),
			Feeding::RemainingHours(), player && IsInfected(player), Feeding::raceMenuPresent.load(), Feeding::Available());
	}

	bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("TestVisit", "SanguineSlumber", PapyrusTestVisit);
		a_vm->RegisterFunction("GetStatus", "SanguineSlumber", PapyrusGetStatus);
		return true;
	}

	struct PlayerAddTarget
	{
		static bool thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data)
		{
			const auto effect = a_data.effect ? a_data.effect->baseEffect : nullptr;

			if (Settings::enabled && Settings::blockDrainInfection && g_sanguinare) {
				if (IsVampireDrain(effect, a_data.caster)) {
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
			REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[4] };
			func = vtbl.write_vfunc(0x1, thunk);
			logs::info("Hooked PlayerCharacter MagicTarget::AddTarget");
		}
	};

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
				logs::info("Sleep interrupted - no visit roll");
				return RE::BSEventNotifyControl::kContinue;
			}
			const float startHour = std::exchange(g_sleepStartHour, -1.0f);
			if (Settings::nightOnly) {
				if (startHour < 0.0f) {
					logs::warn("Sleep start hour unknown - no visit roll (night-only is on)");
					return RE::BSEventNotifyControl::kContinue;
				}
				if (!IsNightHour(startHour)) {
					logs::info("Slept during the day (hour {:.2f}) - no visit roll", startHour);
					return RE::BSEventNotifyControl::kContinue;
				}
			}

			auto player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto race = player->GetRace();
			if (race && g_vampireKeyword && race->HasKeyword(g_vampireKeyword)) {
				logs::info("Player is already a vampire - no visit roll");
				return RE::BSEventNotifyControl::kContinue;
			}
			if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kResistDisease) >= 100.0f) {
				logs::info("Player is immune to disease - no visit roll");
				return RE::BSEventNotifyControl::kContinue;
			}

			std::string where;
			const bool  wild = !IsSafeSleepPlace(player, where);
			const float chance = VisitChance(wild);
			const float result = Roll();
			const bool  visited = result < chance;
			logs::info("Sleep visit roll in {} ({}): {:.3f} vs chance {:.2f}% (interrupted={}) -> {}",
				where, wild ? "wild" : "safe", result, chance, a_event->interrupted, visited ? "VISITED" : "no visit");

			++Stats::sleepRolls;
			Stats::lastRoll = result;
			Stats::lastChance = chance;
			Stats::lastRollWild = wild;
			Stats::lastRollVisited = visited;
			Stats::lastRollInfected = false;
			if (visited) {
				Visit(wild, std::nullopt);
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
			{ 0x08D5C5, "Skyrim.esm" },
			{ 0x011F2B, "Dawnguard.esm" },
			{ 0x00FC75, "Dawnguard.esm" },
			{ 0x008B31, "Dawnguard.esm" },
		};
		for (const auto& [id, mod] : drains) {
			if (auto mgef = dataHandler ? dataHandler->LookupForm<RE::EffectSetting>(id, mod) : nullptr) {
				g_drainEffects.push_back(mgef);
			}
		}

		for (const RE::FormID id : { 0x000130DCu , 0x00039793u ,
				 0x000FC1A3u  }) {
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

		Feeding::Init();
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
	SKSE::GetPapyrusInterface()->Register(RegisterPapyrus);

	SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			UI::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			OnDataLoaded();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Addons::ApplyToggles();
			Feeding::OnGameLoaded();
			break;
		default:
			break;
		}
	});

	return true;
}
