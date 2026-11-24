// red-proof: 2026-11-24, RED-OK (3 assertions fail with the hook on); `tools\cli test --no-build --red LF_Test_MapMarkerEntityBind` (RedProof() skips the bind call, so the missing-entity error is never logged and the spawned entity is never picked up)
//! map/marker-entity-bind (#4726): vanilla's marker manager reads `SCR_MapEntity.GetMapInstance()` once in
//! OnPostInit. A world that places the map entity in a later layer (the playtest town) left that copy null and
//! `OnMapOpen` threw a VME every frame the map was open. The manager now picks the entity up again on use, and
//! with no map entity in the world (this test world) it logs one error and reports "not usable" instead of
//! letting the vanilla handler dereference null.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MapMarkerEntityBind : LF_Test
{
	protected static const ResourceName MAP_ENTITY = "{731564B66F91B107}Prefabs/World/Game/MapEntity.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/marker-entity-bind";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		ctx.NotNull(gameMode, "a game mode is running");
		if (!gameMode)
			return;

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.Cast(gameMode.FindComponent(SCR_MapMarkerManagerComponent));
		ctx.NotNull(manager, "the game mode carries SCR_MapMarkerManagerComponent");
		if (!manager)
			return;

		// The manager initialised before any map entity existed: forget whatever it holds.
		manager.LF_ForgetMapEntityForTest();
		if (SCR_MapEntity.GetMapInstance())
		{
			ctx.True(manager.LF_BindMapEntity(), "a world map entity exists, so the manager binds it");
			return;
		}

		// No map entity: one error, not usable, no exception.
		bool usable = true;
		if (!RedProof())
			usable = manager.LF_BindMapEntity();
		ctx.False(usable, "with no map entity the manager reports it is not usable");
		ctx.True(manager.LF_MapEntityMissingLogged(), "the missing map entity is logged once");

		// The world places a map entity later (the town): the manager picks it up.
		Resource res = Resource.Load(MAP_ENTITY);
		ctx.True(res && res.IsValid(), "the map entity prefab loads");
		if (!res || !res.IsValid())
			return;

		IEntity entity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld());
		ctx.NotNull(entity, "the map entity spawns");
		if (!entity)
			return;

		bool bound = false;
		if (!RedProof())
			bound = manager.LF_BindMapEntity();
		ctx.True(bound, "once the world has a map entity the manager binds it");

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		manager.LF_ForgetMapEntityForTest();
	}
}
