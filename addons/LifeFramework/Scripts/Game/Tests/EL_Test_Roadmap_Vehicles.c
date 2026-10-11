// red-proof: expected Y for LetterAt(25) and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST]   - the alphabet includes Z (expected 'Y', got 'Z')`
// with SUMMARY tier=fast passed=55 failed=1 total=56.
// Restored the expected value to Z.

// tier: LOGIC
class EL_Test_Roadmap_Vehicles : EL_Test
{
	override string GetName()
	{
		return "roadmap/vehicles";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.EqualStr("A", EL_LicensePlateGeneratorGeneric.LetterAt(0), "the alphabet starts at A");
		ctx.EqualStr("Z", EL_LicensePlateGeneratorGeneric.LetterAt(25), "the alphabet includes Z");
		ctx.EqualStr("", EL_LicensePlateGeneratorGeneric.LetterAt(-1), "a letter before A is refused");
		ctx.EqualStr("", EL_LicensePlateGeneratorGeneric.LetterAt(26), "a letter after Z is refused");
		ctx.EqualStr("ZZ ZZ 9999", EL_LicensePlateGeneratorGeneric.ComposePlate(25, 25, 25, 25, 9999), "the top number is 9999");
		ctx.EqualStr("", EL_LicensePlateGeneratorGeneric.ComposePlate(25, 25, 25, 25, 99), "numbers below 100 are refused");
		ctx.EqualStr("", EL_LicensePlateGeneratorGeneric.ComposePlate(25, 25, 25, 25, 10000), "numbers above 9999 are refused");
		ctx.Equal(26, EL_LicensePlateGeneratorGeneric.LetterRollCount(), "the letter roll includes index 25");
		ctx.Equal(10000, EL_LicensePlateGeneratorGeneric.NumberRollEnd(), "the number roll includes 9999");

		EL_LicensePlateRegistry.Remember("AA BB 1000", "veh-a");
		ctx.True(EL_LicensePlateRegistry.IsLive("AA BB 1000"), "a remembered plate is live");
		ctx.EqualStr("", EL_LicensePlateRegistry.TakeUnique("AA BB 1000", "veh-b"), "a second issue does not reuse a live plate");
		ctx.EqualStr("ZZ ZZ 9999", EL_LicensePlateRegistry.TakeUnique("ZZ ZZ 9999", "veh-b"), "a free plate is issued");
		ctx.True(EL_LicensePlateRegistry.IsLive("ZZ ZZ 9999"), "the free plate is now live");

		int cashDebit = 0;
		int bankDebit = 0;
		ctx.True(EL_VehicleOwnership.QuotePurchase(100, 500, 100, 0, cashDebit, bankDebit), "cash covers a small price");
		ctx.Equal(100, cashDebit, "a small purchase debits cash");
		ctx.Equal(0, bankDebit, "a small purchase does not touch the bank");

		ctx.False(EL_VehicleOwnership.QuotePurchase(100, 500, 99, 500, cashDebit, bankDebit), "short cash refuses a small purchase");
		ctx.Equal(0, cashDebit, "a refused small purchase debits no cash");
		ctx.Equal(0, bankDebit, "a refused small purchase does not fall through to the bank");

		ctx.True(EL_VehicleOwnership.QuotePurchase(1000, 500, 5000, 1000, cashDebit, bankDebit), "the bank covers a large price");
		ctx.Equal(0, cashDebit, "a large purchase does not take cash");
		ctx.Equal(1000, bankDebit, "a large purchase debits the bank");

		ctx.False(EL_VehicleOwnership.QuotePurchase(1000, 500, 5000, 999, cashDebit, bankDebit), "short bank refuses a large purchase");
		ctx.Equal(0, cashDebit, "a refused large purchase still does not take cash");
		ctx.Equal(0, bankDebit, "a refused large purchase debits no bank");

		ctx.False(EL_VehicleOwnership.QuotePurchase(0, 500, 100, 100, cashDebit, bankDebit), "an unconfigured price refuses");

		string id = "roadmap-vehicles-logic";
		string owner = "roadmap-vehicles-owner";
		ctx.True(EL_VehicleOwnership.Register(id, owner, "key-logic", EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED, "roadmap-lot", "AA BB 1100", "#EL-Vehicle_S105", true), "a vehicle record registers");
		ctx.False(EL_VehicleOwnership.CanIssueExtraKey(id, "roadmap-vehicles-other"), "a stranger cannot cut a key");
		ctx.True(EL_VehicleOwnership.CanIssueExtraKey(id, owner), "the owner can cut a key");

		EL_VehicleRecord saved = EL_VehicleOwnership.Export(id);
		ctx.NotNull(saved, "export returns the record");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(owner, saved.m_sOwnerUid, "exported owner matches");
		ctx.EqualStr("key-logic", saved.m_sKeyId, "exported key matches");
		EL_VehicleOwnership.NoteLockState(id, false);
		ctx.False(EL_VehicleOwnership.IsLocked(id), "the live lock can change");
		ctx.True(EL_VehicleOwnership.Apply(saved), "apply restores the saved record");
		ctx.True(EL_VehicleOwnership.IsLocked(id), "apply restores the lock flag");
		ctx.EqualStr(owner, EL_VehicleOwnership.GetOwner(id), "apply restores the owner");
		ctx.EqualStr("key-logic", EL_VehicleOwnership.GetKeyId(id), "apply restores the key id");
		ctx.True(EL_VehicleOwnership.Apply(saved), "a second apply converges");
		ctx.EqualStr("key-logic", EL_VehicleOwnership.GetKeyId(id), "a second apply does not change the key");

		ctx.False(EL_VehicleOwnership.Store(id, "roadmap-vehicles-other"), "a stranger cannot store the vehicle");
		ctx.True(EL_VehicleOwnership.Store(id, owner), "the owner can store the vehicle");
		ctx.True(EL_VehicleOwnership.IsStored(id), "the record is stored");
		EL_VehicleRecord stored = EL_VehicleOwnership.Export(id);
		ctx.NotNull(stored, "export returns the stored record");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(stored.m_bStored, "the saved record is stored");
		ctx.True(stored.m_bLocked, "the saved record keeps the lock flag");
		ctx.EqualStr(EL_VehicleCatalog.S105, stored.m_sPrefab, "the saved record keeps the vanilla prefab");
		ctx.True(EL_VehicleOwnership.RestoreRecord(id, owner), "retrieve clears the stored flag");
		ctx.False(EL_VehicleOwnership.IsStored(id), "the record is back in the world");
		ctx.True(EL_VehicleOwnership.Apply(stored), "apply-save puts it back in the garage");
		ctx.True(EL_VehicleOwnership.IsStored(id), "the applied record is stored");
		ctx.True(EL_VehicleOwnership.RestoreRecord(id, owner), "retrieve after apply-save");
		ctx.EqualStr(EL_VehicleCatalog.S105, EL_VehicleOwnership.GetPrefab(id), "retrieve keeps the prefab");
		ctx.True(EL_VehicleOwnership.IsLocked(id), "retrieve keeps the lock flag");
		ctx.EqualStr(owner, EL_VehicleOwnership.GetOwner(id), "retrieve keeps the owner");
		ctx.EqualStr("key-logic", EL_VehicleOwnership.GetKeyId(id), "retrieve keeps the key id");

		EL_VehicleRecord unknown = EL_VehicleRecord.Create("roadmap-vehicles-missing", owner, "key-logic", EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED, "roadmap-lot", "", "#EL-Vehicle_S105", true, false);
		ctx.False(EL_VehicleOwnership.Apply(unknown), "a record for an unknown vehicle is rejected");
	}
}

// tier: WORLD
class EL_Test_Roadmap_VehiclesWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-vehicles";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource s105 = Resource.Load(EL_VehicleCatalog.S105);
		Resource s105Locked = Resource.Load(EL_VehicleCatalog.S105_LOCKED);
		Resource uaz = Resource.Load(EL_VehicleCatalog.UAZ469);
		Resource van = Resource.Load(EL_VehicleCatalog.S1203);
		Resource uaz452 = Resource.Load(EL_VehicleCatalog.UAZ452);
		Resource truck = Resource.Load(EL_VehicleCatalog.M923);
		Resource garage = Resource.Load(EL_VehicleGarageComponent.GARAGE_SHELL);
		Resource keyPrefab = Resource.Load(EL_VehicleCatalog.KEY);
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

		string displayPrefab = EL_Utils.GetPrefabName(display);
		ctx.True(displayPrefab.Contains("S105"), "the showroom body is an S105");
		ctx.NotNull(EL_Component<EL_VehicleLockComponent>.Find(display), "the showroom S105 carries EL_VehicleLockComponent");

		EL_VehicleLotComponent lot = EL_Component<EL_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot carries EL_VehicleLotComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(5, lot.GetOfferCount(), "the lot lists the catalog");
		ctx.Equal(500, lot.GetCashCeiling(), "the cash ceiling is configured");
		ctx.Equal(1000, lot.GetPrice(EL_VehicleCatalog.S105), "the S105 price is above the cash ceiling");
		ctx.True(EL_VehicleGarageComponent.IsPositionNearAnyGarage(lotEntity.GetOrigin()), "the lot is a garage");
		ctx.True(EL_VehicleSpawnPointComponent.CountForShop("roadmap-lot") > 0, "the lot has a spawn pad");

		array<ref EL_VehicleGarageRow> rows = EL_VehicleGarageDialog.CatalogRows(lot);
		ctx.Equal(5, rows.Count(), "catalog rows");
		if (rows.Count() > 0)
		{
			ctx.EqualStr("#EL-Vehicle_S105", rows.Get(0).m_sLabelKey, "the first catalog row is the S105");
			ctx.Equal(1000, rows.Get(0).m_iPrice, "the first catalog row shows the configured price");
		}

		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(lotEntity);
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
			if (EL_VehicleBuyAction.Cast(action))
				buyWired = true;
			if (EL_VehicleExtraKeyAction.Cast(action))
				keyWired = true;
			if (EL_VehicleStoreAction.Cast(action))
				storeWired = true;
			if (EL_VehicleRetrieveAction.Cast(action))
				retrieveWired = true;
		}

		ctx.True(buyWired, "buy action is on the lot");
		ctx.True(keyWired, "extra key action is on the lot");
		ctx.True(storeWired, "store action is on the lot");
		ctx.True(retrieveWired, "retrieve action is on the lot");

		string uid = "roadmap-vehicles-e2e";
		EL_BankAccount account = EL_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "the buyer has a bank account");
		if (ctx.FailureCount() > 0)
			return;

		int start = account.GetBalance();
		int price = lot.GetPrice(EL_VehicleCatalog.S105);
		account.Deposit(price);

		string vehicleId;
		int cashCharged;
		string reasonKey;
		ctx.True(lot.Buy(uid, EL_VehicleCatalog.S105, null, vehicleId, cashCharged, reasonKey), "buy spawns the S105");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(0, cashCharged, "the configured S105 price is not taken as cash");
		ctx.Equal(start, account.GetBalance(), "purchase debits the configured price");
		ctx.EqualStr(uid, EL_VehicleOwnership.GetOwner(vehicleId), "the buyer is the owner");
		ctx.EqualStr(EL_VehicleCatalog.S105, EL_VehicleOwnership.GetPrefab(vehicleId), "the record stores the vanilla S105");
		ctx.False(EL_VehicleOwnership.GetPlate(vehicleId).IsEmpty(), "purchase issued a plate");

		IEntity vehicle = EL_VehicleOwnership.GetVehicle(vehicleId);
		ctx.True(vehicle != null, "the purchased vehicle is in the world");
		if (ctx.FailureCount() > 0)
			return;

		string spawnedPrefab = EL_Utils.GetPrefabName(vehicle);
		ctx.True(spawnedPrefab.Contains("S105"), "buy spawns the S105 prefab");

		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
		ctx.NotNull(vehicleLock, "the purchased S105 carries EL_VehicleLockComponent");
		IEntity key = EL_VehicleOwnership.GetKey(vehicleId);
		ctx.True(key != null, "purchase gives a key");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(vehicleLock.IsVehicleLocked(), "the purchased vehicle starts locked");
		ctx.EqualStr(vehicleLock.GetVehicleIdentifier(), EL_VehicleOwnership.GetKeyId(vehicleId), "the key id is the lock identifier");

		IEntity wrong = EL_Utils.SpawnEntityPrefab(EL_VehicleCatalog.KEY, "560 0 124");
		ctx.True(wrong != null, "a wrong key can be spawned");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleKeyComponent wrongKey = EL_Component<EL_VehicleKeyComponent>.Find(wrong);
		ctx.NotNull(wrongKey, "the wrong key carries EL_VehicleKeyComponent");
		if (ctx.FailureCount() > 0)
			return;

		wrongKey.BindToVehicleIdentifier("roadmap-vehicles-wrong");
		ctx.False(EL_VehicleOwnership.TryUnlock(vehicleId, wrong), "the wrong key does not unlock");
		ctx.True(vehicleLock.IsVehicleLocked(), "the vehicle stays locked after the wrong key");
		ctx.True(EL_VehicleOwnership.TryUnlock(vehicleId, key), "the matching key unlocks");
		ctx.False(vehicleLock.IsVehicleLocked(), "the matching key cleared the lock");
	}
}

// tier: PERSISTENCE
class EL_Test_Roadmap_VehiclesPersistence : EL_Test
{
	override string GetName()
	{
		return "roadmap/vehicles-save";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName("Roadmap_Vehicle_Lot");
		ctx.True(lotEntity != null, "Roadmap_Vehicle_Lot is placed for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleLotComponent lot = EL_Component<EL_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot is present for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		string uid = "roadmap-vehicles-save";
		EL_BankAccount account = EL_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "the save buyer has a bank account");
		if (ctx.FailureCount() > 0)
			return;

		int price = lot.GetPrice(EL_VehicleCatalog.S105);
		account.Deposit(price);

		string vehicleId;
		int cashCharged;
		string reasonKey;
		ctx.True(lot.Buy(uid, EL_VehicleCatalog.S105, null, vehicleId, cashCharged, reasonKey), "buy before the save");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleRecord saved = EL_VehicleOwnership.Export(vehicleId);
		ctx.NotNull(saved, "export returns the owned vehicle");
		if (ctx.FailureCount() > 0)
			return;

		IEntity vehicle = EL_VehicleOwnership.GetVehicle(vehicleId);
		IEntity key = EL_VehicleOwnership.GetKey(vehicleId);
		ctx.True(vehicle != null, "the vehicle is live before the save");
		ctx.True(key != null, "the key is live before the save");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
		EL_VehicleKeyComponent keyComponent = EL_Component<EL_VehicleKeyComponent>.Find(key);
		ctx.NotNull(vehicleLock, "the live vehicle has a lock");
		ctx.NotNull(keyComponent, "the live key has a key component");
		if (ctx.FailureCount() > 0)
			return;

		vehicleLock.SetVehicleIdentifier("mutated");
		keyComponent.BindToVehicleIdentifier("mutated");
		ctx.True(EL_VehicleOwnership.Apply(saved), "apply-save restores the record");
		ctx.EqualStr(uid, EL_VehicleOwnership.GetOwner(vehicleId), "owner id matches after apply-save");
		ctx.EqualStr(saved.m_sKeyId, EL_VehicleOwnership.GetKeyId(vehicleId), "key id matches after apply-save");
		ctx.EqualStr(saved.m_sKeyId, vehicleLock.GetVehicleIdentifier(), "the lock identifier matches the saved key");
		ctx.EqualStr(saved.m_sKeyId, keyComponent.GetVehicleIdentifier(), "the key identifier matches the saved key");
		ctx.True(vehicleLock.IsValidKey(key), "the saved key still matches");

		ctx.True(EL_VehicleOwnership.TryUnlock(vehicleId, key), "the owner unlocks before storing");
		ctx.True(EL_VehicleOwnership.Store(vehicleId, uid), "store before the save");
		ctx.True(EL_VehicleOwnership.GetVehicle(vehicleId) == null, "store removes the world vehicle");

		EL_VehicleRecord stored = EL_VehicleOwnership.Export(vehicleId);
		ctx.NotNull(stored, "export returns the stored vehicle");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(stored.m_bStored, "the saved record is stored");
		ctx.False(stored.m_bLocked, "the saved record kept the unlocked flag");
		ctx.EqualStr(EL_VehicleCatalog.S105, stored.m_sPrefab, "the saved record kept the S105 prefab");
		ctx.True(EL_VehicleOwnership.Apply(stored), "apply-save of the stored record");
		ctx.True(EL_VehicleOwnership.IsStored(vehicleId), "the vehicle stays stored after apply-save");

		array<ref EL_VehicleGarageRow> rows = EL_VehicleGarageDialog.StoredRows(uid);
		ctx.True(rows.Count() > 0, "the garage picker lists the stored vehicle");

		ctx.True(EL_VehicleOwnership.Retrieve(vehicleId, uid), "retrieve after apply-save");
		ctx.False(EL_VehicleOwnership.IsStored(vehicleId), "retrieve brings it back");
		ctx.EqualStr(EL_VehicleCatalog.S105, EL_VehicleOwnership.GetPrefab(vehicleId), "retrieve keeps the prefab");
		ctx.False(EL_VehicleOwnership.IsLocked(vehicleId), "retrieve keeps the lock flag");
		ctx.EqualStr(saved.m_sKeyId, EL_VehicleOwnership.GetKeyId(vehicleId), "retrieve keeps the key id");

		IEntity restored = EL_VehicleOwnership.GetVehicle(vehicleId);
		ctx.True(restored != null, "retrieve spawns the vehicle");
		if (ctx.FailureCount() > 0)
			return;

		string restoredPrefab = EL_Utils.GetPrefabName(restored);
		ctx.True(restoredPrefab.Contains("S105"), "the retrieved vehicle is the S105");
		EL_VehicleLockComponent restoredLock = EL_Component<EL_VehicleLockComponent>.Find(restored);
		ctx.NotNull(restoredLock, "the retrieved vehicle has the lock");
		if (ctx.FailureCount() > 0)
			return;

		ctx.False(restoredLock.IsVehicleLocked(), "the retrieved lock flag is still unlocked");
		ctx.EqualStr(saved.m_sKeyId, restoredLock.GetVehicleIdentifier(), "the retrieved lock uses the saved key id");
		ctx.True(restoredLock.IsValidKey(EL_VehicleOwnership.GetKey(vehicleId)), "the original key matches after retrieve");
	}
}
