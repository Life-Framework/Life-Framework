// red-proof: expected wanted 0 after the first till robbery and ran
// `tools\cli test --no-build --tier fast` on 2026-10-11. The suite reported
// `[LFTEST] - wanted up by the configured amount (expected 0, got 1)`.
// Restored the expected value to 1.

// tier: LOGIC
class LF_Test_Roadmap_Fuel : LF_Test
{
	override string GetName()
	{
		return "roadmap/fuel";
	}

	override void Run(LF_TestContext ctx)
	{
		// The DebugWorld till pins min and max at 150 so the roll is this amount.
		const int PAYOUT = 150;

		ctx.True(Replication.IsServer(), "fuel till test runs on the server");
		ctx.Equal(PAYOUT, LF_RobAction.ResolvePayout(0, PAYOUT, PAYOUT, 0), "fixed range pays the configured cash");
		ctx.Equal(PAYOUT, LF_RobAction.ResolvePayout(PAYOUT, 0, 0, -1), "an unset range pays the single cash amount");
		ctx.Equal(100, LF_RobAction.ResolvePayout(0, 100, 140, 0), "a rolled range starts at the minimum");
		ctx.Equal(140, LF_RobAction.ResolvePayout(0, 100, 140, 40), "a rolled range ends at the maximum");
		ctx.Equal(0, LF_RobAction.ResolvePayout(0, 140, 100, 0), "an inverted range pays nothing");
		ctx.False(LF_RobAction.IsCooldownActive(0, 60, 1000), "a till that has not been robbed is not on cooldown");
		ctx.True(LF_RobAction.IsCooldownActive(100000, 60, 101000), "one second is inside a 60 second cooldown");
		ctx.False(LF_RobAction.IsCooldownActive(100000, 60, 160000), "sixty seconds clears the cooldown");

		LF_RobAction.ResetSession();
		string uid = "roadmap-fuel-logic";
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		ctx.NotNull(atm.CreateAccount(uid), "robber bank account exists");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber civilian account exists");
		ctx.Equal(0, account.GetWantedLevel(), "account starts clean");

		float last = 0;
		float next = 0;
		int paid = LF_RobAction.CommitRobbery(uid, null, PAYOUT, 0, 0, 60, last, 100000, next);
		ctx.Equal(PAYOUT, paid, "cash up by the fixed payout");
		ctx.Equal(PAYOUT, LF_RobAction.GetCashPaid(), "cash paid tracks the payout");
		LF_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "bank account still exists after the robbery");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "bank unchanged");

		LF_PlayerAccount cached = LF_PlayerAccountManager.GetInstance().GetFromCache(uid);
		ctx.NotNull(cached, "account stays cached after the robbery");
		if (cached)
			ctx.Equal(1, cached.GetWantedLevel(), "wanted up by the configured amount");

		ctx.Equal(1, LF_RobAction.GetAlertCount(), "one police alert");
		last = next;

		int second = LF_RobAction.CommitRobbery(uid, null, PAYOUT, 0, 0, 60, last, 101000, next);
		ctx.Equal(0, second, "cooldown refuses");
		ctx.Equal(PAYOUT, LF_RobAction.GetCashPaid(), "refused attempt does not pay again");
		ctx.EqualStr("robbery refused: cooldown", LF_RobAction.GetLastRefusal(), "cooldown refusal is logged");
		ctx.Equal(1, LF_RobAction.GetAlertCount(), "refused attempt does not alert again");
		cached = LF_PlayerAccountManager.GetInstance().GetFromCache(uid);
		if (cached)
			ctx.Equal(1, cached.GetWantedLevel(), "refused attempt does not raise wanted");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "refused attempt does not touch the bank");

		LF_RobAction.ResetSession();
		int blocked = LF_RobAction.CommitRobbery(uid, null, PAYOUT, 0, 2, 60, 0, 200000, next);
		ctx.Equal(0, blocked, "minimum police gate refuses");
		ctx.EqualStr("robbery refused: not enough police on duty", LF_RobAction.GetLastRefusal(), "police refusal is logged");
		ctx.Equal(0, LF_RobAction.GetCashPaid(), "police refusal pays nothing");
		ctx.Equal(0, LF_RobAction.GetAlertCount(), "police refusal does not alert");

		string policeUid = "roadmap-fuel-police";
		LF_PlayerAccount police = LF_PlayerAccountManager.GetOrCreate(policeUid);
		police.SetFaction(LF_Faction.POLICE);
		atm.CreateAccount(policeUid);
		LF_RobAction.ResetSession();
		int civilianGate = LF_RobAction.CommitRobbery(policeUid, null, PAYOUT, 1, 0, 60, 0, 300000, next);
		ctx.Equal(0, civilianGate, "police faction refuses");
		ctx.EqualStr("robbery refused: only civilians can rob", LF_RobAction.GetLastRefusal(), "civilian refusal is logged");
		ctx.Equal(0, police.GetWantedLevel(), "refused officer stays at wanted 0");

		LF_RobAction.ResetSession();
		int unconfigured = LF_RobAction.CommitRobbery(uid, null, 0, 0, 0, 60, 0, 400000, next);
		ctx.Equal(0, unconfigured, "unconfigured payout refuses");
		ctx.EqualStr("robbery refused: payout is not configured", LF_RobAction.GetLastRefusal(), "unconfigured payout is logged");

		string clampUid = "roadmap-fuel-clamp";
		atm.CreateAccount(clampUid);
		LF_PlayerAccountManager.GetOrCreate(clampUid);
		LF_RobAction.ResetSession();
		float clampLast = 0;
		float clampNext = 0;
		for (int i = 0; i < 6; i++)
		{
			int clampPaid = LF_RobAction.CommitRobbery(clampUid, null, PAYOUT, 0, 0, 0, clampLast, 500000 + i, clampNext);
			clampLast = clampNext;
			ctx.Equal(PAYOUT, clampPaid, "cooldown of 0 still pays cash");
		}

		ctx.Equal(PAYOUT * 6, LF_RobAction.GetCashPaid(), "six robberies pay cash six times");
		LF_BankAccount clampBank = atm.GetAccount(clampUid);
		ctx.NotNull(clampBank, "clamp bank account exists");
		if (clampBank)
			ctx.Equal(0, clampBank.GetBalance(), "repeated robberies leave the bank at 0");

		LF_PlayerAccount clampCached = LF_PlayerAccountManager.GetInstance().GetFromCache(clampUid);
		ctx.NotNull(clampCached, "clamp account stays cached");
		if (clampCached)
			ctx.Equal(5, clampCached.GetWantedLevel(), "wanted clamps at 5");

		ctx.Equal(6, LF_RobAction.GetAlertCount(), "each accepted robbery alerts once");
	}
}

// tier: WORLD
class LF_Test_Roadmap_FuelWorld : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName FUEL_STATION_PREFAB = "{9269BD1B6BF3A441}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/FuelStation_E_01_V1.et";

	override string GetName()
	{
		return "e2e/roadmap-fuel";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource station = Resource.Load(FUEL_STATION_PREFAB);
		ctx.True(station.IsValid(), "FuelStation_E_01_V1.et loads");
		Resource cash = Resource.Load(LF_MoneyUtils.PREFAB_CASH);
		ctx.True(cash.IsValid(), "MoneyStack.et loads");

		IEntity till = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.FUEL_TILL);
		ctx.True(till != null, "Annex_Fuel_Till is placed");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(till);
		ctx.NotNull(actions, "till carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		LF_RobAction rob;
		foreach (BaseUserAction action : actionList)
		{
			LF_RobAction candidate = LF_RobAction.Cast(action);
			if (candidate)
				rob = candidate;
		}

		ctx.NotNull(rob, "till uses LF_RobAction");
		if (ctx.FailureCount() > 0)
			return;

		int payout = rob.GetConfiguredPayout();
		ctx.Equal(150, payout, "till pays the fixed test amount");

		string actionName;
		ctx.True(rob.GetActionNameScript(actionName), "till action name is scripted");
		ctx.EqualStr("#EL-Fuel_Till_Rob", actionName, "till action name is the localization key");

		IEntity character = SpawnRobber(ctx);
		if (ctx.FailureCount() > 0)
			return;

		// A spawned body is not a connected player, so it has no account. The server refuses
		// it (no civilian gate, no wanted bump, so no cash); the paying path is roadmap/fuel.
		int before = LF_MoneyUtils.GetCash(character);
		int alertsBefore = LF_RobAction.GetAlertCount();
		ctx.False(rob.CanBePerformedScript(character), "a character with no account is not offered the till");
		rob.PerformAction(till, character);
		ctx.Equal(before, LF_MoneyUtils.GetCash(character), "a character with no account is paid nothing");
		ctx.EqualStr("robbery refused: no account", LF_RobAction.GetLastRefusal(), "the no-account refusal is logged");
		ctx.Equal(alertsBefore, LF_RobAction.GetAlertCount(), "a character with no account does not alert");

		string uid = "roadmap-fuel-e2e";
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		atm.CreateAccount(uid);
		LF_BankAccount bank = atm.GetAccount(uid);
		ctx.NotNull(bank, "e2e bank account exists");
		if (bank)
			ctx.Equal(0, bank.GetBalance(), "a refused robbery does not change the bank");
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnRobber(LF_TestContext ctx)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads");
		if (ctx.FailureCount() > 0)
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();

		ctx.True(spawnPoint != null, "a spawn point resolves for the robber");
		if (ctx.FailureCount() > 0)
			return null;

		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(character != null, "robber spawns");
		if (ctx.FailureCount() > 0)
			return null;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.NotNull(storage, "robber resolves an inventory storage manager");
		if (ctx.FailureCount() > 0)
			return null;

		IEntity jacket = LF_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.True(jacket != null, "jacket spawns for the robber pockets");
		if (ctx.FailureCount() > 0)
			return null;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket inserts into the robber loadout");
		return character;
	}
}
