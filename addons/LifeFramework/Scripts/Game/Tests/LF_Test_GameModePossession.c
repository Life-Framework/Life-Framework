// red-proof: 2026-11-27 (#5154): with the SCR_PossessingManagerComponent row off GameMode_Roleplay.et, `--only LF_Test_GameModePossession` failed `medic/game-mode-possession` (no possessing manager on the game mode); with the row, green.

//! #5154: vanilla `SCR_CharacterHelper.GetCharacterControlType` reads `SCR_PossessingManagerComponent`
//! with no null check, and vanilla death handling calls it for every player killer. Without the
//! component on the roleplay game mode a give-up or bleed-out while the downer is online raised a
//! VME on the server and every client.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GameModePossession : LF_Test
{
	override string GetName()
	{
		return "medic/game-mode-possession";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		ctx.NotNull(gameMode, "the test world has a game mode");
		if (!gameMode)
			return;

		ctx.NotNull(gameMode.FindComponent(SCR_PossessingManagerComponent), "the game mode carries the possessing manager");
		ctx.NotNull(SCR_PossessingManagerComponent.GetInstance(), "vanilla reaches the manager through GetInstance");
	}
}
