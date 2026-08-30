// red-proof: change the bitmask test in EL_SaveSelection.FindLatestAutoSaveIndex
// from (type & ESaveGameType.AUTO) == 0 to == ESaveGameType.AUTO and
// save/autosave-selection-combined-flags goes red; drop the playthrough check
// and save/autosave-selection-foreign-playthrough goes red. For the WORLD test,
// remove EL_PersistenceManagerComponent from GameMode_Roleplay.et and
// save/manager-wiring goes red.

// tier: LOGIC
class EL_Test_AutosaveSelection : EL_Test
{
	override string GetName()
	{
		return "save/autosave-selection";
	}

	override void Run(EL_TestContext ctx)
	{
		ref array<ref EL_SaveInfo> saves = new array<ref EL_SaveInfo>();

		ctx.Equal(-1, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "empty list has no autosave target");
		ctx.Equal(-1, EL_SaveSelection.FindLatestAutoSaveIndex(null, 0), "null list has no autosave target");

		saves.Insert(EL_SaveInfo.Create(ESaveGameType.AUTO, 0));
		ctx.Equal(0, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "single autosave for the current playthrough is the target");

		saves.Clear();
		saves.Insert(EL_SaveInfo.Create(ESaveGameType.AUTO, 1));
		ctx.Equal(-1, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "autosave for a foreign playthrough is not a target");

		saves.Clear();
		saves.Insert(EL_SaveInfo.Create(ESaveGameType.MANUAL, 0));
		ctx.Equal(-1, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "a manual-only list has no autosave target");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_AutosaveSelectionBitmask : EL_Test
{
	override string GetName()
	{
		return "save/autosave-selection-combined-flags";
	}

	override void Run(EL_TestContext ctx)
	{
		ref array<ref EL_SaveInfo> saves = new array<ref EL_SaveInfo>();

		saves.Insert(EL_SaveInfo.Create(ESaveGameType.AUTO | ESaveGameType.MANUAL, 0));
		ctx.Equal(0, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "combined AUTO|MANUAL flag still matches as an autosave target");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_AutosaveSelectionNewest : EL_Test
{
	override string GetName()
	{
		return "save/autosave-selection-newest";
	}

	override void Run(EL_TestContext ctx)
	{
		ref array<ref EL_SaveInfo> saves = new array<ref EL_SaveInfo>();

		saves.Insert(EL_SaveInfo.Create(ESaveGameType.MANUAL, 0));
		saves.Insert(EL_SaveInfo.Create(ESaveGameType.AUTO, 0));
		saves.Insert(EL_SaveInfo.Create(ESaveGameType.AUTO, 0));
		ctx.Equal(2, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "newest autosave for the playthrough is the overwrite target");

		saves.Insert(EL_SaveInfo.Create(ESaveGameType.AUTO, 1));
		ctx.Equal(2, EL_SaveSelection.FindLatestAutoSaveIndex(saves, 0), "a newer autosave for a foreign playthrough is skipped");
	}
};

//------------------------------------------------------------------------------------------------
// tier: WORLD
class EL_Test_PersistenceManagerWiring : EL_Test
{
	override string GetName()
	{
		return "save/manager-wiring";
	}

	override void Run(EL_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode is a SCR_BaseGameMode");
		if (!gameMode)
			return;

		EL_PersistenceManagerComponent manager = EL_PersistenceManagerComponent.Cast(gameMode.FindComponent(EL_PersistenceManagerComponent));
		ctx.NotNull(manager, "the game-mode entity carries EL_PersistenceManagerComponent");
		if (!manager)
			return;

		ctx.NotNull(manager.GetPersistenceSystem(), "the save manager resolved the engine persistence system");
		ctx.NotNull(SCR_PersistenceSystem.GetScriptedInstance(), "the engine persistence system is registered on the test server");
	}
};