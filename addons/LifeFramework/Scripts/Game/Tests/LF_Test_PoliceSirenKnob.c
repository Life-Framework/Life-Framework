// red-proof: observed 2026-11-22 with `tools/cli test --no-build --only LF_Test_PoliceSirenKnob` on the unfixed knob (local SetModeStr, action broadcast on): FAIL `a knob row is not re-run on every client`, `and the change is broadcast to the clients (expected 1, got 0)`, `an off-duty driver's pick is refused`, `and the off is broadcast (expected 2, got 0)`; green on the fix.
//------------------------------------------------------------------------------------------------
//! sirenlights/police-knob (0.2.0 live bug: "the cop siren on multiplayer wasn't working; the menu
//! shows but the sound doesn't toggle on"). The police jeep's knob rows must change the siren
//! through the server rule, so the replicated mode moves and every client (driver, passenger,
//! bystander, late joiner) runs it: an on-duty officer's pick is broadcast once, the row is not
//! re-run on each client by an action broadcast, an off-duty driver is refused, and a destroyed
//! jeep's siren goes off for everyone.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PoliceSirenKnob : LF_Test
{
	static const ResourceName POLICE = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	static const string OFFICER = "police-siren-knob-officer";

	override string GetName()
	{
		return "sirenlights/police-knob";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(POLICE);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the police jeep prefab does not load: " + POLICE);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!vehicle)
		{
			ctx.Fail("the police jeep does not spawn");
			return;
		}

		IEntity driver = LF_TestCharacterFixture.Spawn(ctx);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		LF_Faction faction = account.GetFaction();
		bool wasOnDuty = account.IsOnDuty();
		if (driver)
		{
			LF_Utils.SetUidForTests(driver, OFFICER);
			account.SetFaction(LF_Faction.POLICE);
			account.SetOnDuty(true);
			RunKnob(ctx, vehicle, driver, account);
		}

		account.SetOnDuty(wasOnDuty);
		account.SetFaction(faction);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(driver);
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKnob(LF_TestContext ctx, IEntity vehicle, IEntity driver, LF_PlayerAccount account)
	{
		LF_SirenManagerComponent siren = LF_Component<LF_SirenManagerComponent>.Find(vehicle);
		if (!siren)
		{
			ctx.Fail("the police jeep carries no siren manager");
			return;
		}

		IEntity knob = FindKnob(vehicle);
		ctx.NotNull(knob, "the police jeep has its siren knob");
		if (!knob)
			return;

		LF_SirenModeAction patrol = FindAction(knob, "Mode1");
		LF_SirenModeAction off = FindAction(knob, "default");
		ctx.NotNull(patrol, "the knob has the patrol row");
		ctx.NotNull(off, "the knob has the off row");
		if (!patrol || !off)
			return;

		ctx.False(patrol.CanBroadcastScript(), "a knob row is not re-run on every client: the replicated mode carries the change");
		ctx.False(siren.IsOn(), "a fresh jeep has its siren off");

		patrol.PerformAction(knob, driver);
		ctx.True(siren.IsOn(), "an on-duty officer's patrol pick turns the siren on");
		ctx.Equal(1, siren.GetBroadcastCount(), "and the change is broadcast to the clients (replicated mode bumped)");

		account.SetOnDuty(false);
		off.PerformAction(knob, driver);
		ctx.True(siren.IsOn(), "an off-duty driver's pick is refused by the server");
		ctx.Equal(1, siren.GetBroadcastCount(), "and nothing is broadcast");

		siren.OnVehicleDamageState(EDamageState.DESTROYED);
		ctx.False(siren.IsOn(), "a destroyed jeep's siren goes off");
		ctx.Equal(2, siren.GetBroadcastCount(), "and the off is broadcast, throttle or not");
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindKnob(IEntity vehicle)
	{
		IEntity child = vehicle.GetChildren();
		while (child)
		{
			if (child.FindComponent(LF_SirenKnobComponent))
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_SirenModeAction FindAction(IEntity knob, string mode)
	{
		ActionsManagerComponent manager = ActionsManagerComponent.Cast(knob.FindComponent(ActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_SirenModeAction siren = LF_SirenModeAction.Cast(action);
			if (siren && siren.GetModeName() == mode)
				return siren;
		}

		return null;
	}
}
