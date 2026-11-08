// LF_VehicleLotSheet.c — the car lot's price sheet.
//
// Walking up to the lot always opens the sheet: every car with its price, and either a Buy
// button or the reason it is out of reach ("needs $1,000 in the bank"). Before this the
// interaction bought the first car straight away and a player short of money only saw a refusal
// toast. The bank balance is server-only, so the server sends the sheet (LF_ShowVehicleLot) and
// Buy goes back to it (LF_AskBuyVehicle); EL_VehicleLotComponent.Buy keeps every rule.

//------------------------------------------------------------------------------------------------
class LF_VehicleLot
{
	static const string FEATURE = "Vehicle";

	//------------------------------------------------------------------------------------------------
	//! "" when the offer can be bought now, else the line printed instead of Buy. Prices at or
	//! below the cash ceiling are paid in carried cash; above it, from the bank.
	static string Shortfall(int price, int cashCeiling, int cash, int bank)
	{
		if (price <= cashCeiling)
		{
			if (cash >= price)
				return string.Empty;

			return WidgetManager.Translate("#LF-Lot_NeedsCash", EL_MoneyFormat.FormatMoney(price));
		}

		if (bank >= price)
			return string.Empty;

		return WidgetManager.Translate("#LF-Lot_NeedsBank", EL_MoneyFormat.FormatMoney(price));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the reader the lot's offers and what they can pay with.
	static bool Show(IEntity reader, IEntity lotEntity)
	{
		if (!Replication.IsServer() || !reader || !lotEntity)
			return false;

		EL_VehicleLotComponent lot = EL_Component<EL_VehicleLotComponent>.Find(lotEntity);
		SCR_PlayerController controller;
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(reader);
		if (playerId > 0)
			controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		string uid = EL_Utils.GetPlayerUID(reader);
		if (!lot || !controller || uid.IsEmpty())
		{
			EL_Debug.Error(FEATURE, "lot sheet not sent: no lot, no player controller, or no player id");
			return false;
		}

		array<string> nameKeys = {};
		array<int> prices = {};
		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			EL_VehicleOffer offer = lot.GetOfferAt(i);
			if (!offer)
				continue;

			nameKeys.Insert(offer.GetNameKey());
			prices.Insert(offer.GetPrice());
		}

		int bank = 0;
		EL_BankAccount account = EL_ATMManager.GetOrCreate(uid);
		if (account)
			bank = account.GetBalance();

		controller.LF_ShowVehicleLot(nameKeys, prices, lot.GetCashCeiling(), EL_MoneyUtils.GetCash(reader), bank, EL_NetworkUtils.GetRplId(lotEntity));
		EL_Debug.Info(FEATURE, string.Format("lot sheet sent to %1: %2 offer(s), bank %3", uid, nameKeys.Count(), bank));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the reader pressed Buy on row index. The lot decides; the sheet comes back refreshed.
	static void Buy(IEntity buyer, IEntity lotEntity, int index)
	{
		if (!Replication.IsServer() || !buyer || !lotEntity)
			return;

		EL_VehicleLotComponent lot = EL_Component<EL_VehicleLotComponent>.Find(lotEntity);
		EL_VehicleOffer offer;
		if (lot)
			offer = lot.GetOfferAt(index);

		if (!offer)
		{
			EL_Debug.Error(FEATURE, string.Format("buy refused: no offer %1 on this lot", index));
			return;
		}

		if (!LF_Garage.WithinReach(buyer, lotEntity))
		{
			EL_Debug.Warn(FEATURE, "buy refused: the player is not at the lot counter");
			return;
		}

		string uid = EL_Utils.GetPlayerUID(buyer);
		if (uid.IsEmpty())
		{
			EL_Debug.Error(FEATURE, "buy refused: player UID is unavailable");
			return;
		}

		string vehicleId;
		int cashCharged;
		string reasonKey;
		bool bought = lot.Buy(uid, offer.GetPrefab(), buyer, vehicleId, cashCharged, reasonKey);
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(buyer);
		if (!reasonKey.IsEmpty() && playerId > 0)
			EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Vehicle_Buy", reasonKey);

		if (bought)
		{
			EL_Debug.Info(FEATURE, string.Format("bought %1 from the lot sheet for %2", offer.GetNameKey(), uid));
			if (playerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Garage_Title", "#LF-Garage_BuyHint");
		}
		else
		{
			EL_Debug.Log(FEATURE, string.Format("buy from the lot sheet refused for %1: %2", uid, reasonKey));
		}

		Show(buyer, lotEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! Client: the sheet itself (UI kit, paper).
class LF_VehicleLotSheet
{
	static const string LIST = "Cars";
	static const string ROW_PREFIX = "Car";
	static const float LIST_HEIGHT = 330;

	protected static ref LF_UIScreen s_Screen;
	protected static RplId s_LotId;

	//------------------------------------------------------------------------------------------------
	static void Open(array<string> nameKeys, array<int> prices, int cashCeiling, int cash, int bank, RplId lotId)
	{
		Close();
		s_LotId = lotId;

		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "LotTitle", "#LF-Lot_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LotMoney", WidgetManager.Translate("#LF-Lot_Money", EL_MoneyFormat.FormatMoney(bank), EL_MoneyFormat.FormatMoney(cash))));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LotRule", WidgetManager.Translate("#LF-Lot_Rule", EL_MoneyFormat.FormatMoney(cashCeiling))));
		parts.Insert(LF_UIPart.CreateList(LIST));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "LotClose", "#LF-Lot_Leave"));

		s_Screen = LF_UIScreen.Open(parts, LF_UISkin.PriceList());
		if (!s_Screen)
		{
			EL_Debug.Error(LF_VehicleLot.FEATURE, "lot sheet failed to open");
			return;
		}

		s_Screen.SetListHeight(LIST, LIST_HEIGHT);
		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		IEntity lot = EL_NetworkUtils.FindEntityByRplId(lotId);
		if (lot)
			s_Screen.CloseBeyond(4, lot.GetID());

		int count = Math.Min(nameKeys.Count(), prices.Count());
		for (int i = 0; i < count; i++)
		{
			string shortfall = LF_VehicleLot.Shortfall(prices[i], cashCeiling, cash, bank);
			string status = EL_MoneyFormat.FormatMoney(prices[i]);
			string action = "#LF-Lot_Buy";
			if (!shortfall.IsEmpty())
			{
				status = shortfall;
				action = string.Empty;
			}

			s_Screen.AddListRow(LIST, ROW_PREFIX + i.ToString(), WidgetManager.Translate(nameKeys[i]), status, action);
		}

		s_Screen.GetOnButton().Insert(OnButton);
		EL_Debug.Info(LF_VehicleLot.FEATURE, string.Format("lot sheet open: %1 car(s), bank %2", count, bank));
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
		int index = RowIndex(name);
		if (index < 0)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			EL_Debug.Error(LF_VehicleLot.FEATURE, "buy not sent: no player controller");
			return;
		}

		EL_Debug.Info(LF_VehicleLot.FEATURE, string.Format("buy asked for row %1", index));
		controller.LF_AskBuyVehicle(s_LotId, index);
	}
}

//------------------------------------------------------------------------------------------------
//! server -> owner: LF_ShowVehicleLot; owner -> server: LF_AskBuyVehicle.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_ShowVehicleLot(array<string> nameKeys, array<int> prices, int cashCeiling, int cash, int bank, RplId lot)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_ShowVehicleLot(nameKeys, prices, cashCeiling, cash, bank, lot);
		else
			Rpc(RpcDo_LF_ShowVehicleLot, nameKeys, prices, cashCeiling, cash, bank, lot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowVehicleLot(array<string> nameKeys, array<int> prices, int cashCeiling, int cash, int bank, RplId lot)
	{
		if (!GetGame().GetWorkspace())
		{
			EL_Debug.Error(LF_VehicleLot.FEATURE, "lot sheet not opened: no workspace on this machine");
			return;
		}

		LF_VehicleLotSheet.Open(nameKeys, prices, cashCeiling, cash, bank, lot);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskBuyVehicle(RplId lot, int index)
	{
		if (Replication.IsServer())
			RpcAsk_LF_BuyVehicle(lot, index);
		else
			Rpc(RpcAsk_LF_BuyVehicle, lot, index);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BuyVehicle(RplId lot, int index)
	{
		LF_VehicleLot.Buy(GetControlledEntity(), EL_NetworkUtils.FindEntityByRplId(lot), index);
	}
}
