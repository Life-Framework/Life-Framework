// red-proof: PENDING - not yet observed. The Arma Reforger Server install was being restored
// when this test was rewritten, so `tools\cli test --no-build --tier all` could not boot.
// Planned perturbations: crime/rob-reward - deposit the payout to EL_ATMManager in
// EL_RobAction.CommitRobbery (bank-0 assertion goes red) or drop the cooldown gate
// (cooldown assertions go red). crime/rob-weapon-vehicle - change
// EL_RobWeaponAction.WANTED_INCREASE to 1 (wanted-2 assertion goes red) or drop the
// IsFactionAllowedToRob faction check (police assertion goes red).

//------------------------------------------------------------------------------------------------
//! The cash robbery through the production server path, with a real character so the payout is
//! physical MoneyStack. LOGIC-tier gate coverage of the same seam (police minimum, payout config,
//! wanted clamp) lives in roadmap/fuel.
// tier: WORLD
class EL_Test_RobReward : EL_Test
{
	override string GetName()
	{
		return "crime/rob-reward";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_PlayerAccountManager.Reset();
		EL_RobAction.ResetSession();

		const int PAYOUT = 200;
		const float COOLDOWN_S = 60;
		string uid = "test-rob-reward";
		string policeUid = "test-rob-reward-police";

		ctx.True(Replication.IsServer(), "rob reward test runs on the server");

		IEntity robber = EL_TestCharacterFixture.Spawn(ctx, true);
		if (!robber)
			return;

		IEntity officer = EL_TestCharacterFixture.Spawn(ctx, true);
		if (!officer)
		{
			EL_TestCharacterFixture.Delete(robber);
			return;
		}

		EL_ATMManager atm = EL_ATMManager.GetInstance();
		ctx.NotNull(atm.CreateAccount(uid), "robber bank account exists before the robbery");
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account exists");
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		ctx.NotNull(police, "police account exists");
		if (ctx.FailureCount() > 0)
		{
			EL_TestCharacterFixture.Delete(robber);
			EL_TestCharacterFixture.Delete(officer);
			return;
		}

		police.SetFaction(EL_Faction.POLICE);
		ctx.Equal(0, EL_MoneyUtils.GetCash(robber), "robber starts with no cash");

		float nowMs = GetGame().GetWorld().GetWorldTime() + 1000;
		float last = 0;
		float next = 0;
		int paid = EL_RobAction.CommitRobbery(uid, robber, PAYOUT, 1, 0, 0, COOLDOWN_S, last, nowMs, next);
		ctx.Equal(PAYOUT, paid, "accepted robbery reports the payout");
		ctx.Equal(PAYOUT, EL_MoneyUtils.GetCash(robber), "payout lands in the robber's inventory as MoneyStack");
		ctx.Equal(PAYOUT, EL_InventoryUtils.GetAmount(robber, EL_MoneyUtils.PREFAB_CASH), "the cash is the MoneyStack prefab");
		ctx.EqualFloat(nowMs, next, 0.5, "accepted robbery starts the till cooldown");

		EL_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "bank account still exists after the robbery");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "robbery pays cash, never the bank: balance stays 0");

		ctx.Equal(1, account.GetWantedLevel(), "cash robbery raises wanted by 1");
		ctx.Equal(EL_Faction.CIVILIAN, account.GetFaction(), "robber stays civilian");

		// Cooldown: one second later the same till refuses and nothing moves.
		last = next;
		int second = EL_RobAction.CommitRobbery(uid, robber, PAYOUT, 1, 0, 0, COOLDOWN_S, last, nowMs + 1000, next);
		ctx.Equal(0, second, "robbery inside the cooldown is refused");
		ctx.EqualStr("robbery refused: cooldown", EL_RobAction.GetLastRefusal(), "cooldown refusal is logged");
		ctx.Equal(PAYOUT, EL_MoneyUtils.GetCash(robber), "refused robbery pays no more cash");
		ctx.Equal(1, account.GetWantedLevel(), "refused robbery does not raise wanted");
		ctx.EqualFloat(last, next, 0.5, "refused robbery keeps the original cooldown start");

		// After the cooldown the till pays again.
		int third = EL_RobAction.CommitRobbery(uid, robber, PAYOUT, 1, 0, 0, COOLDOWN_S, last, nowMs + COOLDOWN_S * 1000 + 1, next);
		ctx.Equal(PAYOUT, third, "robbery after the cooldown is accepted");
		ctx.Equal(PAYOUT * 2, EL_MoneyUtils.GetCash(robber), "second accepted robbery adds a second payout");
		ctx.Equal(2, account.GetWantedLevel(), "second accepted robbery raises wanted to 2");

		// Civilian gate: an officer is refused, gets no cash and no wanted.
		int officerPaid = EL_RobAction.CommitRobbery(policeUid, officer, PAYOUT, 1, 0, 0, COOLDOWN_S, 0, nowMs, next);
		ctx.Equal(0, officerPaid, "a police account cannot rob");
		ctx.EqualStr("robbery refused: only civilians can rob", EL_RobAction.GetLastRefusal(), "civilian refusal is logged");
		ctx.Equal(0, EL_MoneyUtils.GetCash(officer), "refused officer receives no cash");
		ctx.Equal(0, police.GetWantedLevel(), "refused officer stays at wanted 0");

		bank = atm.GetAccount(uid);
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "bank stays 0 after every robbery");

		EL_TestCharacterFixture.Delete(robber);
		EL_TestCharacterFixture.Delete(officer);
	}
};

//------------------------------------------------------------------------------------------------
//! Weapon and vehicle robberies through their production server seams. Each delivers the stolen
//! item, raises wanted by its own amount, pays no cash, and refuses non-civilians at the offer gate.
// tier: WORLD
class EL_Test_RobWeaponVehicle : EL_Test
{
	protected static const ResourceName WEAPON_PREFAB = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
	protected static const ResourceName VEHICLE_PREFAB = "{254289B9C09904AB}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et";

	override string GetName()
	{
		return "crime/rob-weapon-vehicle";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		EL_PlayerAccountManager.Reset();

		string uid = "test-rob-weapon-vehicle";
		string policeUid = "test-rob-weapon-vehicle-police";

		IEntity robber = EL_TestCharacterFixture.Spawn(ctx, true);
		if (!robber)
			return;

		EL_ATMManager atm = EL_ATMManager.GetInstance();
		ctx.NotNull(atm.CreateAccount(uid), "robber bank account exists");
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account exists");
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		ctx.NotNull(police, "police account exists");
		if (ctx.FailureCount() > 0)
		{
			EL_TestCharacterFixture.Delete(robber);
			return;
		}

		police.SetFaction(EL_Faction.POLICE);

		// Offer gate shared by both actions' CanBePerformedScript.
		ctx.True(EL_RobAction.IsFactionAllowedToRob(uid), "a civilian may rob");
		ctx.False(EL_RobAction.IsFactionAllowedToRob(policeUid), "a police officer may not rob");
		ctx.True(EL_RobAction.IsFactionAllowedToRob("test-rob-no-account"), "a player with no account is not refused by the faction gate");

		int cashBefore = EL_MoneyUtils.GetCash(robber);

		ctx.True(EL_RobWeaponAction.CommitWeaponRobbery(uid, robber, WEAPON_PREFAB), "weapon robbery delivers the weapon");
		ctx.Equal(2, account.GetWantedLevel(), "weapon robbery raises wanted by 2");
		ctx.True(EL_TestCharacterFixture.HasItem(robber, WEAPON_PREFAB), "the stolen weapon is in the robber's inventory");
		ctx.False(EL_RobWeaponAction.CommitWeaponRobbery(uid, null, WEAPON_PREFAB), "weapon robbery without a user delivers nothing");
		ctx.Equal(2, account.GetWantedLevel(), "an undelivered weapon raises no wanted");

		IEntity vehicle = EL_RobVehicleAction.CommitVehicleRobbery(uid, robber, VEHICLE_PREFAB);
		ctx.True(vehicle != null, "vehicle robbery spawns the vehicle");
		if (vehicle)
			ctx.True(vector.Distance(vehicle.GetOrigin(), robber.GetOrigin()) < 10, "the stolen vehicle spawns next to the robber");
		ctx.Equal(5, account.GetWantedLevel(), "vehicle robbery raises wanted by 3 (2 + 3 = 5)");
		ctx.True(EL_RobVehicleAction.CommitVehicleRobbery(uid, null, VEHICLE_PREFAB) == null, "vehicle robbery without a user spawns nothing");

		IEntity extra = EL_RobVehicleAction.CommitVehicleRobbery(uid, robber, VEHICLE_PREFAB);
		ctx.Equal(5, account.GetWantedLevel(), "stacked robberies clamp wanted at 5");

		ctx.Equal(cashBefore, EL_MoneyUtils.GetCash(robber), "weapon and vehicle robberies pay no cash");
		EL_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "bank account exists after the robberies");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "weapon and vehicle robberies never touch the bank");

		EL_TestCharacterFixture.Delete(extra);
		EL_TestCharacterFixture.Delete(vehicle);
		EL_TestCharacterFixture.Delete(robber);
	}
};
