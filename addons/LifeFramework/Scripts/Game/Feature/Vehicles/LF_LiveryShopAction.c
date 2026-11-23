// LF_LiveryShopAction.c - the respray bay's action rows (WO-L9b, #4571; server half: LF_LiveryShop.c).
//
// The bay prop carries LF_LiveryBay.MAX_ROWS rows, slot 0 up. Row N shows while the reader's own car
// stands in the bay and the car has an Nth paint job the reader may buy; its name is the paint and
// its price ("Paint: Police livery ($250)"). There is no window: the rows are in the world beside the
// pad, like the export board's. A client cannot work the list out (the ownership tables fill on the
// server only, #3366), so it asks the server for it while it stands at the bay, keeps the answer a few
// seconds, and the row reads the answer.
//
// The press names the paint (its option index and its spawn prefab's GUID), never a price or a prefab.
// The server finds the bay by the action it carries, resolves the payer's faction from the account, the
// uid from the player, the car from the owner's cars near the bay, rebuilds the list, and refuses a stale
// press (the GUID no longer matches that index). LF_LiveryShop.Respray then does the rest and its reason
// key comes back as a toast.

//------------------------------------------------------------------------------------------------
class LF_LiveryBay
{
	static const string FEATURE = "LiveryShop";
	//! The LF_Places kind of a bay marker; the island puts one on the Saint-Philippe garage.
	static const string PLACE_KIND = "LiveryBay";
	//! The action class a reader must stand at (LF_ActionSite): the paint rows on the bay prop.
	static const string ACTION = "LF_LiveryShopAction";
	static const string TITLE = "#LF-LiveryShop_Title";
	//! Most paint jobs one bay lists; matches the rows on the prefab.
	static const int MAX_ROWS = 6;	// lf-fixed: the bay's size
	//! How far from the bay prop the server answers a reader, in meters.
	static const float REACH = 5;	// lf-fixed: the rows' action radius with a step to spare
	//! A client asks again no sooner than this after its last ask.
	static const int ASK_GAP_MS = 2000;	// lf-fixed: under the comms limit (3 reads per 5 s) with room for a press
	//! An answer older than this no longer shows rows (the reader walked away or the ask was lost).
	static const int ANSWER_FRESH_MS = 6000;	// lf-fixed: three asks, so one lost reply does not flicker
	static const int TEXTS_PER_ROW = 2;	// lf-fixed: wire stride, not a tunable
	static const int NUMBERS_PER_ROW = 2;	// lf-fixed: wire stride, not a tunable

	// Client: what the last reply said, per row.
	protected static ref array<int> s_aClientIndexes;
	protected static ref array<string> s_aClientGuids;
	protected static ref array<string> s_aClientLabels;
	protected static int s_iAnswerTick;
	protected static int s_iAskTick;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aClientIndexes = null;
		s_aClientGuids = null;
		s_aClientLabels = null;
		s_iAnswerTick = 0;
		s_iAskTick = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the options this faction may buy, in OptionsFor order, with each one's index in the full
	//! list. A service livery is hidden from another job; the cap is the rows the prefab carries.
	static void VisibleOptions(string vehicleId, LF_Faction faction, notnull array<ref LF_LiveryOption> options, notnull array<int> indexes)
	{
		options.Clear();
		indexes.Clear();
		array<ref LF_LiveryOption> all = {};
		LF_LiveryShop.OptionsFor(vehicleId, all);
		foreach (int i, LF_LiveryOption option : all)
		{
			if (options.Count() >= MAX_ROWS)
				break;

			if (option.m_eFaction != LF_Faction.CIVILIAN && option.m_eFaction != faction)
				continue;

			options.Insert(option);
			indexes.Insert(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the wire form of a visible list: per row the name key and the spawn prefab's GUID, and
	//! the option index and the price.
	static void Pack(notnull array<ref LF_LiveryOption> options, notnull array<int> indexes, notnull array<string> texts, notnull array<int> numbers)
	{
		texts.Clear();
		numbers.Clear();
		foreach (int i, LF_LiveryOption option : options)
		{
			texts.Insert(option.m_sNameKey);
			texts.Insert(LF_VehiclePriceModel.GuidOf(option.m_SpawnPrefab));
			numbers.Insert(indexes[i]);
			numbers.Insert(option.m_iPrice);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "Paint: Police livery ($250)".
	static string RowLabel(string nameKey, int price)
	{
		return LF_Terms.Text("#LF-LiveryShop_Row", LF_Terms.Text(nameKey), price.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the bay (the entity carrying the rows) within reach of the player, or null.
	static IEntity BayNear(IEntity user)
	{
		if (!user)
			return null;

		return LF_ActionSite.FindNear(user.GetOrigin(), REACH, ACTION);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the faction a player's account holds; CIVILIAN with no account.
	static LF_Faction FactionOf(string uid)
	{
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (!manager || uid.IsEmpty())
			return LF_Faction.CIVILIAN;

		LF_PlayerAccount account = manager.GetAccount(uid);
		if (!account)
			return LF_Faction.CIVILIAN;

		return account.GetFaction();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: who is asking and what they ask about. False when the player stands at no bay or owns
	//! no car in it (not an error: the reader just has no rows).
	static bool Resolve(IEntity user, out IEntity bay, out string uid, out LF_Faction faction, out string vehicleId, out string padId)
	{
		bay = BayNear(user);
		uid = string.Empty;
		vehicleId = string.Empty;
		padId = string.Empty;
		faction = LF_Faction.CIVILIAN;
		if (!bay)
			return false;

		uid = LF_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
			return false;

		faction = FactionOf(uid);
		LF_VehicleSpawnPointComponent pad = LF_Component<LF_VehicleSpawnPointComponent>.Find(bay);
		if (!pad || pad.GetShopId().IsEmpty())
		{
			LF_Debug.Error(FEATURE, "the bay prop has no pad: no respray is offered there");
			return false;
		}

		padId = pad.GetShopId();
		vehicleId = LF_VehicleOwnership.OwnedVehicleNear(uid, bay.GetOrigin(), LF_LiveryShopConfig.Get().m_fBayRadius);
		return !vehicleId.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the rows for this reader, in the wire form. Empty arrays when no car of theirs is in the
	//! bay (the client then shows the hint row).
	static void ServerOffer(IEntity user, notnull array<string> texts, notnull array<int> numbers)
	{
		texts.Clear();
		numbers.Clear();
		if (LF_Systems.Refuses("Vehicles", "respray offer") || !Replication.IsServer())
			return;

		IEntity bay;
		string uid;
		LF_Faction faction;
		string vehicleId;
		string padId;
		if (!Resolve(user, bay, uid, faction, vehicleId, padId))
			return;

		array<ref LF_LiveryOption> options = {};
		array<int> indexes = {};
		VisibleOptions(vehicleId, faction, options, indexes);
		Pack(options, indexes, texts, numbers);
		LF_Debug.Log(FEATURE, string.Format("bay offer for %1: %2 paint jobs for %3", uid, options.Count(), vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the reader presses a row. `index` and `guid` name the paint as the client's list showed
	//! it; the server's own list must agree, or the press is stale and charges nothing.
	//! \return True when the car was repainted.
	static bool ServerBuy(IEntity user, int index, string guid)
	{
		if (!Replication.IsServer() || !user)
			return false;

		IEntity bay;
		string uid;
		LF_Faction faction;
		string vehicleId;
		string padId;
		if (!Resolve(user, bay, uid, faction, vehicleId, padId))
		{
			LF_Debug.Info(FEATURE, string.Format("respray press refused: %1 has no car in a bay", uid));
			LF_Utils.NotifyEntity(user, "#LF-LiveryShop_NotInBay", TITLE);
			return false;
		}

		array<ref LF_LiveryOption> all = {};
		LF_LiveryShop.OptionsFor(vehicleId, all);
		if (index < 0 || index >= all.Count() || LF_VehiclePriceModel.GuidOf(all[index].m_SpawnPrefab) != guid)
		{
			LF_Debug.Info(FEATURE, string.Format("respray press refused: option %1 (%2) is not what the bay lists for %3 now", index, guid, vehicleId));
			LF_Utils.NotifyEntity(user, "#LF-LiveryShop_NoOption", TITLE);
			return false;
		}

		int charged;
		string reasonKey;
		bool done = LF_LiveryShop.Respray(user, uid, faction, vehicleId, bay.GetOrigin(), padId, index, charged, reasonKey);
		LF_Utils.NotifyEntity(user, reasonKey, TITLE, 4.0);
		return done;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a press with no client list (the agent harness, a server-driven use): row `slot` of the
	//! reader's current list.
	static bool ServerBuySlot(IEntity user, int slot)
	{
		if (!Replication.IsServer() || !user)
			return false;

		IEntity bay;
		string uid;
		LF_Faction faction;
		string vehicleId;
		string padId;
		if (!Resolve(user, bay, uid, faction, vehicleId, padId))
		{
			LF_Utils.NotifyEntity(user, "#LF-LiveryShop_NotInBay", TITLE);
			return false;
		}

		array<ref LF_LiveryOption> options = {};
		array<int> indexes = {};
		VisibleOptions(vehicleId, faction, options, indexes);
		if (slot < 0 || slot >= options.Count())
		{
			LF_Utils.NotifyEntity(user, "#LF-LiveryShop_NoOption", TITLE);
			return false;
		}

		return ServerBuy(user, indexes[slot], LF_VehiclePriceModel.GuidOf(options[slot].m_SpawnPrefab));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: remember a reply. `nowMs` is the tick it arrived on.
	static void RememberAnswer(notnull array<string> texts, notnull array<int> numbers, int nowMs)
	{
		s_aClientIndexes = {};
		s_aClientGuids = {};
		s_aClientLabels = {};
		s_iAnswerTick = nowMs;
		int rows = Math.Min(numbers.Count() / NUMBERS_PER_ROW, texts.Count() / TEXTS_PER_ROW);
		if (rows == 0 && (numbers.Count() > 0 || texts.Count() > 0))
			LF_Debug.Error(FEATURE, string.Format("bay reply dropped: %1 texts and %2 numbers do not make one row", texts.Count(), numbers.Count()));

		for (int i = 0; i < rows && i < MAX_ROWS; i++)
		{
			s_aClientIndexes.Insert(numbers[i * NUMBERS_PER_ROW]);
			s_aClientGuids.Insert(texts[i * TEXTS_PER_ROW + 1]);
			s_aClientLabels.Insert(RowLabel(texts[i * TEXTS_PER_ROW], numbers[i * NUMBERS_PER_ROW + 1]));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: true while the last reply is recent enough to draw rows from.
	static bool AnswerFresh(int nowMs)
	{
		return s_iAnswerTick != 0 && nowMs - s_iAnswerTick <= ANSWER_FRESH_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the row `slot` shows (its option index, spawn GUID and label), false when it is empty.
	static bool RowAt(int slot, int nowMs, out int index, out string guid, out string label)
	{
		index = -1;
		guid = string.Empty;
		label = string.Empty;
		if (!AnswerFresh(nowMs) || !s_aClientIndexes || slot < 0 || slot >= s_aClientIndexes.Count())
			return false;

		index = s_aClientIndexes[slot];
		guid = s_aClientGuids[slot];
		label = s_aClientLabels[slot];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: true while a fresh reply holds no rows (the hint row shows).
	static bool AnswerEmpty(int nowMs)
	{
		return AnswerFresh(nowMs) && s_aClientIndexes && s_aClientIndexes.Count() == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: true when the reader may ask now.
	static bool ShouldAsk(int nowMs)
	{
		return s_iAskTick == 0 || nowMs - s_iAskTick >= ASK_GAP_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask for the list when a row is being evaluated and the last ask was a moment ago.
	static void MaybeAsk(IEntity user)
	{
		int now = System.GetTickCount();
		if (!ShouldAsk(now) || !BayNear(user))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		s_iAskTick = now;
		controller.LF_AskLiveryBay();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a press went out; the list changes (the car's own paint leaves it), so the rows go
	//! until the next ask (within ASK_GAP_MS) brings the new list.
	static void NotePress()
	{
		s_aClientIndexes = null;
		s_aClientGuids = null;
		s_aClientLabels = null;
		s_iAnswerTick = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LiveryBay's statics (LF_StaticReset finds it by reflection).
class LF_LiveryBay_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LiveryBay";
	}

	override void Apply()
	{
		LF_LiveryBay.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! One paint row on the bay prop. Slot N is the Nth paint job of the reader's car; slot -1 is the
//! hint row, shown while the bay has nothing for the reader.
class LF_LiveryShopAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Which paint job of the reader's car this row buys (0 = the first); -1 is the hint row")]
	protected int m_iSlot;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Vehicles") || !user)
			return false;

		if (m_iSlot <= 0)
			LF_LiveryBay.MaybeAsk(user);

		int now = System.GetTickCount();
		if (m_iSlot < 0)
			return LF_LiveryBay.AnswerEmpty(now);

		int index;
		string guid;
		string label;
		return LF_LiveryBay.RowAt(m_iSlot, now, index, guid, label);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (m_iSlot < 0)
		{
			SetCannotPerformReason("#LF-LiveryShop_NotInBay");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_iSlot < 0)
		{
			outName = WidgetManager.Translate("#LF-LiveryShop_Hint");
			return true;
		}

		int index;
		string guid;
		string label;
		if (!LF_LiveryBay.RowAt(m_iSlot, System.GetTickCount(), index, guid, label))
			return false;

		outName = label;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Vehicles", "LF_LiveryShopAction"))
			return;

		if (m_iSlot < 0 || !pOwnerEntity || !pUserEntity)
			return;

		// A server-driven use (the agent harness): the server buys that slot for that reader directly.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			LF_LiveryBay.ServerBuySlot(pUserEntity, m_iSlot);
			return;
		}

		int index;
		string guid;
		string label;
		if (!LF_LiveryBay.RowAt(m_iSlot, System.GetTickCount(), index, guid, label))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_LiveryBay.FEATURE, "respray press not sent: no SCR_PlayerController");
			return;
		}

		LF_LiveryBay.NotePress();
		controller.LF_BuyLivery(index, guid);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the paint jobs of this player's car at the bay they stand at.
	void LF_AskLiveryBay()
	{
		if (Replication.IsServer())
			RpcAsk_LF_LiveryBay();
		else
			Rpc(RpcAsk_LF_LiveryBay);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: press a row; the server checks the whole purchase again.
	void LF_BuyLivery(int index, string guid)
	{
		if (Replication.IsServer())
			RpcAsk_LF_LiveryBuy(index, guid);
		else
			Rpc(RpcAsk_LF_LiveryBuy, index, guid);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LiveryBay()
	{
		// The shared board comms limit comes first: a spamming client cannot flood the server.
		if (!LF_BoardProviders.AllowReadAt(GetPlayerId(), System.GetTickCount(), false))
			return;

		array<string> texts = {};
		array<int> numbers = {};
		LF_LiveryBay.ServerOffer(GetControlledEntity(), texts, numbers);
		if (LF_IsLocalController())
			RpcDo_LF_LiveryBay(texts, numbers);
		else
			Rpc(RpcDo_LF_LiveryBay, texts, numbers);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LiveryBuy(int index, string guid)
	{
		if (!LF_BoardProviders.AllowReadAt(GetPlayerId(), System.GetTickCount()))
			return;

		// The GUID comes from the client: bound it before it is compared or logged.
		if (guid.Length() > 40)
			return;

		LF_LiveryBay.ServerBuy(GetControlledEntity(), index, guid);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_LiveryBay(array<string> texts, array<int> numbers)
	{
		array<string> shownTexts = {};
		array<int> shownNumbers = {};
		if (texts)
			shownTexts.Copy(texts);

		if (numbers)
			shownNumbers.Copy(numbers);

		LF_LiveryBay.RememberAnswer(shownTexts, shownNumbers, System.GetTickCount());
	}
}
