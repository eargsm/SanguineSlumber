#include "Cast.h"

#include "Settings.h"

namespace
{
	constexpr int  kMaxWaitPolls = 120;
	constexpr int  kMaxCasts = 60;
	constexpr auto kPollInterval = std::chrono::milliseconds(500);
	constexpr auto kRecastInterval = std::chrono::milliseconds(400);

	void Schedule(std::shared_ptr<Cast::Job> a_job, int a_poll, int a_cast, std::chrono::milliseconds a_delay);

	void Step(std::shared_ptr<Cast::Job> a_job, int a_poll, int a_cast)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		if (a_cast > 0 && a_job->isActive(player)) {
			logs::info("{} effect is ACTIVE on the player (cast {}, after {} waits)", a_job->name, a_cast, a_poll);
			a_job->onActive(a_cast);
			return;
		}
		if (!Cast::PlayerIsFree(player)) {
			if (a_poll >= kMaxWaitPolls) {
				logs::warn("Player was not free to receive {} within {} s - giving up", a_job->name, kMaxWaitPolls / 2);
				return;
			}
			Schedule(a_job, a_poll + 1, a_cast, kPollInterval);
			return;
		}
		if (a_cast >= kMaxCasts) {
			logs::warn("{} effect still not active after {} casts - giving up", a_job->name, a_cast);
			return;
		}
		if (a_cast > 0 && Settings::verboseLog && a_cast % 10 == 0) {
			logs::info("{} not contracted after {} casts - still trying", a_job->name, a_cast);
		}
		a_job->cast(player);
		Schedule(a_job, a_poll, a_cast + 1, kRecastInterval);
	}

	void Schedule(std::shared_ptr<Cast::Job> a_job, int a_poll, int a_cast, std::chrono::milliseconds a_delay)
	{
		std::thread([a_job, a_poll, a_cast, a_delay]() {
			std::this_thread::sleep_for(a_delay);
			SKSE::GetTaskInterface()->AddTask([a_job, a_poll, a_cast]() { Step(a_job, a_poll, a_cast); });
		}).detach();
	}
}

bool Cast::PlayerIsFree(RE::PlayerCharacter* a_player)
{
	auto ui = RE::UI::GetSingleton();
	if (ui && ui->GameIsPaused()) {
		return false;
	}
	auto state = a_player->AsActorState();
	return state && state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
}

void Cast::Start(std::shared_ptr<Job> a_job)
{
	Schedule(std::move(a_job), 0, 0, kPollInterval);
}

void Cast::OnPlayer(RE::PlayerCharacter* a_player, RE::SpellItem* a_spell)
{
	auto caster = a_player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
	if (!caster) {
		logs::warn("Player has no instant magic caster - cannot apply {}", a_spell->GetName());
		return;
	}
	caster->CastSpellImmediate(a_spell, true, a_player, 1.0f, false, 0.0f, a_player);
}
