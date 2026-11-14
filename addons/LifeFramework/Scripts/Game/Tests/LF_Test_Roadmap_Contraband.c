// red-proof: 2026-11-08 (M1-14) LF_Contraband.ITEM pointed back at RowanBerries.et, `tools\cli test --no-build --only LF_Test_Roadmap_Contraband,LF_Test_Roadmap_ContrabandWorld` printed [LFTEST] SUMMARY tier=only passed=0 failed=2 / - rowan berries are food again, not contraband / - the contraband crate is carried in both hands. Restored: passed=5 failed=0.
// red-proof: 2026-10-11 fast boot logs_2026-10-11_05-19-01 printed [LFTEST] FAIL roadmap/contraband / [LFTEST]   - delivery pays the configured cash once (expected 0, got 200)

// tier: LOGIC
class LF_Test_Roadmap_Contraband : LF_Test
{
	override string GetName()
	{
		return "roadmap/contraband";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();

		ctx.True(LF_Contraband.IsContraband(LF_Contraband.ITEM), "the configured prefab is contraband");
		ctx.False(LF_Contraband.IsContraband(LF_MoneyUtils.PREFAB_CASH), "cash is not contraband");
		ctx.False(LF_Contraband.IsContraband("{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et"), "rowan berries are food again, not contraband");

		vector drop = "131.5 0 284";
		ctx.True(LF_Contraband.NearDrop("140 0 300", drop), "getting out 18 m from the drop is arriving");
		ctx.False(LF_Contraband.NearDrop("160 0 284", drop), "getting out 28.5 m from the drop is leaving the vehicle");
		ctx.True(LF_Contraband.NearDrop("131.5 40 284", drop), "height does not count toward the drop distance");


		string founder = "roadmap-contraband-founder";
		string member = "roadmap-contraband-member";
		string outsider = "roadmap-contraband-outsider";
		string leaver = "roadmap-contraband-leaver";
		string casualty = "roadmap-contraband-casualty";

		LF_Organization org = LF_OrganizationManager.Create(founder, "Convoy Outfit");
		ctx.NotNull(org, "founder creates the owning organization");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds the courier");
		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, leaver), "founder adds the driver who leaves");
		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, casualty), "founder adds the driver who dies");
		ctx.True(LF_Hideout.Assign(LF_Hideout.OrgHideoutId(), org.GetId()), "the organization owns the hideout");

		ctx.False(LF_Contraband.Start(outsider, null), "a non-member cannot start the convoy");
		ctx.EqualStr("convoy refused: not a member", LF_Contraband.GetLastRefusal(), "the non-member start is logged");

		ctx.True(LF_Contraband.Start(member, null), "a member starts the convoy");
		ctx.Equal(0, LF_Contraband.Deliver(member, 0, null), "delivery without contraband pays nothing");
		ctx.EqualStr("delivery refused: no contraband", LF_Contraband.GetLastRefusal(), "the empty delivery is logged");
		ctx.Equal(LF_Contraband.PAYOUT, LF_Contraband.Deliver(member, 1, null), "delivery pays the configured cash once");
		ctx.Equal(LF_Contraband.PAYOUT, LF_Contraband.GetCashPaid(member), "the payout is recorded once");
		ctx.Equal(0, LF_Contraband.Deliver(member, 1, null), "a second delivery pays nothing");
		ctx.EqualStr("delivery refused: already paid", LF_Contraband.GetLastRefusal(), "the second delivery is logged");
		ctx.Equal(LF_Contraband.PAYOUT, LF_Contraband.GetCashPaid(member), "the second delivery does not add cash");

		ctx.True(LF_Contraband.Start(leaver, null), "the leaving driver starts a run");
		ctx.True(LF_Contraband.FailLeftVehicle(leaver), "leaving the vehicle fails the run");
		ctx.EqualStr("convoy failed: left the vehicle", LF_Contraband.GetLastRefusal(), "leaving the vehicle is logged");
		ctx.Equal(0, LF_Contraband.Deliver(leaver, 1, null), "a run that left the vehicle pays nothing");
		ctx.EqualStr("delivery refused: run failed", LF_Contraband.GetLastRefusal(), "the failed delivery is logged");
		ctx.Equal(0, LF_Contraband.GetCashPaid(leaver), "a failed run records no cash");

		ctx.True(LF_Contraband.Start(casualty, null), "the casualty starts a run");
		ctx.True(LF_Contraband.FailDied(casualty), "dying fails the run");
		ctx.EqualStr("convoy failed: died", LF_Contraband.GetLastRefusal(), "dying is logged");
		ctx.Equal(0, LF_Contraband.Deliver(casualty, 1, null), "a run that ended in death pays nothing");
		ctx.Equal(0, LF_Contraband.GetCashPaid(casualty), "death records no cash");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
	}
}

// tier: WORLD
class LF_Test_Roadmap_ContrabandWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/roadmap-contraband";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConvoyMarkerComponent startMarker = LF_ConvoyMarkerComponent.FindStart();
		LF_ConvoyMarkerComponent dropMarker = LF_ConvoyMarkerComponent.FindDrop(false);
		LF_ConvoyMarkerComponent nearMarker = LF_ConvoyMarkerComponent.FindDrop(true);
		ctx.NotNull(startMarker, "the world places a convoy start (LF_ConvoyMarkerComponent, not a drop)");
		ctx.NotNull(dropMarker, "the world places a convoy drop");
		ctx.NotNull(nearMarker, "the world places a near convoy drop for test modes");
		if (ctx.FailureCount() > 0)
			return;

		IEntity start = startMarker.GetOwner();
		IEntity drop = dropMarker.GetOwner();
		IEntity nearDrop = nearMarker.GetOwner();
		ctx.False(startMarker.IsDrop(), "the start marker is not the drop");
		ctx.True(dropMarker.IsDrop(), "the drop marker is the drop");
		ctx.True(nearMarker.IsDrop(), "the near drop marker is a drop");
		ctx.True(nearMarker.IsNearDrop(), "the near drop marker is flagged near");
		ctx.False(dropMarker.IsNearDrop(), "the far drop marker is not flagged near");
		ctx.True(nearDrop != drop, "the near and far drops are two markers");

		LF_GameModeRoleplay mode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (mode)
		{
			ctx.True(mode.IsConvoyNearDrop(), "the debug game mode picks the near drop");
			ctx.True(LF_Contraband.IsNearDropActive(), "the active drop in DebugWorld is the near one");
			ctx.True(LF_Contraband.FindActiveDrop() == nearDrop, "the active drop entity is the near marker");
		}

		ctx.True(HasAction(nearDrop, false), "the near drop uses the convoy deliver action");
		ctx.True(start.GetVObject() != null, "the pickup has a mesh");
		ctx.True(drop.GetVObject() != null, "the drop has a mesh");

		ctx.True(HasAction(start, true), "the start marker uses the convoy start action");
		ctx.False(HasAction(start, false), "the start marker does not pay out");
		ctx.True(HasAction(drop, false), "the drop marker uses the convoy deliver action");
		ctx.False(HasAction(drop, true), "the drop marker does not start a run");

		CheckCrate(ctx);
		CheckLeaveAtDrop(ctx, nearDrop.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! The cargo is a real crate in the hands: it loads, has a mesh, is hand-carried, and is flagged.
	protected void CheckCrate(LF_TestContext ctx)
	{
		ctx.True(LF_Contraband.IsContraband(LF_Contraband.ITEM), "the crate is flagged contraband");
		Resource resource = Resource.Load(LF_Contraband.ITEM);
		ctx.True(resource.IsValid(), "the contraband crate prefab loads");
		if (!resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity crate = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.True(crate != null, "the contraband crate spawns");
		if (!crate)
			return;

		ctx.True(crate.GetVObject() != null, "the contraband crate has a mesh");
		ctx.NotNull(LF_Component<LF_HandCarryComponent>.Find(crate), "the contraband crate is carried in both hands");
		SCR_EntityHelper.DeleteEntityAndChildren(crate);
	}

	//------------------------------------------------------------------------------------------------
	//! Getting out at the placed drop keeps the run for the delivery; getting out on the road fails it.
	protected void CheckLeaveAtDrop(LF_TestContext ctx, vector dropOrigin)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();

		string founder = "roadmap-contraband-world-founder";
		string arriving = "roadmap-contraband-world-arriving";
		string quitting = "roadmap-contraband-world-quitting";
		LF_Organization org = LF_OrganizationManager.Create(founder, "Pub Suppliers");
		ctx.NotNull(org, "founder creates the owning organization");
		if (!org)
			return;

		LF_OrganizationManager.Invite(org.GetId(), founder, arriving);
		LF_OrganizationManager.Invite(org.GetId(), founder, quitting);
		LF_Hideout.Assign(LF_Hideout.OrgHideoutId(), org.GetId());

		ctx.True(LF_Contraband.Start(arriving, null), "the arriving driver starts a run");
		ctx.False(LF_Contraband.LeftVehicleAt(arriving, dropOrigin + "6 0 4"), "getting out beside the drop does not fail the run");
		ctx.Equal(LF_Contraband.PAYOUT, LF_Contraband.Deliver(arriving, 1, null), "the driver who got out at the drop is paid");

		ctx.True(LF_Contraband.Start(quitting, null), "the quitting driver starts a run");
		ctx.True(LF_Contraband.LeftVehicleAt(quitting, "300 0 250"), "getting out on the main street fails the run");
		ctx.Equal(0, LF_Contraband.Deliver(quitting, 1, null), "the driver who got out on the way is not paid");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
	}

	protected bool HasAction(IEntity entity, bool startAction)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(entity);
		if (!actions)
			return false;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			if (startAction && LF_ConvoyStartAction.Cast(action))
				return true;

			if (!startAction && LF_ConvoyDeliverAction.Cast(action))
				return true;
		}

		return false;
	}
}
