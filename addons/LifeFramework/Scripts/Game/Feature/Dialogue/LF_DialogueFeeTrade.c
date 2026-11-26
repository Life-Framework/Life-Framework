//------------------------------------------------------------------------------------------------
//! An NPC sells one item for a cash fee from a reply (#4381), such as a pack clerk's key. The
//! reply names the action `FeeForItem` with the param `<fee>|<item prefab>`, for example
//! `400|{GUID}path/to/the/item.et`. The server checks the carried cash,
//! takes the fee, hands the item over and gives the fee back when the item could not be handed
//! over. Cash is never created or lost.
//!
//! The NPC answers with one of the ANSWER_* keys. A dialogue voices an answer by giving one of its
//! lines that key as `for_key` (the tally clerk's "key" line answers ANSWER_PAID); an answer with
//! no voiced line is shown as a caption.
//------------------------------------------------------------------------------------------------
enum LF_EFeeTrade
{
	SOLD,
	BAD_PARAM,
	NO_PLAYER,
	ALREADY_HELD,
	NOT_ENOUGH_CASH,
	TAKE_FAILED,
	NO_ROOM
}

//------------------------------------------------------------------------------------------------
class LF_DialogueFeeTrade
{
	static const string LEDGER_REASON = "dialogue-fee";
	static const string LEDGER_REFUND = "dialogue-fee-refund";
	static const string PARAM_SEPARATOR = "|";

	static const string ANSWER_PAID = "#LF-Dialogue_FeePaid";
	static const string ANSWER_SHORT = "#LF-Dialogue_FeeShort";
	static const string ANSWER_HELD = "#LF-Dialogue_FeeHeld";
	static const string ANSWER_FAILED = "#LF-Dialogue_FeeFailed";

	//------------------------------------------------------------------------------------------------
	//! Reads `<fee>|<prefab>`. \return false (fee 0, prefab empty) when either part is missing or the fee is not positive.
	static bool ParseParam(string param, out int fee, out ResourceName prefab)
	{
		fee = 0;
		prefab = ResourceName.Empty;
		int bar = param.IndexOf(PARAM_SEPARATOR);
		if (bar <= 0)
			return false;

		string feeText = param.Substring(0, bar).Trim();
		string prefabText = param.Substring(bar + 1, param.Length() - bar - 1).Trim();
		int parsed = feeText.ToInt();
		if (parsed <= 0 || prefabText.IsEmpty() || parsed.ToString() != feeText)
			return false;

		fee = parsed;
		prefab = prefabText;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The answer key the NPC says for an outcome.
	static string AnswerKey(LF_EFeeTrade outcome)
	{
		if (outcome == LF_EFeeTrade.SOLD)
			return ANSWER_PAID;

		if (outcome == LF_EFeeTrade.NOT_ENOUGH_CASH)
			return ANSWER_SHORT;

		if (outcome == LF_EFeeTrade.ALREADY_HELD)
			return ANSWER_HELD;

		return ANSWER_FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sell `prefab` to `player` for `fee` carried cash.
	LF_EFeeTrade Sell(IEntity player, notnull LF_CashWallet wallet, int fee, ResourceName prefab)
	{
		if (fee <= 0 || prefab.IsEmpty())
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("fee trade refused: bad fee %1 or prefab '%2'", fee, prefab));
			return LF_EFeeTrade.BAD_PARAM;
		}

		if (!player)
			return LF_EFeeTrade.NO_PLAYER;

		if (Holds(player, prefab))
		{
			LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("fee trade refused: the player already carries %1", prefab));
			return LF_EFeeTrade.ALREADY_HELD;
		}

		int carried = wallet.Carried();
		if (carried < fee)
		{
			LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("fee trade refused: carries %1 of the %2 fee for %3", carried, fee, prefab));
			return LF_EFeeTrade.NOT_ENOUGH_CASH;
		}

		int taken = wallet.Take(fee);
		if (taken != fee)
		{
			if (taken > 0)
				Refund(player, wallet, taken);

			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("fee trade refused: took %1 of the %2 fee for %3; given back", taken, fee, prefab));
			return LF_EFeeTrade.TAKE_FAILED;
		}

		if (!Give(player, prefab))
		{
			Refund(player, wallet, fee);
			LF_Debug.Warn(LF_Dialogue.FEATURE, string.Format("fee trade undone: %1 could not be handed over; the fee of %2 was given back", prefab, fee));
			return LF_EFeeTrade.NO_ROOM;
		}

		LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("fee trade: sold %1 for %2 cash", prefab, fee));
		return LF_EFeeTrade.SOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the player already carries one (a second copy for a second fee helps no one).
	protected bool Holds(IEntity player, ResourceName prefab)
	{
		return LF_InventoryUtils.GetAmount(player, prefab) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the item in the player's inventory. \return false when it did not fit or did not spawn.
	protected bool Give(IEntity player, ResourceName prefab)
	{
		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(player.FindComponent(InventoryStorageManagerComponent));
		if (!storage)
			return false;

		return LF_InventoryUtils.AddItem(storage, prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives `amount` back; what the wallet cannot hold lands at the player's feet, so none is lost.
	protected void Refund(IEntity player, LF_CashWallet wallet, int amount)
	{
		int back = wallet.Put(amount);
		if (back < 0)
			back = 0;

		if (back >= amount)
			return;

		int dropped = LF_MoneyUtils.AddCash(player, amount - back, LEDGER_REFUND, string.Empty, true);
		if (back + dropped != amount)
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("fee refund short: %1 of %2 went back", back + dropped, amount));
	}
}

//------------------------------------------------------------------------------------------------
//! Action: pay the NPC's fee for an item. Param `<fee>|<item prefab>`.
// lf-spoken: #LF-Dialogue_FeePaid #LF-Dialogue_FeeShort #LF-Dialogue_FeeHeld #LF-Dialogue_FeeFailed
// lf-not-spoken: #LF-Dialogue_FeeTitle #LF-Dialogue_FeePaidToast
class LF_DialogueHook_FeeForItem : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		int fee;
		ResourceName prefab;
		LF_EFeeTrade outcome = LF_EFeeTrade.BAD_PARAM;
		if (LF_DialogueFeeTrade.ParseParam(param, fee, prefab))
		{
			LF_DialogueFeeTrade trade = new LF_DialogueFeeTrade();
			LF_EntityCashWallet wallet = new LF_EntityCashWallet(player, LF_DialogueFeeTrade.LEDGER_REASON, string.Empty);
			outcome = trade.Sell(player, wallet, fee, prefab);
		}
		else
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("FeeForItem param '%1' is not '<fee>|<prefab>'", param));

		spoken.Insert(LF_DialogueFeeTrade.AnswerKey(outcome));
		if (outcome != LF_EFeeTrade.SOLD)
			return;

		LF_PlayerInteraction.PlayHandoverSound(player, LF_PlayerInteraction.SOUND_HANDOVER);
		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(player), "#LF-Dialogue_FeeTitle", "#LF-Dialogue_FeePaidToast", 5.0, LF_ENotificationType.INFO, LF_MoneyFormat.FormatMoney(fee));
	}
}
