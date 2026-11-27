//------------------------------------------------------------------------------------------------
//! Fitting a hidden compartment at the chop shop yard (#3776, Beat 2 of #1035). The owner of a car
//! standing in the bench's drop area pays the fitting price from the compartment table
//! (LF_VehicleCompartmentConfig, the pack names its period cars and prices, #3436) and the yard
//! runs LF_VehicleCompartment.Fit on it. The server decides: it refuses a non-owner, a car with no
//! table row, a car that already has a compartment and a purse that is short. The money is taken
//! before the fit and handed back if the compartment does not build, so a car is never fitted for
//! free and a fee is never kept for nothing. The size is written to the saved vehicle state and the
//! empty compartment into the vehiclecache ledger (#3435), so it survives a garage trip and a restart.
//------------------------------------------------------------------------------------------------
enum LF_EFitCheck
{
	NONE,
	NO_VEHICLE,
	NOT_OWNER,
	UNSUPPORTED,
	ALREADY_FITTED,
	SHORT_PURSE,
	TOO_FAR,
	FIT_FAILED
}

//------------------------------------------------------------------------------------------------
//! The pure rules of fitting. No entities, no money, no time.
class LF_ChopFitRules
{
	//------------------------------------------------------------------------------------------------
	//! Whether the worker may have this car fitted.
	//! \param price The table's fitting price, -1 when the car has no row.
	//! \param cash Cash the worker carries.
	static LF_EFitCheck Check(bool hasVehicle, bool isOwner, int price, bool fitted, int cash)
	{
		if (!hasVehicle)
			return LF_EFitCheck.NO_VEHICLE;

		if (!isOwner)
			return LF_EFitCheck.NOT_OWNER;

		if (price < 0)
			return LF_EFitCheck.UNSUPPORTED;

		if (fitted)
			return LF_EFitCheck.ALREADY_FITTED;

		if (cash < price)
			return LF_EFitCheck.SHORT_PURSE;

		return LF_EFitCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Which car in the drop area the fitting takes: the nearest one the worker owns, so a stranger's
	//! car parked beside the bench cannot block it. With no owned car it is the nearest car, so the
	//! worker still reads why it is refused.
	//! \param distances Metres from the bench, one per candidate.
	//! \param ownedBy Whether the worker owns each candidate, same order.
	//! \return Index of the chosen candidate, or -1 when there is none.
	static int PickIndex(notnull array<float> distances, notnull array<bool> ownedBy)
	{
		int bestOwned = -1;
		int bestAny = -1;
		for (int i = 0; i < distances.Count(); i++)
		{
			if (bestAny < 0 || distances[i] < distances[bestAny])
				bestAny = i;

			if (i < ownedBy.Count() && ownedBy[i] && (bestOwned < 0 || distances[i] < distances[bestOwned]))
				bestOwned = i;
		}

		if (bestOwned >= 0)
			return bestOwned;

		return bestAny;
	}

	//------------------------------------------------------------------------------------------------
	//! The fitting price of a vehicle prefab from the compartment table, -1 when the table has no row
	//! for it (or no table loads). A colour child is priced as its model.
	static int PriceFor(ResourceName prefab)
	{
		LF_VehicleCompartmentConfig config = LF_VehicleCompartmentConfig.Get();
		if (!config || prefab.IsEmpty())
			return -1;

		LF_VehicleCompartmentRow row = config.Find(prefab);
		if (!row)
			row = config.Find(LF_VehicleColoursConfig.ResolveBase(prefab));

		if (!row)
			return -1;

		return LF_Utils.MaxInt(row.m_iFitPrice, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the table fits any car at all (false in the bare core: the bench offers no fitting).
	static bool TableHasRows()
	{
		LF_VehicleCompartmentConfig config = LF_VehicleCompartmentConfig.Get();
		return config && config.m_aRows && config.m_aRows.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EFitCheck check)
	{
		switch (check)
		{
			case LF_EFitCheck.NO_VEHICLE: return "#LF-ChopFit_NoVehicle";
			case LF_EFitCheck.NOT_OWNER: return "#LF-ChopFit_NotOwner";
			case LF_EFitCheck.UNSUPPORTED: return "#LF-ChopFit_Unsupported";
			case LF_EFitCheck.ALREADY_FITTED: return "#LF-ChopFit_AlreadyFitted";
			case LF_EFitCheck.SHORT_PURSE: return "#LF-ChopFit_ShortPurse";
			case LF_EFitCheck.TOO_FAR: return "#LF-ChopFit_TooFar";
			case LF_EFitCheck.FIT_FAILED: return "#LF-ChopFit_Failed";
		}

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The fitting service behind the bench action. Server only.
class LF_ChopFit
{
	static const string FEATURE = "ChopShop";
	static const string REASON = "chopshop-fit";

	//------------------------------------------------------------------------------------------------
	//! The car in the bench's drop area to fit: the nearest one this worker owns, else the nearest car.
	static IEntity FindCar(notnull LF_ChopShopComponent shop, IEntity user)
	{
		vector origin = shop.GetOwner().GetOrigin();
		array<IEntity> nearby = LF_Utils.GetNearbyVehicles(origin, shop.GetDropRadius());
		array<IEntity> candidates = {};
		array<float> distances = {};
		array<bool> ownedBy = {};
		foreach (IEntity vehicle : nearby)
		{
			if (!LF_Component<LF_VehicleLockComponent>.Find(vehicle))
				continue;

			candidates.Insert(vehicle);
			distances.Insert(vector.Distance(vehicle.GetOrigin(), origin));
			ownedBy.Insert(LF_VehicleCompartment.IsOwnerOf(user, vehicle));
		}

		int index = LF_ChopFitRules.PickIndex(distances, ownedBy);
		if (index < 0)
			return null;

		return candidates[index];
	}

	//------------------------------------------------------------------------------------------------
	//! The fitting price of this car as toast text, empty when the car is null or has no table row.
	static string PriceText(IEntity vehicle)
	{
		if (!vehicle)
			return string.Empty;

		int price = LF_ChopFitRules.PriceFor(LF_Utils.GetPrefabName(vehicle));
		if (price < 0)
			return string.Empty;

		return LF_MoneyFormat.FormatMoney(price);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the worker asks the yard to fit the car in the drop area. Takes the price in cash on a
	//! success, refuses with a reason and takes nothing otherwise.
	//! \param paid Out: the price taken, 0 on a refusal.
	static LF_EFitCheck Serve(notnull LF_ChopShopComponent shop, IEntity user, out int paid)
	{
		paid = 0;
		if (!Replication.IsServer() || !user)
			return LF_EFitCheck.NONE;

		if (!shop.IsInReach(user))
			return LF_EFitCheck.TOO_FAR;

		IEntity vehicle = FindCar(shop, user);
		LF_VehicleLockComponent vehicleLock;
		string vehicleId;
		bool isOwner;
		bool fitted;
		int price = -1;
		if (vehicle)
		{
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			vehicleId = vehicleLock.GetVehicleIdentifier();
			isOwner = LF_VehicleCompartment.IsOwnerOf(user, vehicle);
			price = LF_ChopFitRules.PriceFor(LF_Utils.GetPrefabName(vehicle));
			fitted = LF_VehicleCompartment.Find(vehicle) != null;
		}

		string uid = LF_Utils.GetPlayerUID(user);
		LF_EFitCheck check = LF_ChopFitRules.Check(vehicle != null, isOwner, price, fitted, LF_MoneyUtils.GetCash(user));
		if (check != LF_EFitCheck.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused the fitting of %2 for %3: owner=%4 price=%5 fitted=%6 (%7)", shop.GetShopId(), vehicleId, uid, isOwner, price, fitted, typename.EnumToString(LF_EFitCheck, check)));
			return check;
		}

		// Pay first; a short take is handed back whole.
		int taken = 0;
		if (price > 0)
			taken = LF_MoneyUtils.RemoveCash(user, price, REASON);

		if (taken < price)
		{
			if (taken > 0)
				LF_MoneyUtils.AddCash(user, taken, REASON + "-rollback");

			LF_Debug.Warn(FEATURE, string.Format("%1: fitting of %2 for %3 refused, only %4 of %5 could be taken", shop.GetShopId(), vehicleId, uid, taken, price));
			return LF_EFitCheck.SHORT_PURSE;
		}

		IEntity panel = LF_VehicleCompartment.Fit(vehicle);
		if (!panel)
		{
			if (taken > 0)
				LF_MoneyUtils.AddCash(user, taken, REASON + "-refund");

			LF_Debug.Error(FEATURE, string.Format("%1: the compartment of %2 did not build; %3 handed back to %4", shop.GetShopId(), vehicleId, taken, uid));
			return LF_EFitCheck.FIT_FAILED;
		}

		Remember(vehicleId, vehicle);
		paid = taken;
		LF_Debug.Info(FEATURE, string.Format("%1: compartment fitted to %2 for %3: paid %4", shop.GetShopId(), vehicleId, uid, taken));
		return LF_EFitCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the fitted size into the car's saved state and the empty compartment into the vehiclecache
	//! ledger, so a garage trip or a restart brings it back (#3435). An unowned car has no record to write.
	protected static void Remember(string vehicleId, IEntity vehicle)
	{
		int size = LF_VehicleCompartmentConfig.SizeFor(LF_Utils.GetPrefabName(vehicle));
		if (size < 1)
			size = LF_VehicleCompartmentConfig.SizeFor(LF_VehicleColoursConfig.ResolveBase(LF_Utils.GetPrefabName(vehicle)));

		LF_VehicleStateRow state = LF_VehicleOwnership.StateOf(vehicleId);
		if (state)
			LF_VehicleOwnership.SetState(vehicleId, state.m_iTowState, state.m_iTowedAt, state.m_iInspectedAt, Math.ClampInt(size, 1, LF_VehicleOwnership.COMPARTMENT_MAX));
		else
			LF_Debug.Warn(FEATURE, string.Format("compartment of %1 fitted but the car has no ownership record, so no size is saved", vehicleId));

		LF_VehicleCache.Capture(vehicleId, vehicle);
	}
}

//------------------------------------------------------------------------------------------------
//! "Fit a hidden compartment" at the chop shop bench. Hidden where the compartment table is empty
//! (the bare core fits no car). The server does the checks and takes the money.
class LF_ChopFitAction : ScriptedUserAction
{
	[Attribute("SOUND_PICK_UP", UIWidgets.EditBox, "Sound event on the bench SCR_SoundDataComponent, played when the fitting lands")]
	protected string m_sSound;

	protected LF_ChopShopComponent m_Shop;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Shop = LF_Component<LF_ChopShopComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Shop || !LF_Systems.IsEnabled("Crime"))
			return false;

		return LF_ChopFitRules.TableHasRows();
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Crime", "LF_ChopFitAction"))
			return;

		if (!m_Shop || !pUserEntity)
		{
			LF_Debug.Error(LF_ChopFit.FEATURE, "fitting action performed without a bench or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		int paid;
		LF_EFitCheck result = LF_ChopFit.Serve(m_Shop, pUserEntity, paid);
		if (result != LF_EFitCheck.NONE)
		{
			string key = LF_ChopFitRules.RefusalKey(result);
			if (!key.IsEmpty())
				LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Chop_Title", 4.0, LF_ChopFit.PriceText(LF_ChopFit.FindCar(m_Shop, pUserEntity)));

			return;
		}

		LF_Utils.NotifyEntity(pUserEntity, "#LF-ChopFit_Done", "#LF-Chop_Title", 4.0, LF_MoneyFormat.FormatMoney(paid));
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (character && !m_sSound.IsEmpty())
			character.LF_BroadcastStationSound("LF_ChopFitAction", m_sSound);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-ChopFit_Action";
		return true;
	}
}
