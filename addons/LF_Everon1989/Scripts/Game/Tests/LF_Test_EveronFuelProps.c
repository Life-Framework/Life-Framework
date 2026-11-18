// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronFuelProps`: RedProof() skips the spawn, so no prop, action or site exists and every check fails.
//! everon/fuel-props (#3370): the island's fuel delivery job needs a FUEL site to start the run and a
//! DROP site to finish it. `LF_FuelPickup` is a pallet of jerrycans with the FUEL site and the
//! start-run action; `LF_FuelDrop` is a fuel drum with the DROP site and the drop-off action.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFuelProps : LF_Test
{
	protected static const ResourceName PICKUP = "{7A3C91E5B2D40F18}Prefabs/Props/Roleplay/LF_FuelPickup.et";
	protected static const ResourceName DROP = "{3E8B5A27C94D61F0}Prefabs/Props/Roleplay/LF_FuelDrop.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/fuel-props";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity pickup;
		IEntity drop;
		if (!RedProof())
		{
			pickup = LF_Utils.SpawnEntityPrefab(PICKUP, Vector(0, 500, 0));
			drop = LF_Utils.SpawnEntityPrefab(DROP, Vector(10, 500, 0));
		}

		ctx.NotNull(pickup, "the fuel pickup prefab spawns");
		ctx.NotNull(drop, "the fuel drop prefab spawns");

		if (pickup)
		{
			LF_JobSiteComponent site = LF_Component<LF_JobSiteComponent>.Find(pickup);
			ctx.NotNull(site, "the pickup carries a job site");
			if (site)
				ctx.True(site.GetSite() == LF_EJobSite.FUEL, "the pickup is a FUEL site");

			ctx.True(HasStart(pickup, LF_EJobType.FUEL_DELIVERY), "the pickup starts the fuel delivery run");
			ctx.NotNull(RplComponent.Cast(pickup.FindComponent(RplComponent)), "the pickup replicates");
			SCR_EntityHelper.DeleteEntityAndChildren(pickup);
		}

		if (drop)
		{
			LF_JobSiteComponent site = LF_Component<LF_JobSiteComponent>.Find(drop);
			ctx.NotNull(site, "the drop carries a job site");
			if (site)
				ctx.True(site.GetSite() == LF_EJobSite.DROP, "the drop is a DROP site");

			ctx.True(HasDropoff(drop), "the drop takes a delivery");
			ctx.NotNull(RplComponent.Cast(drop.FindComponent(RplComponent)), "the drop replicates");
			SCR_EntityHelper.DeleteEntityAndChildren(drop);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasStart(IEntity entity, LF_EJobType job)
	{
		array<BaseUserAction> actionList = {};
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			LF_JobStartRunAction start = LF_JobStartRunAction.Cast(action);
			if (start && start.GetJob() == job)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDropoff(IEntity entity)
	{
		array<BaseUserAction> actionList = {};
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			if (LF_JobDropoffAction.Cast(action))
				return true;
		}

		return false;
	}
}
