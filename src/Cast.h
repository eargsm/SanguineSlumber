#pragma once

namespace Cast
{
	struct Job
	{
		std::string                                    name;
		std::function<void(RE::PlayerCharacter*)>      cast;
		std::function<bool(RE::PlayerCharacter*)>      isActive;
		std::function<void(int)>                       onActive;
	};

	bool PlayerIsFree(RE::PlayerCharacter* a_player);
	void Start(std::shared_ptr<Job> a_job);
	void OnPlayer(RE::PlayerCharacter* a_player, RE::SpellItem* a_spell);
}
