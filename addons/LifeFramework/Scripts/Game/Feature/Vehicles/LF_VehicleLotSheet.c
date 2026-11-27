// LF_VehicleLotSheet.c — the car lot's price sheet.
//
// Walking up to the lot always opens the sheet: every car with its price, and either a Buy
// button or the reason it is out of reach ("needs $1,000 in the bank"). Before this the
// interaction bought the first car straight away and a player short of money only saw a refusal
// toast. The bank balance is server-only, so the server sends the sheet (LF_ShowVehicleLot) and
// Buy goes back to it (LF_AskBuyVehicle); LF_VehicleLotComponent.Buy keeps every rule.

//------------------------------------------------------------------------------------------------
class LF_VehicleLot
{
	static const string FEATURE = "Vehicle";
	//! The sheet title of a lot with no title row of its own.
	static const string DEFAULT_TITLE = "#LF-Lot_Title";
	//! A lot's own title row is this prefix plus its lot id (dashes as underscores), no "#".
	static const string TITLE_ID_PREFIX = "LF-Lot_Title_";

	//------------------------------------------------------------------------------------------------
	//! "" when the offer can be bought now, else the line printed instead of Buy. Prices at or
	//! below the cash ceiling are paid in carried cash; above it, from the bank.
	static string Shortfall(int price, int cashCeiling, int cash, int bank)
	{
		if (price <= cashCeiling)
		{
			if (cash >= price)
				return string.Empty;

			return WidgetManager.Translate("#LF-Lot_NeedsCash", LF_MoneyFormat.FormatMoney(price));
		}

		if (bank >= price)
			return string.Empty;

		return WidgetManager.Translate("#LF-Lot_NeedsBank", LF_MoneyFormat.FormatMoney(price));
	}

	//------------------------------------------------------------------------------------------------
	//! The row's name text. A truck row (the capacity class Buy checks for the truck license,
	//! LF_VehicleLotComponent.TruckRefusal) says so beside the name, so the player learns it before
	//! saving up; a car row, or a row with no class, keeps the plain name.
	static string RowLabel(string nameText, string capacityClass)
	{
		if (capacityClass != LF_VehicleLotComponent.CLASS_TRUCK)
			return nameText;

		return WidgetManager.Translate("#LF-Lot_RowNeedsTruck", nameText);
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet title's own key for a lot id: TITLE_ID_PREFIX plus the id with its dashes as
	//! underscores. The row lives in a string table, so a pack titles its lot by adding a row;
	//! no row (or no lot id) is the default title.
	static string TitleKeyFor(string lotId)
	{
		if (lotId.IsEmpty())
			return DEFAULT_TITLE;

		lotId.Replace("-", "_");
		string key = "#" + TITLE_ID_PREFIX + lotId;
		if (LF_DialogueAudit.IsUntranslated(key))
			return DEFAULT_TITLE;

		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the reader the lot's offers and what they can pay with.
	static bool Show(IEntity reader, IEntity lotEntity)
	{
		if (!Replication.IsServer() || !reader || !lotEntity)
			return false;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		SCR_PlayerController controller;
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(reader);
		if (playerId > 0)
			controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		string uid = LF_Utils.GetPlayerUID(reader);
		if (!lot || !controller || uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "lot sheet not sent: no lot, no player controller, or no player id");
			return false;
		}

		array<string> nameKeys = {};
		array<int> prices = {};
		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			LF_VehicleOffer offer = lot.GetOfferAt(i);
			if (!offer)
				continue;

			nameKeys.Insert(offer.GetNameKey());
			prices.Insert(offer.GetPrice());
		}

		int bank = 0;
		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		if (account)
			bank = account.GetBalance();

		// The change-locks row is offered only to an owner and names the car it re-keys (#2411);
		// a price of 0 hides it.
		int locksPrice = 0;
		string locksPlate;
		string locksTarget = LF_VehicleLotComponent.LocksTarget(uid);
		if (!locksTarget.IsEmpty())
		{
			locksPrice = lot.GetExtraKeyPrice();
			locksPlate = LF_VehicleOwnership.GetPlate(locksTarget);
		}

		controller.LF_ShowVehicleLot(nameKeys, prices, lot.GetCashCeiling(), LF_MoneyUtils.GetCash(reader), bank, locksPrice, locksPlate, LF_NetworkUtils.GetRplId(lotEntity));
		LF_Debug.Info(FEATURE, string.Format("lot sheet sent to %1: %2 offer(s), bank %3", uid, nameKeys.Count(), bank));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the reader pressed Buy on row index. The lot decides; the sheet comes back refreshed.
	//! dlcOwned is the buyer's client answer for a DLC vehicle (LF_VehicleDlcGate, #3304).
	static void Buy(IEntity buyer, IEntity lotEntity, int index, int colour, bool dlcOwned = true)
	{
		if (!Replication.IsServer() || !buyer || !lotEntity)
			return;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		LF_VehicleOffer offer;
		if (lot)
			offer = lot.GetOfferAt(index);

		if (!offer)
		{
			LF_Debug.Error(FEATURE, string.Format("buy refused: no offer %1 on this lot", index));
			return;
		}

		if (!LF_Garage.WithinReach(buyer, lotEntity))
		{
			LF_Debug.Warn(FEATURE, "buy refused: the player is not at the lot counter");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(buyer);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "buy refused: player UID is unavailable");
			return;
		}

		string vehicleId;
		int cashCharged;
		string reasonKey;
		// A DLC vehicle the buyer's client cannot use is refused before the lot charges (#3304).
		bool bought = false;
		if (LF_VehicleDlcGate.Allows(offer.GetPrefab(), dlcOwned, reasonKey))
			bought = lot.BuyColour(uid, offer.GetPrefab(), colour, buyer, vehicleId, cashCharged, reasonKey);
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(buyer);
		if (!reasonKey.IsEmpty() && playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Vehicle_Buy", reasonKey, 5.0, LF_ENotificationType.INFO, LF_VehicleLotComponent.ReasonArg(reasonKey));

		if (bought)
		{
			LF_Debug.Info(FEATURE, string.Format("bought %1 from the lot sheet for %2", offer.GetNameKey(), uid));
			if (playerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Garage_Title", "#LF-Garage_BuyHint");
		}
		else
		{
			LF_Debug.Log(FEATURE, string.Format("buy from the lot sheet refused for %1: %2", uid, reasonKey));
		}

		Show(buyer, lotEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the reader pressed Change locks (#2411). Same counter reach as Buy; the lot decides.
	static void ChangeLocks(IEntity owner, IEntity lotEntity)
	{
		if (!Replication.IsServer() || !owner || !lotEntity)
			return;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		if (!lot)
		{
			LF_Debug.Error(FEATURE, "change locks refused: the target is not a lot");
			return;
		}

		if (!LF_Garage.WithinReach(owner, lotEntity))
		{
			LF_Debug.Warn(FEATURE, "change locks refused: the player is not at the lot counter");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(owner);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "change locks refused: player UID is unavailable");
			return;
		}

		int cashCharged;
		string reasonKey;
		bool changed = lot.ChangeLocks(uid, owner, cashCharged, reasonKey);
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
		if (playerId > 0 && !reasonKey.IsEmpty())
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Lot_ChangeLocks", reasonKey);

		LF_Debug.Info(FEATURE, string.Format("change locks for %1: changed=%2 (%3)", uid, changed, reasonKey));

		Show(owner, lotEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! Client: the sheet itself (UI kit, paper).
class LF_VehicleLotSheet
{
	static const string LIST = "Cars";
	static const string ROW_PREFIX = "Car";
	//! The change-locks row (#2411). Not ROW_PREFIX, so RowIndex never reads it as an offer.
	static const string LOCKS_ROW = "ChangeLocks";
	static const float LIST_HEIGHT = 312;	// lf-fixed: eight compact rows of 39 (UI sweep 2026-11-27)

	protected static ref LF_UIScreen s_Screen;
	protected static RplId s_LotId;

	//------------------------------------------------------------------------------------------------
	static void Open(array<string> nameKeys, array<int> prices, int cashCeiling, int cash, int bank, int locksPrice, string locksPlate, RplId lotId)
	{
		Close();
		s_LotId = lotId;

		// The lot's entity is layer data, present on the client; its id picks the title.
		IEntity lot = LF_NetworkUtils.FindEntityByRplId(lotId);
		string lotKey;
		LF_VehicleLotComponent lotComponent = LF_Component<LF_VehicleLotComponent>.Find(lot);
		if (lotComponent)
			lotKey = lotComponent.GetLotId();

		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "LotTitle", LF_VehicleLot.TitleKeyFor(lotKey)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LotMoney", WidgetManager.Translate("#LF-Lot_Money", LF_MoneyFormat.FormatMoney(bank), LF_MoneyFormat.FormatMoney(cash))));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LotRule", WidgetManager.Translate("#LF-Lot_Rule", LF_MoneyFormat.FormatMoney(cashCeiling))));
		parts.Insert(LF_UIPart.CreateList(LIST).Compact());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "LotClose", "#LF-Lot_Leave"));

		s_Screen = LF_UIScreen.Open(parts, LF_UISkin.PriceList());
		if (!s_Screen)
		{
			LF_Debug.Error(LF_VehicleLot.FEATURE, "lot sheet failed to open");
			return;
		}

		s_Screen.SetListHeight(LIST, LIST_HEIGHT);
		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (lot)
			s_Screen.CloseBeyond(4, lot.GetID());

		int count = Math.Min(nameKeys.Count(), prices.Count());
		for (int i = 0; i < count; i++)
		{
			string shortfall = LF_VehicleLot.Shortfall(prices[i], cashCeiling, cash, bank);
			string status = LF_MoneyFormat.FormatMoney(prices[i]);
			string action = "#LF-Lot_Buy";
			if (!shortfall.IsEmpty())
			{
				status = shortfall;
				action = string.Empty;
			}

			// The offers are layer data, present on the client; the row's class comes from the same
			// price config the server's Buy reads.
			string capacityClass;
			if (lotComponent)
			{
				LF_VehicleOffer offer = lotComponent.GetOfferAt(i);
				if (offer)
					capacityClass = offer.CapacityClassIn(LF_VehiclePricesConfig.Get());
			}

			s_Screen.AddListRow(LIST, ROW_PREFIX + i.ToString(), LF_VehicleLot.RowLabel(WidgetManager.Translate(nameKeys[i]), capacityClass), status, action);
		}

		if (locksPrice > 0)
			s_Screen.AddListRow(LIST, LOCKS_ROW, WidgetManager.Translate("#LF-Lot_ChangeLocksRow", locksPlate), LF_MoneyFormat.FormatMoney(locksPrice), "#LF-Lot_ChangeLocksButton");

		s_Screen.GetOnButton().Insert(OnButton);
		LF_Debug.Info(LF_VehicleLot.FEATURE, string.Format("lot sheet open: %1 car(s), bank %2", count, bank));
	}

	//------------------------------------------------------------------------------------------------
	static void Close()
	{
		if (!s_Screen)
			return;

		s_Screen.GetOnButton().Remove(OnButton);
		s_Screen.Close();
		s_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Row index from a row name ("Car2" -> 2), or -1.
	static int RowIndex(string rowName)
	{
		int prefix = ROW_PREFIX.Length();
		if (rowName.Length() <= prefix || rowName.Substring(0, prefix) != ROW_PREFIX)
			return -1;

		string digits = rowName.Substring(prefix, rowName.Length() - prefix);
		if (digits.ToInt().ToString() != digits)
			return -1;

		return digits.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnButton(string name)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (name == LOCKS_ROW)
		{
			if (!controller)
			{
				LF_Debug.Error(LF_VehicleLot.FEATURE, "change locks not sent: no player controller");
				return;
			}

			LF_Debug.Info(LF_VehicleLot.FEATURE, "change locks asked");
			controller.LF_AskChangeVehicleLocks(s_LotId);
			return;
		}

		int index = RowIndex(name);
		if (index < 0)
			return;

		if (!controller)
		{
			LF_Debug.Error(LF_VehicleLot.FEATURE, "buy not sent: no player controller");
			return;
		}

		LF_Debug.Info(LF_VehicleLot.FEATURE, string.Format("buy asked for row %1", index));
		controller.LF_AskBuyVehicle(s_LotId, index, 0);
	}
}

//------------------------------------------------------------------------------------------------
//! server -> owner: LF_ShowVehicleLot; owner -> server: LF_AskBuyVehicle.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_ShowVehicleLot(array<string> nameKeys, array<int> prices, int cashCeiling, int cash, int bank, int locksPrice, string locksPlate, RplId lot)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_ShowVehicleLot(nameKeys, prices, cashCeiling, cash, bank, locksPrice, locksPlate, lot);
		else
			Rpc(RpcDo_LF_ShowVehicleLot, nameKeys, prices, cashCeiling, cash, bank, locksPrice, locksPlate, lot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowVehicleLot(array<string> nameKeys, array<int> prices, int cashCeiling, int cash, int bank, int locksPrice, string locksPlate, RplId lot)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_VehicleLot.FEATURE, "lot sheet not opened: no workspace on this machine");
			return;
		}

		LF_VehicleLotSheet.Open(nameKeys, prices, cashCeiling, cash, bank, locksPrice, locksPlate, lot);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the buyer's machine, so the DLC answer is this player's licence (DlcManager is local
	//! only, LF_VehicleDlcGate). The lot's offers are layer data, present on the client too.
	void LF_AskBuyVehicle(RplId lot, int index, int colour)
	{
		bool dlcOwned = true;
		LF_VehicleLotComponent lotComponent = LF_Component<LF_VehicleLotComponent>.Find(LF_NetworkUtils.FindEntityByRplId(lot));
		if (lotComponent)
		{
			LF_VehicleOffer offer = lotComponent.GetOfferAt(index);
			if (offer)
				dlcOwned = LF_VehicleDlcGate.LocalOwns(offer.GetPrefab());
		}

		if (Replication.IsServer())
			RpcAsk_LF_BuyVehicle(lot, index, colour, dlcOwned);
		else
			Rpc(RpcAsk_LF_BuyVehicle, lot, index, colour, dlcOwned);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BuyVehicle(RplId lot, int index, int colour, bool dlcOwned)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.PURCHASE, "vehicle purchase"))
			return;

		LF_VehicleLot.Buy(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(lot), index, colour, dlcOwned);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskChangeVehicleLocks(RplId lot)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ChangeVehicleLocks(lot);
		else
			Rpc(RpcAsk_LF_ChangeVehicleLocks, lot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ChangeVehicleLocks(RplId lot)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.PURCHASE, "change vehicle locks"))
			return;

		LF_VehicleLot.ChangeLocks(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(lot));
	}
}
