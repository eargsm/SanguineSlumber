#include "Cast.h"
#include "Settings.h"

namespace
{
	RE::SpellItem*     g_drained{ nullptr };
	RE::EffectSetting* g_drainedEffect{ nullptr };
	RE::TESGlobal*     g_until{ nullptr };
	constexpr auto     kOverlayScript = "SanguineSlumber_Overlay";

	class PapyrusCallback : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		explicit PapyrusCallback(std::function<void(const RE::BSScript::Variable&)> a_fn) :
			_fn(std::move(a_fn)) {}

		void operator()(RE::BSScript::Variable a_result) override
		{
			if (_fn) {
				_fn(a_result);
			}
		}
		void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

	private:
		std::function<void(const RE::BSScript::Variable&)> _fn;
	};

	void CallOverlay(const char* a_function, RE::BSScript::IFunctionArguments* a_args,
		std::function<void(const RE::BSScript::Variable&)> a_onResult = nullptr)
	{
		auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			logs::error("No Papyrus VM - cannot call {}.{}", kOverlayScript, a_function);
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		if (a_onResult) {
			callback.reset(new PapyrusCallback(std::move(a_onResult)));
		}
		if (!vm->DispatchStaticCall(kOverlayScript, a_function, a_args, callback)) {
			logs::error("Papyrus call {}.{} failed (is SanguineSlumber_Overlay.pex installed?)", kOverlayScript, a_function);
		}
	}

	bool IsBeastHead(RE::PlayerCharacter* a_player)
	{
		const auto race = a_player->GetRace();
		if (!race) {
			return false;
		}
		switch (race->GetFormID()) {
		case 0x00013745:
		case 0x00088845:
		case 0x00013740:
		case 0x0008883A:
			return true;
		default:
			break;
		}
		std::string name = race->GetName() ? race->GetName() : "";
		std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return name.find("khajiit") != std::string::npos || name.find("argonian") != std::string::npos;
	}

	void ApplyMark()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !Settings::biteMark || !Feeding::raceMenuPresent) {
			return;
		}
		if (IsBeastHead(player)) {
			logs::info("No bite mark: it is drawn for human, elf and orc heads only");
			return;
		}
		const auto base = player->GetActorBase();
		const bool female = base && base->GetSex() == RE::SEX::kFemale;
		const char* texture = female ? "actors\\character\\SanguineSlumber\\BiteMark_f.dds" : "actors\\character\\SanguineSlumber\\BiteMark_m.dds";
		CallOverlay("Apply", RE::MakeFunctionArguments(static_cast<RE::Actor*>(player), RE::BSFixedString{ texture }),
			[texture](const RE::BSScript::Variable& a_result) {
				const auto slot = a_result.IsInt() ? a_result.GetSInt() : -1;
				if (slot < 0) {
					logs::warn("Bite mark not shown: every RaceMenu face overlay slot is in use");
				} else {
					logs::info("Bite mark applied in face overlay slot {} ({})", slot, texture);
				}
			});
	}

	void ClearMark(const char* a_why)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || !Feeding::raceMenuPresent) {
			return;
		}
		std::string why = a_why;
		CallOverlay("Clear", RE::MakeFunctionArguments(static_cast<RE::Actor*>(player)), [why](const RE::BSScript::Variable& a_result) {
			if (a_result.IsBool() && a_result.GetBool()) {
				logs::info("Bite mark removed ({})", why);
			}
		});
	}

	float DaysPassed()
	{
		const auto calendar = RE::Calendar::GetSingleton();
		return calendar ? calendar->GetDaysPassed() : 0.0f;
	}

	void DispelDrained(RE::PlayerCharacter* a_player)
	{
		if (auto list = a_player->AsMagicTarget()->GetActiveEffectList()) {
			for (auto effect : *list) {
				if (effect && effect->spell == g_drained) {
					effect->Dispel(true);
				}
			}
		}
	}

	void Tick()
	{
		if (!g_until || g_until->value <= 0.0f) {
			return;
		}
		auto player = RE::PlayerCharacter::GetSingleton();
		auto ui = RE::UI::GetSingleton();
		if (!player || !player->Is3DLoaded() || (ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))) {
			return;
		}
		if (!Feeding::IsDrained(player)) {
			g_until->value = 0.0f;
			logs::info("Drained is gone before its time (cured)");
			ClearMark("cured");
			return;
		}
		if (DaysPassed() >= g_until->value) {
			DispelDrained(player);
			g_until->value = 0.0f;
			logs::info("Drained wore off");
			ClearMark("wore off");
		}
	}

	void SendFedEvent(bool a_wild)
	{
		SKSE::ModCallbackEvent event{ "SanguineSlumber_Fed", a_wild ? "wild" : "safe", a_wild ? 1.0f : 0.0f, g_drained };
		if (auto source = SKSE::GetModCallbackEventSource()) {
			source->SendEvent(&event);
		}
	}

	void OnDrained(bool a_wild)
	{
		g_until->value = DaysPassed() + Settings::drainedHours.load() / 24.0f;
		logs::info("Drained until game day {:.3f} ({:.1f} h)", g_until->value, Settings::drainedHours.load());
		ApplyMark();
		if (Settings::showNotification && !Settings::visitText.empty()) {
			RE::SendHUDMessage::ShowHUDMessage(Settings::visitText.c_str(), nullptr, false);
		}
		SendFedEvent(a_wild);
	}
}

void Feeding::Init()
{
	auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (dataHandler) {
		g_drained = dataHandler->LookupForm<RE::SpellItem>(kDrainedSpell, kPlugin);
		g_drainedEffect = dataHandler->LookupForm<RE::EffectSetting>(kDrainedEffect, kPlugin);
		g_until = dataHandler->LookupForm<RE::TESGlobal>(kDrainedUntil, kPlugin);
	}
	raceMenuPresent = REX::W32::GetModuleHandleW(L"skee64.dll") || REX::W32::GetModuleHandleW(L"skeevr.dll");
	logs::info("Feeding: {} (Drained spell {}, effect {}, timer {}); RaceMenu {}", kPlugin, g_drained ? "found" : "MISSING",
		g_drainedEffect ? "found" : "MISSING", g_until ? "found" : "MISSING",
		raceMenuPresent.load() ? "installed - bite marks on" : "not installed - no bite marks");

	std::thread([]() {
		for (;;) {
			std::this_thread::sleep_for(std::chrono::seconds(2));
			SKSE::GetTaskInterface()->AddTask(Tick);
		}
	}).detach();
}

bool Feeding::Available()
{
	return g_drained && g_drainedEffect && g_until;
}

bool Feeding::IsDrained(RE::Actor* a_actor)
{
	return g_drainedEffect && a_actor && a_actor->AsMagicTarget()->HasMagicEffect(g_drainedEffect);
}

float Feeding::RemainingHours()
{
	return g_until && g_until->value > 0.0f ? (std::max)(0.0f, (g_until->value - DaysPassed()) * 24.0f) : 0.0f;
}

void Feeding::Feed(bool a_wild, std::function<void()> a_then)
{
	if (!Available()) {
		logs::warn("{} not loaded (enabled in the load order?) - a vampire fed, but there is no Drained debuff or bite mark", kPlugin);
		SendFedEvent(a_wild);
		if (a_then) {
			a_then();
		}
		return;
	}
	auto player = RE::PlayerCharacter::GetSingleton();
	const bool already = player && IsDrained(player);
	if (already) {
		logs::info("Already Drained - the debuff starts over");
	}
	auto job = std::make_shared<Cast::Job>();
	job->name = "Drained";
	job->cast = [already](RE::PlayerCharacter* a_player) {
		if (!already) {
			Cast::OnPlayer(a_player, g_drained);
		}
	};
	job->isActive = IsDrained;
	job->onActive = [a_wild, a_then](int) {
		OnDrained(a_wild);
		if (a_then) {
			a_then();
		}
	};
	Cast::Start(job);
}

void Feeding::RefreshMark()
{
	SKSE::GetTaskInterface()->AddTask([]() {
		auto player = RE::PlayerCharacter::GetSingleton();
		if (player && Settings::biteMark && IsDrained(player)) {
			ApplyMark();
		} else {
			ClearMark(Settings::biteMark ? "not Drained" : "bite mark turned off");
		}
	});
}

void Feeding::OnGameLoaded()
{
	if (!Available() || !raceMenuPresent) {
		return;
	}
	std::thread([]() {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		RefreshMark();
	}).detach();
}
