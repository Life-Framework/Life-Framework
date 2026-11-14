//------------------------------------------------------------------------------------------------
//! Pack overlay for the swap-and-spawn proof: the evidence marker slot swapped to the casing, the
//! phone handset slot pointed at a prefab that does not exist.
class LF_Test_PrefabSlotsSwapStub : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{5E0C7A93D1B24F68}Configs/Tests/LF_Test_PrefabSlotsSwapOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_PrefabSlotsConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "prefab-slots-swap-stub";
	}
}

//------------------------------------------------------------------------------------------------
//! A pack swaps one slot (evidence_marker) and the swapped prefab really spawns; a bad swap
//! (phone_handset to a missing prefab) falls back to the core default, which also spawns, and logs
//! `[LFDebug:PrefabSlots] ... rejected` (the log line is the proof of the Error, see the red-proof).
// tier: WORLD
// red-proof: batched `--red` 2026-11-15: with the pack stub skipped the swap assertion fails (expected the casing, got the core marker).
class LF_Test_PrefabSlotsSwapSpawn : LF_Test
{
	static const ResourceName CASING = "{7D3A9E15C2B4F680}Prefabs/Investigation/LF_EvidenceCasing.et";
	static const ResourceName MARKER = "{4C81F2A6D9E37B05}Prefabs/Investigation/LF_EvidenceMarker.et";
	static const ResourceName HANDSET = "{3942EAE9AFB4F8F3}Prefabs/Comms/LF_PhoneHandset.et";

	override string GetName()
	{
		return "prefabs/slot-swap-spawn";
	}

	protected void CheckSpawns(LF_TestContext ctx, ResourceName prefab, string expectName, string what)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(320, 100);
		IEntity entity = LF_Utils.SpawnEntityPrefab(prefab, Vector(320, groundY, 100));
		ctx.NotNull(entity, what + " spawned");
		if (!entity)
			return;
		EntityPrefabData data = entity.GetPrefabData();
		string name;
		if (data)
			name = data.GetPrefabName();
		ctx.True(name.Contains(expectName), what + " is the " + expectName + " prefab (got '" + name + "')");
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		if (!RedProof())
			LF_ConfigService.SetPackSource(new LF_Test_PrefabSlotsSwapStub());
		LF_PrefabSlots.ResetStatics();

		ResourceName marker = LF_Evidence.MarkerPrefab();
		ctx.EqualStr(CASING, marker, "the pack swapped the evidence marker to the casing");
		if (marker == MARKER)
			ctx.Fail("the evidence marker slot was not swapped");
		else
			CheckSpawns(ctx, marker, "LF_EvidenceCasing", "the swapped marker");

		ResourceName handset = LF_PhoneHandsetTransport.HandsetPrefab();
		ctx.EqualStr(HANDSET, handset, "a handset that does not load falls back to the core default");
		CheckSpawns(ctx, handset, "LF_PhoneHandset", "the fallback handset");

		LF_ConfigService.Reset();
		LF_PrefabSlots.ResetStatics();
	}
}
