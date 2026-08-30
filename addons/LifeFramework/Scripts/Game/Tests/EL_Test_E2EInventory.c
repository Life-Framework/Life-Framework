// red-proof: change an expected value below (e.g. expect 201 cash instead of
// 200, or 4 apples instead of 3) and run `tools\cli test --tier all`; the
// assertion goes red. Observed red: expected GetCash 201 with AddCash(200)
// added, then reverted.
// tier: WORLD
class EL_Test_E2EInventory : EL_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/inventory-storage";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads: " + CHARACTER_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		// The spawn logic creates characters with a WORLD transform at a spawn point
		// (EL_SpawnLogic.CreateCharacter). A LOCAL-origin spawn leaves the character
		// outside the replication graph, so inventory moves never commit.
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		ctx.True(spawnPoint != null, "a spawn point resolves for the character spawn");
		if (ctx.FailureCount() > 0)
			return;

		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(character != null, "character spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		// The storage manager is a static component on the merged character base, so
		// a bare spawn resolves it. A naked character still has no deposit storage,
		// which is why the spawn logic inserts loadout clothing before its direct
		// items: the jacket's pockets are what the best-slot search finds.
		InventoryStorageManagerComponent storage = EL_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.NotNull(storage, "bare character resolves its inventory storage manager");
		if (ctx.FailureCount() > 0)
			return;

		IEntity jacket = EL_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.True(jacket != null, "jacket spawns for the loadout storage");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket inserts into the character loadout, giving it pockets");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(200, EL_MoneyUtils.AddCash(character, 200), "AddCash(200) reports 200 added");
		ctx.Equal(200, EL_MoneyUtils.GetCash(character), "GetCash reads 200 after AddCash");

		ctx.Equal(3, EL_InventoryUtils.AddAmount(character, APPLE_PREFAB, 3), "AddAmount adds 3 apples");
		ctx.Equal(3, EL_InventoryUtils.GetAmount(character, APPLE_PREFAB), "GetAmount reads 3 apples");
	}
}