// LF_GranaryUnload.c: unload a reaper's hopper at the granary's weighbridge (#3294, harvester spike H7).
// A driver whose vehicle tows a reaper and stands inside the unload box gets two prompts: Sell grain pays the
// wholesale curve price (LF_MarketBook) for the hopper's whole lots of kilograms and keeps the rest, Bag grain
// fills grain sacks into the vehicle's storage and keeps what does not fit. Both resolve on the server from the
// user's own seat; the client sends only the intent. The hopper is a kg count (LF_ImplementComponent), a sack is
// a 50 kg LF_QuantityComponent stack, so one kg is one unit of the good the market prices (a lot is 50 units).

enum LF_EGranaryUnload
{
	OK,
	NOT_SERVER,
	NO_USER,
	NO_VEHICLE,		//!< The user is not in a seat of a vehicle that tows a reaper
	NOT_DRIVER,		//!< In the vehicle, but not at the wheel
	NO_REAPER,
	OUTSIDE_BOX,
	EMPTY,			//!< The hopper holds no grain
	BELOW_LOT,		//!< Less than one lot of kg: the granary buys whole lots only (#3622)
	NO_PRICE,		//!< The market does not price grain (off, or no row), or the quote is 0
	UNPAYABLE,		//!< The cash did not reach the seller; the hopper is put back
	NO_ROOM			//!< The vehicle's storage took no sack
}

//------------------------------------------------------------------------------------------------
class LF_GranaryUnload
{
	static const string FEATURE = "Harvest";

	//! The ledger reason of a sale (LF_OnMoneyMoved).
	static const string REASON_SALE = "grain-sale";

	//! The good the market prices and the sack carries: one unit is one kilogram.
	static const ResourceName GRAIN_SACK = "{7B2E5A1C93D40F11}Prefabs/Items/Harvest/LF_GrainSack.et";

	//------------------------------------------------------------------------------------------------
	//! \return true when `at` lies inside the box of `box`: its local frame, half extents `half` (x, y, z).
	static bool InBox(IEntity box, vector half, vector at)
	{
		if (!box)
			return false;

		vector local = box.CoordToLocal(at);
		return Math.AbsFloat(local[0]) <= half[0] && Math.AbsFloat(local[1]) <= half[1] && Math.AbsFloat(local[2]) <= half[2];
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle the user sits in, or null. `atWheel` is set when the seat is the driver's.
	static IEntity VehicleOf(IEntity user, out bool atWheel)
	{
		atWheel = false;
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || !character.IsInVehicle())
			return null;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return null;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return null;

		atWheel = PilotCompartmentSlot.Cast(slot) != null;
		return slot.GetVehicle();
	}

	//------------------------------------------------------------------------------------------------
	//! Checks every rule both actions share. \return OK with `reaper` set, or the refusal (logged).
	static LF_EGranaryUnload Check(IEntity user, IEntity vehicle, IEntity box, vector half, out LF_ImplementComponent reaper)
	{
		reaper = null;
		if (!Replication.IsServer())
			return Refuse(LF_EGranaryUnload.NOT_SERVER, "it resolves on the server only");

		if (!user)
			return Refuse(LF_EGranaryUnload.NO_USER, "no user");

		if (!vehicle)
			return Refuse(LF_EGranaryUnload.NO_VEHICLE, "the user is not in a vehicle");

		if (!InBox(box, half, vehicle.GetOrigin()))
			return Refuse(LF_EGranaryUnload.OUTSIDE_BOX, "the vehicle is outside the unload box: " + LF_ImplementHitch.Describe(vehicle));

		reaper = LF_ImplementComponent.Of(LF_ImplementHitch.FindHitched(vehicle));
		if (!reaper)
			return Refuse(LF_EGranaryUnload.NO_REAPER, "the vehicle tows no reaper: " + LF_ImplementHitch.Describe(vehicle));

		if (reaper.GetHopperKg() <= 0)
			return Refuse(LF_EGranaryUnload.EMPTY, "the hopper is empty");

		return LF_EGranaryUnload.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells the hopper's whole lots at the market's wholesale curve and empties what was sold. The user
	//! is paid through LF_MoneyUtils.AddCash (reason grain-sale, so LF_OnMoneyMoved and the ledger record it);
	//! a part lot stays in the hopper. The hopper is emptied before the pay-out, so a second call finds nothing.
	//! \param[out] paid The cash paid
	//! \param[out] soldKg The kilograms taken from the hopper
	//! \return OK, or why nothing changed.
	static LF_EGranaryUnload Sell(IEntity user, IEntity vehicle, IEntity box, vector half, out int paid, out int soldKg)
	{
		paid = 0;
		soldKg = 0;
		LF_ImplementComponent reaper;
		LF_EGranaryUnload state = Check(user, vehicle, box, half, reaper);
		if (state != LF_EGranaryUnload.OK)
			return state;

		int kg = reaper.GetHopperKg();
		if (!LF_MarketBook.Curved(GRAIN_SACK))
			return Refuse(LF_EGranaryUnload.NO_PRICE, "the market does not price grain");

		int units = LF_MarketBook.SellableUnits(GRAIN_SACK, kg);
		if (units <= 0)
			return Refuse(LF_EGranaryUnload.BELOW_LOT, string.Format("%1 kg is less than one lot of %2", kg, LF_MarketBook.LotOf(GRAIN_SACK)));

		int nowUnix = System.GetUnixTime();
		int total = LF_MarketBook.QuoteAt(GRAIN_SACK, units, nowUnix);
		if (total <= 0)
			return Refuse(LF_EGranaryUnload.NO_PRICE, "the market pays nothing for grain");

		reaper.SetHopperKg(kg - units);
		int added = LF_MoneyUtils.AddCash(user, total, REASON_SALE, "", true);
		if (added != total)
		{
			if (added > 0)
				LF_MoneyUtils.RemoveCash(user, added, REASON_SALE + "-rollback");

			reaper.SetHopperKg(kg);
			return Refuse(LF_EGranaryUnload.UNPAYABLE, string.Format("cash payout failed (%1 of %2)", added, total));
		}

		int oldPrice;
		int newPrice = LF_MarketBook.ApplySaleAt(GRAIN_SACK, units, nowUnix, oldPrice);
		if (newPrice != oldPrice)
			LF_EventBus.Publish(LF_OnMarketPriceMoved.Create(GRAIN_SACK, oldPrice, newPrice));

		paid = total;
		soldKg = units;
		LF_Debug.Info(FEATURE, string.Format("grain sold: %1 kg for $%2 from %3 (%4 kg stay in the hopper)", units, total, LF_ImplementHitch.Describe(vehicle), kg - units));
		LF_Utils.NotifyEntity(user, "#LF-Harvest_UnloadSold", "#LF-Harvest_UnloadSell", 4.0, total.ToString());
		return LF_EGranaryUnload.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fills grain sacks into the vehicle's storage from the hopper and takes out of the hopper exactly what
	//! went in. What does not fit stays in the hopper.
	//! \param[out] baggedKg The kilograms that went into sacks
	//! \return OK (also when only part fit), NO_ROOM when nothing did, or why nothing changed.
	static LF_EGranaryUnload Bag(IEntity user, IEntity vehicle, IEntity box, vector half, out int baggedKg)
	{
		baggedKg = 0;
		LF_ImplementComponent reaper;
		LF_EGranaryUnload state = Check(user, vehicle, box, half, reaper);
		if (state != LF_EGranaryUnload.OK)
			return state;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(vehicle);
		if (!storage)
			return Refuse(LF_EGranaryUnload.NO_ROOM, "the vehicle has no storage: " + LF_ImplementHitch.Describe(vehicle));

		int kg = reaper.GetHopperKg();
		int added = LF_InventoryUtils.AddAmount(storage, GRAIN_SACK, kg);
		if (added <= 0)
		{
			LF_Utils.NotifyEntity(user, "#LF-Harvest_UnloadNoRoom", "#LF-Harvest_UnloadBag", 4.0);
			return Refuse(LF_EGranaryUnload.NO_ROOM, "the vehicle's storage took no sack");
		}

		reaper.SetHopperKg(kg - added);
		baggedKg = added;
		LF_Debug.Info(FEATURE, string.Format("grain bagged: %1 of %2 kg into the storage of %3 (%4 kg stay in the hopper)", added, kg, LF_ImplementHitch.Describe(vehicle), kg - added));
		LF_Utils.NotifyEntity(user, "#LF-Harvest_UnloadBagged", "#LF-Harvest_UnloadBag", 4.0, added.ToString());
		return LF_EGranaryUnload.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast line for a refusal; empty when the refusal toasts nothing (the caller was not a player at all).
	static string RefusalKey(LF_EGranaryUnload result)
	{
		switch (result)
		{
			case LF_EGranaryUnload.NO_VEHICLE: return "#LF-Harvest_UnloadNoVehicle";
			case LF_EGranaryUnload.NOT_DRIVER: return "#LF-Harvest_UnloadNotDriver";
			case LF_EGranaryUnload.NO_REAPER: return "#LF-Harvest_UnloadNoReaper";
			case LF_EGranaryUnload.OUTSIDE_BOX: return "#LF-Harvest_UnloadOutside";
			case LF_EGranaryUnload.EMPTY: return "#LF-Harvest_UnloadEmpty";
			case LF_EGranaryUnload.BELOW_LOT: return "#LF-Harvest_UnloadBelowLot";
			case LF_EGranaryUnload.NO_PRICE: return "#LF-Harvest_UnloadNoPrice";
			case LF_EGranaryUnload.NO_ROOM: return "#LF-Harvest_UnloadNoRoom";
			case LF_EGranaryUnload.UNPAYABLE: return "#LF-Harvest_UnloadFailed";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EGranaryUnload Refuse(LF_EGranaryUnload why, string detail)
	{
		LF_Debug.Warn(FEATURE, string.Format("grain unload refused (%1): %2", typename.EnumToString(LF_EGranaryUnload, why), detail));
		return why;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EGranaryUnloadMode
{
	SELL,
	BAG
}

//! One prompt on the weighbridge entity. The unload box is the entity's local box (half extents below); the
//! vehicle's origin must lie in it. The prompt shows to a driver whose vehicle tows a reaper with grain in the
//! hopper and refuses the rest on the server with a toast.
class LF_GranaryUnloadAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this prompt does", "", ParamEnumArray.FromEnum(LF_EGranaryUnloadMode))]
	protected LF_EGranaryUnloadMode m_eMode;

	[Attribute("6 4 10", UIWidgets.EditBox, "Half extents of the unload box in the weighbridge's local frame (metres)")]
	protected vector m_vHalfExtents;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		bool atWheel;
		IEntity vehicle = LF_GranaryUnload.VehicleOf(user, atWheel);
		if (!vehicle || !atWheel)
			return false;

		if (!LF_GranaryUnload.InBox(GetOwner(), m_vHalfExtents, vehicle.GetOrigin()))
			return false;

		LF_ImplementComponent reaper = LF_ImplementComponent.Of(LF_ImplementHitch.FindHitched(vehicle));
		return reaper && reaper.GetHopperKg() > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eMode == LF_EGranaryUnloadMode.BAG)
			outName = "#LF-Harvest_UnloadBag";
		else
			outName = "#LF-Harvest_UnloadSell";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		if (!LF_RateLimit.AllowCharacter(pUserEntity, LF_ERequestKind.PURCHASE, "granary unload"))
			return;

		bool atWheel;
		IEntity vehicle = LF_GranaryUnload.VehicleOf(pUserEntity, atWheel);
		LF_EGranaryUnload result;
		if (vehicle && !atWheel)
			result = LF_EGranaryUnload.NOT_DRIVER;
		else if (m_eMode == LF_EGranaryUnloadMode.BAG)
			result = Bag(pOwnerEntity, pUserEntity, vehicle);
		else
			result = Sell(pOwnerEntity, pUserEntity, vehicle);

		if (result == LF_EGranaryUnload.OK)
			return;

		string key = LF_GranaryUnload.RefusalKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, GetTitle(), 4.0, LotArg(result));
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EGranaryUnload Sell(IEntity box, IEntity user, IEntity vehicle)
	{
		int paid;
		int soldKg;
		return LF_GranaryUnload.Sell(user, vehicle, box, m_vHalfExtents, paid, soldKg);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EGranaryUnload Bag(IEntity box, IEntity user, IEntity vehicle)
	{
		int baggedKg;
		return LF_GranaryUnload.Bag(user, vehicle, box, m_vHalfExtents, baggedKg);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetTitle()
	{
		if (m_eMode == LF_EGranaryUnloadMode.BAG)
			return "#LF-Harvest_UnloadBag";

		return "#LF-Harvest_UnloadSell";
	}

	//------------------------------------------------------------------------------------------------
	//! %1 of the below-lot line: the units in one lot.
	protected string LotArg(LF_EGranaryUnload result)
	{
		if (result != LF_EGranaryUnload.BELOW_LOT)
			return "";

		return LF_MarketBook.LotOf(LF_GranaryUnload.GRAIN_SACK).ToString();
	}
}
