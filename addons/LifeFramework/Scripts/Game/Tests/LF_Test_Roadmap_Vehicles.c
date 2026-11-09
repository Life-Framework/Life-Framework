// red-proof: expected Y for LetterAt(25) and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[LFTEST]   - the alphabet includes Z (expected 'Y', got 'Z')`
// with SUMMARY tier=fast passed=55 failed=1 total=56.
// Restored the expected value to Z.

// tier: LOGIC
class LF_Test_Roadmap_Vehicles : LF_Test
{
	override string GetName()
	{
		return "roadmap/vehicles";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("A", LF_LicensePlateGeneratorGeneric.LetterAt(0), "the alphabet starts at A");
		ctx.EqualStr("Z", LF_LicensePlateGeneratorGeneric.LetterAt(25), "the alphabet includes Z");
		ctx.EqualStr("", LF_LicensePlateGeneratorGeneric.LetterAt(-1), "a letter before A is refused");
		ctx.EqualStr("", LF_LicensePlateGeneratorGeneric.LetterAt(26), "a letter after Z is refused");
		ctx.EqualStr("ZZ ZZ 9999", LF_LicensePlateGeneratorGeneric.ComposePlate(25, 25, 25, 25, 9999), "the top number is 9999");
		ctx.EqualStr("", LF_LicensePlateGeneratorGeneric.ComposePlate(25, 25, 25, 25, 99), "numbers below 100 are refused");
		ctx.EqualStr("", LF_LicensePlateGeneratorGeneric.ComposePlate(25, 25, 25, 25, 10000), "numbers above 9999 are refused");
		ctx.Equal(26, LF_LicensePlateGeneratorGeneric.LetterRollCount(), "the letter roll includes index 25");
		ctx.Equal(10000, LF_LicensePlateGeneratorGeneric.NumberRollEnd(), "the number roll includes 9999");

		LF_LicensePlateRegistry.Remember("AA BB 1000", "veh-a");
		ctx.True(LF_LicensePlateRegistry.IsLive("AA BB 1000"), "a remembered plate is live");
		ctx.EqualStr("", LF_LicensePlateRegistry.TakeUnique("AA BB 1000", "veh-b"), "a second issue does not reuse a live plate");
		ctx.EqualStr("ZZ ZZ 9999", LF_LicensePlateRegistry.TakeUnique("ZZ ZZ 9999", "veh-b"), "a free plate is issued");
		ctx.True(LF_LicensePlateRegistry.IsLive("ZZ ZZ 9999"), "the free plate is now live");

		int cashDebit = 0;
		int bankDebit = 0;
		ctx.True(LF_VehicleOwnership.QuotePurchase(100, 500, 100, 0, cashDebit, bankDebit), "cash covers a small price");
		ctx.Equal(100, cashDebit, "a small purchase debits cash");
		ctx.Equal(0, bankDebit, "a small purchase does not touch the bank");

		ctx.False(LF_VehicleOwnership.QuotePurchase(100, 500, 99, 500, cashDebit, bankDebit), "short cash refuses a small purchase");
		ctx.Equal(0, cashDebit, "a refused small purchase debits no cash");
		ctx.Equal(0, bankDebit, "a refused small purchase does not fall through to the bank");

		ctx.True(LF_VehicleOwnership.QuotePurchase(1000, 500, 5000, 1000, cashDebit, bankDebit), "the bank covers a large price");
		ctx.Equal(0, cashDebit, "a large purchase does not take cash");
		ctx.Equal(1000, bankDebit, "a large purchase debits the bank");

		ctx.False(LF_VehicleOwnership.QuotePurchase(1000, 500, 5000, 999, cashDebit, bankDebit), "short bank refuses a large purchase");
		ctx.Equal(0, cashDebit, "a refused large purchase still does not take cash");
		ctx.Equal(0, bankDebit, "a refused large purchase debits no bank");

		ctx.False(LF_VehicleOwnership.QuotePurchase(0, 500, 100, 100, cashDebit, bankDebit), "an unconfigured price refuses");

		string id = "roadmap-vehicles-logic";
		string owner = "roadmap-vehicles-owner";
		ctx.True(LF_VehicleOwnership.Register(id, owner, "key-logic", LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "AA BB 1100", "#EL-Vehicle_S105", true), "a vehicle record registers");
		ctx.False(LF_VehicleOwnership.CanIssueExtraKey(id, "roadmap-vehicles-other"), "a stranger cannot cut a key");
		ctx.True(LF_VehicleOwnership.CanIssueExtraKey(id, owner), "the owner can cut a key");

		LF_VehicleRecord saved = LF_VehicleOwnership.Export(id);
		ctx.NotNull(saved, "export returns the record");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(owner, saved.m_sOwnerUid, "exported owner matches");
		ctx.EqualStr("key-logic", saved.m_sKeyId, "exported key matches");
		LF_VehicleOwnership.NoteLockState(id, false);
		ctx.False(LF_VehicleOwnership.IsLocked(id), "the live lock can change");
		ctx.True(LF_VehicleOwnership.Apply(saved), "apply restores the saved record");
		ctx.True(LF_VehicleOwnership.IsLocked(id), "apply restores the lock flag");
		ctx.EqualStr(owner, LF_VehicleOwnership.GetOwner(id), "apply restores the owner");
		ctx.EqualStr("key-logic", LF_VehicleOwnership.GetKeyId(id), "apply restores the key id");
		ctx.True(LF_VehicleOwnership.Apply(saved), "a second apply converges");
		ctx.EqualStr("key-logic", LF_VehicleOwnership.GetKeyId(id), "a second apply does not change the key");

		ctx.False(LF_VehicleOwnership.Store(id, "roadmap-vehicles-other"), "a stranger cannot store the vehicle");
		ctx.True(LF_VehicleOwnership.Store(id, owner), "the owner can store the vehicle");
		ctx.True(LF_VehicleOwnership.IsStored(id), "the record is stored");
		LF_VehicleRecord stored = LF_VehicleOwnership.Export(id);
		ctx.NotNull(stored, "export returns the stored record");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(stored.m_bStored, "the saved record is stored");
		ctx.True(stored.m_bLocked, "the saved record keeps the lock flag");
		ctx.EqualStr(LF_VehicleCatalog.S105, stored.m_sPrefab, "the saved record keeps the vanilla prefab");
		ctx.True(LF_VehicleOwnership.RestoreRecord(id, owner), "retrieve clears the stored flag");
		ctx.False(LF_VehicleOwnership.IsStored(id), "the record is back in the world");
		ctx.True(LF_VehicleOwnership.Apply(stored), "apply-save puts it back in the garage");
		ctx.True(LF_VehicleOwnership.IsStored(id), "the applied record is stored");
		ctx.True(LF_VehicleOwnership.RestoreRecord(id, owner), "retrieve after apply-save");
		ctx.EqualStr(LF_VehicleCatalog.S105, LF_VehicleOwnership.GetPrefab(id), "retrieve keeps the prefab");
		ctx.True(LF_VehicleOwnership.IsLocked(id), "retrieve keeps the lock flag");
		ctx.EqualStr(owner, LF_VehicleOwnership.GetOwner(id), "retrieve keeps the owner");
		ctx.EqualStr("key-logic", LF_VehicleOwnership.GetKeyId(id), "retrieve keeps the key id");

		LF_VehicleRecord unknown = LF_VehicleRecord.Create("roadmap-vehicles-missing", owner, "key-logic", LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "", "#EL-Vehicle_S105", true, false);
		ctx.False(LF_VehicleOwnership.Apply(unknown), "a record for an unknown vehicle is rejected");
	}
}

// tier: WORLD
class LF_Test_Roadmap_VehiclesWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/roadmap-vehicles";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource s105 = Resource.Load(LF_VehicleCatalog.S105);
		Resource s105Locked = Resource.Load(LF_VehicleCatalog.S105_LOCKED);
		Resource uaz = Resource.Load(LF_VehicleCatalog.UAZ469);
		Resource van = Resource.Load(LF_VehicleCatalog.S1203);
		Resource uaz452 = Resource.Load(LF_VehicleCatalog.UAZ452);
		Resource truck = Resource.Load(LF_VehicleCatalog.M923);
		Resource garage = Resource.Load(LF_VehicleGarageComponent.GARAGE_SHELL);
		Resource keyPrefab = Resource.Load(LF_VehicleCatalog.KEY);
		ctx.True(s105.IsValid(), "S105_beige.et loads");
		ctx.True(s105Locked.IsValid(), "the locked S105 child loads");
		ctx.True(uaz.IsValid(), "UAZ469_covered_CIV_blue.et loads");
		ctx.True(van.IsValid(), "S1203_transport_beige.et loads");
		ctx.True(uaz452.IsValid(), "UAZ452_base.et loads");
		ctx.True(truck.IsValid(), "M923A1_transport.et loads");
		ctx.True(garage.IsValid(), "Garage_E_01.et loads");
		ctx.True(keyPrefab.IsValid(), "VehicleKey.et loads");

		IEntity display = GetGame().GetWorld().FindEntityByName("Roadmap_Vehicle_S105");
		ctx.True(display != null, "Roadmap_Vehicle_S105 is placed");
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName("Roadmap_Vehicle_Lot");
		ctx.True(lotEntity != null, "Roadmap_Vehicle_Lot is placed");
		if (ctx.FailureCount() > 0)
			return;

		string displayPrefab = LF_Utils.GetPrefabName(display);
		ctx.True(displayPrefab.Contains("S105"), "the showroom body is an S105");
		ctx.NotNull(LF_Component<LF_VehicleLockComponent>.Find(display), "the showroom S105 carries LF_VehicleLockComponent");

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot carries LF_VehicleLotComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(5, lot.GetOfferCount(), "the lot lists the catalog");
		ctx.Equal(500, lot.GetCashCeiling(), "the cash ceiling is configured");
		ctx.Equal(1000, lot.GetPrice(LF_VehicleCatalog.S105), "the S105 price is above the cash ceiling");
		ctx.True(LF_Component<LF_VehicleGarageComponent>.Find(lotEntity) == null, "the lot sells; it is not a garage");
		ctx.True(LF_VehicleSpawnPointComponent.CountForShop("roadmap-lot") > 0, "the lot has a spawn pad");
		LF_VehicleOffer firstOffer = lot.GetOfferAt(0);
		ctx.NotNull(firstOffer, "the lot has a first offer");
		if (firstOffer)
			ctx.EqualStr("#EL-Vehicle_S105", firstOffer.GetNameKey(), "the first offer is the S105");

		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(lotEntity);
		ctx.NotNull(actions, "the lot carries actions");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool buyWired = false;
		bool keyWired = false;
		bool storeWired = false;
		bool retrieveWired = false;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_VehicleBuyAction.Cast(action))
				buyWired = true;
			if (LF_VehicleExtraKeyAction.Cast(action))
				keyWired = true;
			if (LF_VehicleStoreAction.Cast(action))
				storeWired = true;
			if (LF_VehicleRetrieveAction.Cast(action))
				retrieveWired = true;
		}

		ctx.True(buyWired, "buy action is on the lot");
		ctx.True(keyWired, "extra key action is on the lot");
		ctx.False(storeWired, "store is at the garage, not the lot");
		ctx.False(retrieveWired, "retrieve is at the garage, not the lot");

		string uid = "roadmap-vehicles-e2e";
		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "the buyer has a bank account");
		if (ctx.FailureCount() > 0)
			return;

		int start = account.GetBalance();
		int price = lot.GetPrice(LF_VehicleCatalog.S105);
		account.Deposit(price);

		string vehicleId;
		int cashCharged;
		string reasonKey;
		ctx.True(lot.Buy(uid, LF_VehicleCatalog.S105, null, vehicleId, cashCharged, reasonKey), "buy spawns the S105");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(0, cashCharged, "the configured S105 price is not taken as cash");
		ctx.Equal(start, account.GetBalance(), "purchase debits the configured price");
		ctx.EqualStr(uid, LF_VehicleOwnership.GetOwner(vehicleId), "the buyer is the owner");
		ctx.EqualStr(LF_VehicleCatalog.S105, LF_VehicleOwnership.GetPrefab(vehicleId), "the record stores the vanilla S105");
		ctx.False(LF_VehicleOwnership.GetPlate(vehicleId).IsEmpty(), "purchase issued a plate");

		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		ctx.True(vehicle != null, "the purchased vehicle is in the world");
		if (ctx.FailureCount() > 0)
			return;

		string spawnedPrefab = LF_Utils.GetPrefabName(vehicle);
		ctx.True(spawnedPrefab.Contains("S105"), "buy spawns the S105 prefab");

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		ctx.NotNull(vehicleLock, "the purchased S105 carries LF_VehicleLockComponent");
		IEntity key = LF_VehicleOwnership.GetKey(vehicleId);
		ctx.True(key != null, "purchase gives a key");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(vehicleLock.IsVehicleLocked(), "the purchased vehicle starts locked");
		ctx.EqualStr(vehicleLock.GetVehicleIdentifier(), LF_VehicleOwnership.GetKeyId(vehicleId), "the key id is the lock identifier");

		IEntity wrong = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.KEY, "560 0 124");
		ctx.True(wrong != null, "a wrong key can be spawned");
		if (ctx.FailureCount() > 0)
			return;

		LF_VehicleKeyComponent wrongKey = LF_Component<LF_VehicleKeyComponent>.Find(wrong);
		ctx.NotNull(wrongKey, "the wrong key carries LF_VehicleKeyComponent");
		if (ctx.FailureCount() > 0)
			return;

		wrongKey.BindToVehicleIdentifier("roadmap-vehicles-wrong");
		ctx.False(LF_VehicleOwnership.TryUnlock(vehicleId, wrong), "the wrong key does not unlock");
		ctx.True(vehicleLock.IsVehicleLocked(), "the vehicle stays locked after the wrong key");
		ctx.True(LF_VehicleOwnership.TryUnlock(vehicleId, key), "the matching key unlocks");
		ctx.False(vehicleLock.IsVehicleLocked(), "the matching key cleared the lock");
	}
}

// tier: PERSISTENCE
class LF_Test_Roadmap_VehiclesPersistence : LF_Test
{
	override string GetName()
	{
		return "roadmap/vehicles-save";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName("Roadmap_Vehicle_Lot");
		ctx.True(lotEntity != null, "Roadmap_Vehicle_Lot is placed for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot is present for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		string uid = "roadmap-vehicles-save";
		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "the save buyer has a bank account");
		if (ctx.FailureCount() > 0)
			return;

		int price = lot.GetPrice(LF_VehicleCatalog.S105);
		account.Deposit(price);

		string vehicleId;
		int cashCharged;
		string reasonKey;
		ctx.True(lot.Buy(uid, LF_VehicleCatalog.S105, null, vehicleId, cashCharged, reasonKey), "buy before the save");
		if (ctx.FailureCount() > 0)
			return;

		LF_VehicleRecord saved = LF_VehicleOwnership.Export(vehicleId);
		ctx.NotNull(saved, "export returns the owned vehicle");
		if (ctx.FailureCount() > 0)
			return;

		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		IEntity key = LF_VehicleOwnership.GetKey(vehicleId);
		ctx.True(vehicle != null, "the vehicle is live before the save");
		ctx.True(key != null, "the key is live before the save");
		if (ctx.FailureCount() > 0)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(key);
		ctx.NotNull(vehicleLock, "the live vehicle has a lock");
		ctx.NotNull(keyComponent, "the live key has a key component");
		if (ctx.FailureCount() > 0)
			return;

		vehicleLock.SetVehicleIdentifier("mutated");
		keyComponent.BindToVehicleIdentifier("mutated");
		ctx.True(LF_VehicleOwnership.Apply(saved), "apply-save restores the record");
		ctx.EqualStr(uid, LF_VehicleOwnership.GetOwner(vehicleId), "owner id matches after apply-save");
		ctx.EqualStr(saved.m_sKeyId, LF_VehicleOwnership.GetKeyId(vehicleId), "key id matches after apply-save");
		ctx.EqualStr(saved.m_sKeyId, vehicleLock.GetVehicleIdentifier(), "the lock identifier matches the saved key");
		ctx.EqualStr(saved.m_sKeyId, keyComponent.GetVehicleIdentifier(), "the key identifier matches the saved key");
		ctx.True(vehicleLock.IsValidKey(key), "the saved key still matches");

		ctx.True(LF_VehicleOwnership.TryUnlock(vehicleId, key), "the owner unlocks before storing");
		ctx.True(LF_VehicleOwnership.Store(vehicleId, uid), "store before the save");
		ctx.True(LF_VehicleOwnership.GetVehicle(vehicleId) == null, "store removes the world vehicle");

		LF_VehicleRecord stored = LF_VehicleOwnership.Export(vehicleId);
		ctx.NotNull(stored, "export returns the stored vehicle");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(stored.m_bStored, "the saved record is stored");
		ctx.False(stored.m_bLocked, "the saved record kept the unlocked flag");
		ctx.EqualStr(LF_VehicleCatalog.S105, stored.m_sPrefab, "the saved record kept the S105 prefab");
		ctx.True(LF_VehicleOwnership.Apply(stored), "apply-save of the stored record");
		ctx.True(LF_VehicleOwnership.IsStored(vehicleId), "the vehicle stays stored after apply-save");

		array<ref LF_VehicleGarageRow> rows = LF_VehicleGarageDialog.StoredRows(uid);
		ctx.True(rows.Count() > 0, "the garage picker lists the stored vehicle");

		ctx.True(LF_VehicleOwnership.Retrieve(vehicleId, uid, "roadmap-garage"), "retrieve after apply-save onto the garage pad");
		ctx.False(LF_VehicleOwnership.IsStored(vehicleId), "retrieve brings it back");
		ctx.EqualStr(LF_VehicleCatalog.S105, LF_VehicleOwnership.GetPrefab(vehicleId), "retrieve keeps the prefab");
		ctx.False(LF_VehicleOwnership.IsLocked(vehicleId), "retrieve keeps the lock flag");
		ctx.EqualStr(saved.m_sKeyId, LF_VehicleOwnership.GetKeyId(vehicleId), "retrieve keeps the key id");

		IEntity restored = LF_VehicleOwnership.GetVehicle(vehicleId);
		ctx.True(restored != null, "retrieve spawns the vehicle");
		if (ctx.FailureCount() > 0)
			return;

		string restoredPrefab = LF_Utils.GetPrefabName(restored);
		ctx.True(restoredPrefab.Contains("S105"), "the retrieved vehicle is the S105");
		LF_VehicleLockComponent restoredLock = LF_Component<LF_VehicleLockComponent>.Find(restored);
		ctx.NotNull(restoredLock, "the retrieved vehicle has the lock");
		if (ctx.FailureCount() > 0)
			return;

		ctx.False(restoredLock.IsVehicleLocked(), "the retrieved lock flag is still unlocked");
		ctx.EqualStr(saved.m_sKeyId, restoredLock.GetVehicleIdentifier(), "the retrieved lock uses the saved key id");
		ctx.True(restoredLock.IsValidKey(LF_VehicleOwnership.GetKey(vehicleId)), "the original key matches after retrieve");

		// Leave the garage pad free for the next retrieve.
		LF_VehicleOwnership.Store(vehicleId, uid);
	}
}
