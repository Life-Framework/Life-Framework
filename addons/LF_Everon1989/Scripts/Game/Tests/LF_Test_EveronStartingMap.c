// red-proof: 2026-11-19, `tools\cli test --no-build --red LF_Test_ArrivalGuide,LF_Test_EveronStartingMap` (RedProof() skips the kit): RED-OK everon/starting-map (fails with the hook on, 6 assertions); green without it (pack leg).
//! A new character on the island starts with the vanilla folded paper map in their inventory, so they
//! can find the clerk the first errand sends them to (the waypoint and map mark are drawn on it). Under the
//! Everon 1989 header (the pack leg), a body given the real civilian starting kit carries exactly one
//! map: not none, and not a second from the clerk's welcome gift, which no longer lists one.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronStartingMap : LF_Test
{
	static const ResourceName MAP = "{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/Map_Paper_01/PaperMap_01_folded.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/starting-map";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		ctx.NotNull(spawnLogic, "the pack leg runs the LF spawn logic");
		if (!spawnLogic)
			return;

		IEntity newcomer = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(newcomer, "the newcomer spawns");
		if (!newcomer)
			return;

		ctx.Equal(0, LF_QuestManager.GetInstance().CountItem(newcomer, MAP), "a bare body carries no map");
		if (!RedProof())
			ctx.True(spawnLogic.GiveRoleKit(newcomer, LF_Faction.CIVILIAN), "the newcomer is given the civilian starting kit");

		ctx.Equal(1, LF_QuestManager.GetInstance().CountItem(newcomer, MAP), "a new character starts with exactly one paper map");

		// The clerk's welcome gift adds food and a drink, never a second map.
		array<ResourceName> gift = {};
		LF_WelcomeGiftTable.ItemsOf("arrival_register", gift);
		ctx.True(gift.Find(MAP) == -1, "the welcome gift holds no paper map");

		LF_TestCharacterFixture.Delete(newcomer);
	}
}
