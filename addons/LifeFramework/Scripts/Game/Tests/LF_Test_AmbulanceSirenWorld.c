// red-proof: the pre-fix prefab (ParentContextList `driver` on both siren actions) fails `a siren action has no positional context` (0 vs 1 context); observed 2026-11-16: light default back to lit (m_isOn = true, SpawnLights in OnPostInit) -> `a fresh vehicle starts with every bar light off` FAIL on both service vehicles, green on the fix; also m_bDutyOnly 0 on the prefab actions -> [LFTEST] FAIL a siren action on the ambulance is for the on-duty medic only
// tier: WORLD
//! The ambulance's siren (#923): the prefab carries the toggle with its other light controls
//! (no positional context, so the seated driver sees them; a duty-only on and off pair that shows greyed with a reason when off duty), its own sound project loads and is
//! set on the manager, the police car keeps the shared siren, and an anonymous request is refused.
class LF_Test_AmbulanceSirenWorld : LF_Test
{
	static const ResourceName AMBULANCE = "{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et";
	static const ResourceName POLICE = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	static const ResourceName PROJECT = "{504037CAE978B960}Sounds/Siren/Ambulance/Siren_Ambulance_Hilo.acp";

	override string GetName()
	{
		return "sirenlights/ambulance-siren-world";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource project = Resource.Load(PROJECT);
		ctx.True(project && project.IsValid(), "the ambulance's sound project loads");

		RunActions(ctx);
		RunVehicle(ctx, AMBULANCE, true);
		RunVehicle(ctx, POLICE, false);
	}

	//------------------------------------------------------------------------------------------------
	//! A freshly spawned service vehicle has every bar light dark and the siren off.
	protected void CheckLightsOff(LF_TestContext ctx, LF_SirenManagerComponent siren, string label)
	{
		ctx.True(siren.CountLights() > 0, label + ": the light bar registered its lights");
		ctx.Equal(0, siren.CountLitLights(), label + ": a fresh vehicle starts with every bar light off");
		ctx.False(siren.IsOn(), label + ": and the siren off");
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab's own action list: one duty-only action per mode, both with no positional context (shown to the seated pilot by the action's own seat rule).
	protected void RunActions(LF_TestContext ctx)
	{
		Resource res = Resource.Load(AMBULANCE);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the ambulance prefab does not load: " + AMBULANCE);
			return;
		}

		IEntitySource source = res.GetResource().ToEntitySource();
		int found = 0;
		bool wantOn;
		bool wantOff;
		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "ActionsManagerComponent")
				continue;

			BaseContainerList actions = component.GetObjectArray("additionalActions");
			if (!actions)
				continue;

			for (int a = 0; a < actions.Count(); a++)
			{
				BaseContainer action = actions.Get(a);
				if (action.GetClassName() != "LF_SirenModeAction")
					continue;

				found++;
				array<string> contexts = {};
				action.Get("ParentContextList", contexts);
				ctx.Equal(0, contexts.Count(), "a siren action has no positional context (a seated driver would have to aim at one), like the vanilla headlight toggle");
				bool dutyOnly;
				action.Get("m_bDutyOnly", dutyOnly);
				ctx.True(dutyOnly, "a siren action on the ambulance is for the on-duty medic only");
				string mode;
				action.Get("m_Mode", mode);
				if (mode == LF_AmbulanceSiren.MODE_ON)
					wantOn = true;
				if (mode == LF_AmbulanceSiren.MODE_OFF)
					wantOff = true;
			}
		}

		ctx.Equal(2, found, "the ambulance has the on and the off action");
		ctx.True(wantOn, "one action turns the siren on");
		ctx.True(wantOff, "one action turns it off");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVehicle(LF_TestContext ctx, ResourceName prefab, bool ownLoop)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			ctx.Fail("prefab does not load: " + prefab);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!vehicle)
		{
			ctx.Fail("prefab does not spawn: " + prefab);
			return;
		}

		LF_SirenManagerComponent siren = LF_Component<LF_SirenManagerComponent>.Find(vehicle);
		ctx.NotNull(siren, "the vehicle carries a siren manager");
		if (siren)
		{
			ctx.Equal(ownLoop, siren.HasOwnLoop(), "own siren loop set: " + ownLoop);
			CheckLightsOff(ctx, siren, prefab);
			ctx.False(siren.RequestModeStr(null, LF_AmbulanceSiren.MODE_ON), "a request from nobody is refused");
			ctx.False(siren.IsOn(), "and the siren stays off");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}
}
