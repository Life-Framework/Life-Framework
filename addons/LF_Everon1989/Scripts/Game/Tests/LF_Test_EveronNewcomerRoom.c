// red-proof: observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_EveronNewcomerRoom` before the kit sack: FAIL `a bought shirt fits on a newcomer (expected 1, got 0)` with `[LFDebug:Quantity] add failed: inventory rejected ...Shirt_CottonShirt_01_V2.et`; with `kit_canvas_sack` in `LF_Catalogue_Kit_Everon1989.conf` it passes.
//! Weekend blocker: a fresh newcomer on the island bought a shirt at the Saint-Philippe clothing counter
//! and was refused `buy failed (inventory full)`. The pack's starting kit (denim jacket, trousers, boots,
//! flat cap) wears the jacket slot a shirt needs, and its pockets take items up to 10x10x10 while a shirt
//! is 15x15x15, so a bought shirt had nowhere to go. Under the Everon 1989 header (the pack leg), a body
//! given the real civilian kit must have room for the first-hour buys through the shop's own give path
//! (`LF_InventoryUtils.AddAmount`): the shirt and a bottle of water.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronNewcomerRoom : LF_Test
{
	static const ResourceName SHIRT = "{86E0117B630414B5}Prefabs/Characters/Uniforms/Shirt_CottonShirt_01/Shirt_CottonShirt_01_V2.et";
	static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/newcomer-room";
	}

	//------------------------------------------------------------------------------------------------
	//! A bare character at a civilian spawn point, wearing nothing the kit does not give it.
	protected IEntity SpawnBare(LF_TestContext ctx)
	{
		Resource res = Resource.Load(LF_TestCharacterFixture.CHARACTER_PREFAB);
		if (!res.IsValid())
		{
			ctx.Fail("the character prefab loads");
			return null;
		}

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		if (!spawnPoint)
		{
			ctx.Fail("a spawn point resolves for the newcomer");
			return null;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		ctx.NotNull(spawnLogic, "the pack leg runs the LF spawn logic");
		if (!spawnLogic)
			return;

		IEntity newcomer = SpawnBare(ctx);
		ctx.NotNull(newcomer, "the newcomer spawns");
		if (!newcomer)
			return;

		ctx.True(spawnLogic.GiveRoleKit(newcomer, LF_Faction.CIVILIAN), "the newcomer is given the civilian starting kit");
		ctx.Equal(100, LF_MoneyUtils.GetCash(newcomer), "the kit carries the starting cash");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(newcomer, SHIRT, 1), "a bought shirt fits on a newcomer");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(newcomer, WATER, 1), "a bought bottle of water fits on a newcomer");
		ctx.Equal(100, LF_MoneyUtils.GetCash(newcomer), "the cash is still carried after the buys land");

		LF_TestCharacterFixture.Delete(newcomer);
	}
}
