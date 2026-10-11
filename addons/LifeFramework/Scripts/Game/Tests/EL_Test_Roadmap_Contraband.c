// red-proof: 2026-10-11 fast boot logs_2026-10-11_05-19-01 printed [ELTEST] FAIL roadmap/contraband / [ELTEST]   - delivery pays the configured cash once (expected 0, got 200)

// tier: LOGIC
class EL_Test_Roadmap_Contraband : EL_Test
{
	override string GetName()
	{
		return "roadmap/contraband";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();

		ctx.True(LF_Contraband.IsContraband(LF_Contraband.ITEM), "the configured prefab is contraband");
		ctx.False(LF_Contraband.IsContraband(EL_MoneyUtils.PREFAB_CASH), "cash is not contraband");

		string founder = "roadmap-contraband-founder";
		string member = "roadmap-contraband-member";
		string outsider = "roadmap-contraband-outsider";
		string leaver = "roadmap-contraband-leaver";
		string casualty = "roadmap-contraband-casualty";

		EL_Organization org = EL_OrganizationManager.Create(founder, "Convoy Outfit");
		ctx.NotNull(org, "founder creates the owning organization");
		if (!org)
			return;

		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds the courier");
		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, leaver), "founder adds the driver who leaves");
		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, casualty), "founder adds the driver who dies");
		ctx.True(LF_Hideout.Assign(LF_Hideout.HIDEOUT_ID, org.GetId()), "the organization owns the hideout");

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

		EL_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
	}
}

// tier: WORLD
class EL_Test_Roadmap_ContrabandWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-contraband";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity start = GetGame().GetWorld().FindEntityByName(LF_Contraband.START_ID);
		ctx.True(start != null, "Roadmap_Convoy_Start is placed");
		IEntity drop = GetGame().GetWorld().FindEntityByName(LF_Contraband.DROP_ID);
		ctx.True(drop != null, "Roadmap_Convoy_Drop is placed");
		if (ctx.FailureCount() > 0)
			return;

		vector startOrigin = start.GetOrigin();
		ctx.EqualFloat(222, startOrigin[0], 0.1, "the start marker is at X 222");
		ctx.EqualFloat(0, startOrigin[1], 0.1, "the start marker is at Y 0");
		ctx.EqualFloat(120, startOrigin[2], 0.1, "the start marker is at Z 120");
		vector dropOrigin = drop.GetOrigin();
		ctx.EqualFloat(225, dropOrigin[0], 0.1, "the drop marker is at X 225");
		ctx.EqualFloat(0, dropOrigin[1], 0.1, "the drop marker is at Y 0");
		ctx.EqualFloat(120, dropOrigin[2], 0.1, "the drop marker is at Z 120");

		LF_ConvoyMarkerComponent startMarker = EL_Component<LF_ConvoyMarkerComponent>.Find(start);
		ctx.NotNull(startMarker, "the start marker carries LF_ConvoyMarkerComponent");
		LF_ConvoyMarkerComponent dropMarker = EL_Component<LF_ConvoyMarkerComponent>.Find(drop);
		ctx.NotNull(dropMarker, "the drop marker carries LF_ConvoyMarkerComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(LF_Contraband.START_ID, startMarker.GetMarkerId(), "the start id is the placed name");
		ctx.False(startMarker.IsDrop(), "the start marker is not the drop");
		ctx.EqualStr(LF_Contraband.DROP_ID, dropMarker.GetMarkerId(), "the drop id is the placed name");
		ctx.True(dropMarker.IsDrop(), "the drop marker is the drop");

		ctx.True(HasAction(start, true), "the start marker uses the convoy start action");
		ctx.False(HasAction(start, false), "the start marker does not pay out");
		ctx.True(HasAction(drop, false), "the drop marker uses the convoy deliver action");
		ctx.False(HasAction(drop, true), "the drop marker does not start a run");
	}

	protected bool HasAction(IEntity entity, bool startAction)
	{
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(entity);
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
