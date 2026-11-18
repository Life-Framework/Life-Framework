// red-proof: Observed 2026-11-18 with `tools cli test --no-build --red LF_Test_RobCommitGates`: RED PROOF OK; with the hook on the commits get no gate arguments, as on main, so the vehicle and weapon cases spawn or deliver and fail (`a refused weapon robbery inside the cooldown adds no wanted (expected 0, got 5)`); green without it.
//! The vehicle and weapon robbery commits judge the robber on the server (#2538): a robber in grace,
//! a faction that may not rob, too few police on duty and a robbery inside the cooldown each get
//! nothing delivered and no wanted level. A robber who passes every gate still gets the robbery.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RobCommitGates : LF_Test
{
	protected static const ResourceName WEAPON_PREFAB = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
	protected static const ResourceName VEHICLE_PREFAB = "{254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et";

	protected static const float COOLDOWN_S = 300;
	protected static const float LAST_MS = 1000;
	protected static const float NOW_MS = 2000;

	override string GetName()
	{
		return "crime/rob-commit-gates";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccountManager.Reset();

		string uidGrace = "test-rob-gates-grace";
		string uidPolice = "test-rob-gates-police";
		string uidCivilian = "test-rob-gates-civilian";

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		LF_PlayerAccount grace = LF_PlayerAccountManager.GetOrCreate(uidGrace);
		LF_PlayerAccount police = LF_PlayerAccountManager.GetOrCreate(uidPolice);
		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate(uidCivilian);
		ctx.NotNull(grace, "grace robber account exists");
		ctx.NotNull(police, "police account exists");
		ctx.NotNull(civilian, "civilian account exists");
		if (ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(robber);
			return;
		}

		police.SetFaction(LF_Faction.POLICE);

		array<ref LF_GraceClockRow> clocksBefore = LF_GraceCharacterClock.ExportAll();
		LF_GraceCharacterClock.Credit(uidGrace, 60);
		LF_GraceCharacterClock.Credit(uidPolice, 7200);
		LF_GraceCharacterClock.Credit(uidCivilian, 7200);
		LF_Grace.SetMinutesOverride(30);
		ctx.True(LF_Grace.Protects(uidGrace), "the grace robber is protected");
		ctx.False(LF_Grace.Protects(uidCivilian), "the other robbers are not");

		// Vehicle.
		IEntity v1 = RobVehicle(uidGrace, robber, 5, 3, 0, LF_RobCooldown.NEVER);
		ctx.True(v1 == null, "a robber in grace steals no vehicle");
		ctx.Equal(0, grace.GetWantedLevel(), "a refused grace carjacking adds no wanted");

		LF_Grace.SetMinutesOverride(-1);

		IEntity v2 = RobVehicle(uidPolice, robber, 5, 3, 0, LF_RobCooldown.NEVER);
		ctx.True(v2 == null, "a police officer steals no vehicle");
		ctx.Equal(0, police.GetWantedLevel(), "a refused police carjacking adds no wanted");

		IEntity v3 = RobVehicle(uidCivilian, robber, 0, 3, 0, LF_RobCooldown.NEVER);
		ctx.True(v3 == null, "no police on duty: no vehicle");
		ctx.Equal(0, civilian.GetWantedLevel(), "a refused carjacking with no police adds no wanted");

		IEntity v4 = RobVehicle(uidCivilian, robber, 5, 3, COOLDOWN_S, LAST_MS);
		ctx.True(v4 == null, "a carjacking inside the cooldown steals no vehicle");
		ctx.Equal(0, civilian.GetWantedLevel(), "a refused carjacking inside the cooldown adds no wanted");

		// Weapon.
		LF_Grace.SetMinutesOverride(30);
		bool w1 = RobWeapon(uidGrace, robber, 5, 1, 0, LF_RobCooldown.NEVER);
		LF_Grace.SetMinutesOverride(-1);
		ctx.False(w1, "a robber in grace steals no weapon");
		ctx.Equal(0, grace.GetWantedLevel(), "a refused grace weapon robbery adds no wanted");

		ctx.False(RobWeapon(uidPolice, robber, 5, 1, 0, LF_RobCooldown.NEVER), "a police officer steals no weapon");
		ctx.Equal(0, police.GetWantedLevel(), "a refused police weapon robbery adds no wanted");

		ctx.False(RobWeapon(uidCivilian, robber, 0, 1, 0, LF_RobCooldown.NEVER), "no police on duty: no weapon");
		ctx.Equal(0, civilian.GetWantedLevel(), "a refused weapon robbery with no police adds no wanted");

		ctx.False(RobWeapon(uidCivilian, robber, 5, 1, COOLDOWN_S, LAST_MS), "a weapon robbery inside the cooldown steals no weapon");
		ctx.Equal(0, civilian.GetWantedLevel(), "a refused weapon robbery inside the cooldown adds no wanted");
		ctx.False(LF_TestCharacterFixture.HasItem(robber, WEAPON_PREFAB), "no refused robbery put a weapon in the inventory");

		// Control: every gate passes, so the robberies go through (the gates are not just refusing everything).
		LF_Grace.SetMinutesOverride(-1);
		IEntity vOk = LF_RobVehicleAction.CommitVehicleRobbery(uidCivilian, robber, VEHICLE_PREFAB, 5, 3, COOLDOWN_S, LAST_MS - 400000, NOW_MS);
		ctx.True(vOk != null, "a robber who passes every gate steals the vehicle");
		ctx.True(LF_RobWeaponAction.CommitWeaponRobbery(uidCivilian, robber, WEAPON_PREFAB, 5, 1, COOLDOWN_S, LAST_MS - 400000, NOW_MS), "a robber who passes every gate steals the weapon");

		LF_TestCharacterFixture.Delete(vOk);
		LF_TestCharacterFixture.Delete(v1);
		LF_TestCharacterFixture.Delete(v2);
		LF_TestCharacterFixture.Delete(v3);
		LF_TestCharacterFixture.Delete(v4);
		LF_TestCharacterFixture.Delete(robber);
		LF_GraceCharacterClock.ApplyAll(clocksBefore);
		LF_Grace.SetMinutesOverride(-1);
	}

	//------------------------------------------------------------------------------------------------
	//! The red hook calls the commit as main did: no police count, minimum, cooldown or time.
	protected IEntity RobVehicle(string uid, IEntity robber, int police, int minPolice, float cooldownS, float lastMs)
	{
		if (RedProof())
			return LF_RobVehicleAction.CommitVehicleRobbery(uid, robber, VEHICLE_PREFAB);

		return LF_RobVehicleAction.CommitVehicleRobbery(uid, robber, VEHICLE_PREFAB, police, minPolice, cooldownS, lastMs, NOW_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected bool RobWeapon(string uid, IEntity robber, int police, int minPolice, float cooldownS, float lastMs)
	{
		if (RedProof())
			return LF_RobWeaponAction.CommitWeaponRobbery(uid, robber, WEAPON_PREFAB);

		return LF_RobWeaponAction.CommitWeaponRobbery(uid, robber, WEAPON_PREFAB, police, minPolice, cooldownS, lastMs, NOW_MS);
	}
}
