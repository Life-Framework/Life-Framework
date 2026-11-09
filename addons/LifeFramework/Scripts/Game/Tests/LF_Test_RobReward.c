// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`.
// crime/rob-reward: LF_RobAction.PayCash reporting the payout paid without calling AddCash failed
// it with `[LFTEST] FAIL crime/rob-reward`,
// `- payout lands in the robber's inventory as MoneyStack (expected 200, got 0)`.
// crime/rob-weapon-vehicle: LF_RobWeaponAction.WANTED_INCREASE 2 -> 1 failed it with
// `[LFTEST] FAIL crime/rob-weapon-vehicle`, `- weapon robbery raises wanted by 2 (expected 2, got 1)`.
// Both restored; green.

//------------------------------------------------------------------------------------------------
//! The cash robbery through the production server path, with a real character so the payout is
//! physical MoneyStack. LOGIC-tier gate coverage of the same seam (police minimum, payout config,
//! wanted clamp) lives in roadmap/fuel.
// tier: WORLD
class LF_Test_RobReward : LF_Test
{
	override string GetName()
	{
		return "crime/rob-reward";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		LF_PlayerAccountManager.Reset();
		LF_RobAction.ResetSession();

		const int PAYOUT = 200;
		const float COOLDOWN_S = 60;
		string uid = "test-rob-reward";
		string policeUid = "test-rob-reward-police";

		ctx.True(Replication.IsServer(), "rob reward test runs on the server");

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
		{
			LF_TestCharacterFixture.Delete(robber);
			return;
		}

		LF_ATMManager atm = LF_ATMManager.GetInstance();
		ctx.NotNull(atm.CreateAccount(uid), "robber bank account exists before the robbery");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account exists");
		LF_PlayerAccount police = LF_PlayerAccountManager.GetOrCreate(policeUid);
		ctx.NotNull(police, "police account exists");
		if (ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(robber);
			LF_TestCharacterFixture.Delete(officer);
			return;
		}

		police.SetFaction(LF_Faction.POLICE);
		ctx.Equal(0, LF_MoneyUtils.GetCash(robber), "robber starts with no cash");

		float nowMs = GetGame().GetWorld().GetWorldTime() + 1000;
		float last = 0;
		float next = 0;
		int paid = LF_RobAction.CommitRobbery(uid, robber, PAYOUT, 1, 0, 0, COOLDOWN_S, last, nowMs, next);
		ctx.Equal(PAYOUT, paid, "accepted robbery reports the payout");
		ctx.Equal(PAYOUT, LF_MoneyUtils.GetCash(robber), "payout lands in the robber's inventory as MoneyStack");
		ctx.Equal(PAYOUT, LF_InventoryUtils.GetAmount(robber, LF_MoneyUtils.PREFAB_CASH), "the cash is the MoneyStack prefab");
		ctx.EqualFloat(nowMs, next, 0.5, "accepted robbery starts the till cooldown");

		LF_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "bank account still exists after the robbery");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "robbery pays cash, never the bank: balance stays 0");

		ctx.Equal(1, account.GetWantedLevel(), "cash robbery raises wanted by 1");
		ctx.Equal(LF_Faction.CIVILIAN, account.GetFaction(), "robber stays civilian");

		// Cooldown: one second later the same till refuses and nothing moves.
		last = next;
		int second = LF_RobAction.CommitRobbery(uid, robber, PAYOUT, 1, 0, 0, COOLDOWN_S, last, nowMs + 1000, next);
		ctx.Equal(0, second, "robbery inside the cooldown is refused");
		ctx.EqualStr("robbery refused: cooldown", LF_RobAction.GetLastRefusal(), "cooldown refusal is logged");
		ctx.Equal(PAYOUT, LF_MoneyUtils.GetCash(robber), "refused robbery pays no more cash");
		ctx.Equal(1, account.GetWantedLevel(), "refused robbery does not raise wanted");
		ctx.EqualFloat(last, next, 0.5, "refused robbery keeps the original cooldown start");

		// After the cooldown the till pays again.
		int third = LF_RobAction.CommitRobbery(uid, robber, PAYOUT, 1, 0, 0, COOLDOWN_S, last, nowMs + COOLDOWN_S * 1000 + 1, next);
		ctx.Equal(PAYOUT, third, "robbery after the cooldown is accepted");
		ctx.Equal(PAYOUT * 2, LF_MoneyUtils.GetCash(robber), "second accepted robbery adds a second payout");
		ctx.Equal(2, account.GetWantedLevel(), "second accepted robbery raises wanted to 2");

		// Civilian gate: an officer is refused, gets no cash and no wanted.
		int officerPaid = LF_RobAction.CommitRobbery(policeUid, officer, PAYOUT, 1, 0, 0, COOLDOWN_S, 0, nowMs, next);
		ctx.Equal(0, officerPaid, "a police account cannot rob");
		ctx.EqualStr("robbery refused: only civilians can rob", LF_RobAction.GetLastRefusal(), "civilian refusal is logged");
		ctx.Equal(0, LF_MoneyUtils.GetCash(officer), "refused officer receives no cash");
		ctx.Equal(0, police.GetWantedLevel(), "refused officer stays at wanted 0");

		bank = atm.GetAccount(uid);
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "bank stays 0 after every robbery");

		LF_TestCharacterFixture.Delete(robber);
		LF_TestCharacterFixture.Delete(officer);
	}
};

//------------------------------------------------------------------------------------------------
//! Weapon and vehicle robberies through their production server seams. Each delivers the stolen
//! item, raises wanted by its own amount, pays no cash, and refuses non-civilians at the offer gate.
// tier: WORLD
class LF_Test_RobWeaponVehicle : LF_Test
{
	protected static const ResourceName WEAPON_PREFAB = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
	protected static const ResourceName VEHICLE_PREFAB = "{254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et";

	override string GetName()
	{
		return "crime/rob-weapon-vehicle";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		LF_PlayerAccountManager.Reset();

		string uid = "test-rob-weapon-vehicle";
		string policeUid = "test-rob-weapon-vehicle-police";

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		LF_ATMManager atm = LF_ATMManager.GetInstance();
		ctx.NotNull(atm.CreateAccount(uid), "robber bank account exists");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account exists");
		LF_PlayerAccount police = LF_PlayerAccountManager.GetOrCreate(policeUid);
		ctx.NotNull(police, "police account exists");
		if (ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(robber);
			return;
		}

		police.SetFaction(LF_Faction.POLICE);

		// Offer gate shared by both actions' CanBePerformedScript.
		ctx.True(LF_RobAction.IsFactionAllowedToRob(uid), "a civilian may rob");
		ctx.False(LF_RobAction.IsFactionAllowedToRob(policeUid), "a police officer may not rob");
		ctx.True(LF_RobAction.IsFactionAllowedToRob("test-rob-no-account"), "a player with no account is not refused by the faction gate");

		int cashBefore = LF_MoneyUtils.GetCash(robber);

		ctx.True(LF_RobWeaponAction.CommitWeaponRobbery(uid, robber, WEAPON_PREFAB), "weapon robbery delivers the weapon");
		ctx.Equal(2, account.GetWantedLevel(), "weapon robbery raises wanted by 2");
		ctx.True(LF_TestCharacterFixture.HasItem(robber, WEAPON_PREFAB), "the stolen weapon is in the robber's inventory");
		ctx.False(LF_RobWeaponAction.CommitWeaponRobbery(uid, null, WEAPON_PREFAB), "weapon robbery without a user delivers nothing");
		ctx.Equal(2, account.GetWantedLevel(), "an undelivered weapon raises no wanted");

		IEntity vehicle = LF_RobVehicleAction.CommitVehicleRobbery(uid, robber, VEHICLE_PREFAB);
		ctx.True(vehicle != null, "vehicle robbery spawns the vehicle");
		if (vehicle)
			ctx.True(vector.Distance(vehicle.GetOrigin(), robber.GetOrigin()) < 10, "the stolen vehicle spawns next to the robber");
		ctx.Equal(5, account.GetWantedLevel(), "vehicle robbery raises wanted by 3 (2 + 3 = 5)");
		ctx.True(LF_RobVehicleAction.CommitVehicleRobbery(uid, null, VEHICLE_PREFAB) == null, "vehicle robbery without a user spawns nothing");

		IEntity extra = LF_RobVehicleAction.CommitVehicleRobbery(uid, robber, VEHICLE_PREFAB);
		ctx.Equal(5, account.GetWantedLevel(), "stacked robberies clamp wanted at 5");

		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(robber), "weapon and vehicle robberies pay no cash");
		LF_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "bank account exists after the robberies");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "weapon and vehicle robberies never touch the bank");

		LF_TestCharacterFixture.Delete(extra);
		LF_TestCharacterFixture.Delete(vehicle);
		LF_TestCharacterFixture.Delete(robber);
	}
};
