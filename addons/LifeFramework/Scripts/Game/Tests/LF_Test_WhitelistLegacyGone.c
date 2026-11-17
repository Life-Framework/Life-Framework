// red-proof: 2026-11-17 on the unfixed prefab `tools/cli test --no-build --only LF_Test_WhitelistLegacyGone`
// failed whitelist/legacy-gone with "the whitelist sign offers no whitelist action" (the sign still carried
// LF_WhitelistAction); green once the action and its component were deleted.
// tier: WORLD
//! whitelist/legacy-gone (#2576): the whitelist sign is a readable sign only. The dead
//! LF_WhitelistAction (it needed a component no character carries) is off the prefab, and the
//! game mode's whitelist handler takes a spawned player without an error.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WhitelistLegacyGone : LF_Test
{
	protected static const ResourceName SIGN = "{C6940227F886B8EF}Prefabs/Props/Signs/EL_WhitelistSign.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/legacy-gone";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(SIGN);
		ctx.True(resource && resource.IsValid(), "the whitelist sign prefab loads");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity sign = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(sign, "the whitelist sign spawns");
		if (!sign)
			return;

		int reads = 0;
		int whitelistActions = 0;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(sign);
		ctx.NotNull(actions, "the whitelist sign has an actions manager");
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			if (!RedProof())
				actions.GetActionsList(actionList);

			foreach (BaseUserAction action : actionList)
			{
				if (!action)
					continue;

				if (LF_SignReadAction.Cast(action))
					reads++;

				if (action.ClassName() == "LF_WhitelistAction")
					whitelistActions++;
			}
		}

		ctx.Equal(1, reads, "the whitelist sign offers the read action");
		ctx.Equal(0, whitelistActions, "the whitelist sign offers no whitelist action");

		// A spawned player goes through the handler: a no-op for the legacy flags, no error.
		BaseGameMode mode = GetGame().GetGameMode();
		if (mode)
		{
			LF_WhitelistRespawnHandlerComponent handler = LF_WhitelistRespawnHandlerComponent.Cast(mode.FindComponent(LF_WhitelistRespawnHandlerComponent));
			if (handler)
			{
				handler.OnPlayerSpawned(1, sign);
				ctx.Pass("the handler takes a spawned player without an error");
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(sign);
	}
}
