// red-proof: run on the unfixed game-mode prefab (no SCR_SupportStationManagerComponent) it fails: GetInstance() is null and the vanilla ambulance's heal and load-casualty stations raise a VME at every draw (#4878).
//! #4878: the vanilla ambulance carries support stations with a range (heal, load casualty). Each registers itself with
//! the game mode's SCR_SupportStationManagerComponent in DelayedInit and raises a Virtual Machine Exception when the
//! game mode has none. The Life Framework game mode carries the manager, so a drawn ambulance initialises cleanly and
//! its vanilla stations work.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SupportStationManager : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/support-station-manager";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		ctx.NotNull(gameMode, "the world has a game mode");
		if (!gameMode)
			return;

		SCR_SupportStationManagerComponent onMode = SCR_SupportStationManagerComponent.Cast(gameMode.FindComponent(SCR_SupportStationManagerComponent));
		ctx.NotNull(onMode, "the game mode carries a SCR_SupportStationManagerComponent (vanilla support stations look it up)");
		ctx.NotNull(SCR_SupportStationManagerComponent.GetInstance(), "the support station manager has registered its instance");
	}
}
