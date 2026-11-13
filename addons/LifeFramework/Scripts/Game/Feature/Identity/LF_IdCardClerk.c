//------------------------------------------------------------------------------------------------
//! The town clerk replaces a lost or stolen identity card (#902): a reply in the clerk's
//! conversation (Configs/Dialogue/LF_Dialogue_Clerk.conf). The clerk takes the fee
//! (LF_PhotoIdCardConfig.ReplacementFee) in carried cash, takes a new photo, and prints a new card
//! with a new number and today's date; the old card is void from then on.
//------------------------------------------------------------------------------------------------
class LF_IdCardClerk
{
	static const string LEDGER_REASON = "id-card";

	//------------------------------------------------------------------------------------------------
	//! Server: replace the player's card. \return The outcome; OK when a new card was handed over.
	static LF_EIdReplace Replace(IEntity player, LF_CashWallet wallet, int fee)
	{
		if (!Replication.IsServer() || !player || !wallet)
			return LF_EIdReplace.NEVER_ISSUED;

		LF_PlayerAccount account = LF_PoliceUtils.GetAccountForEntity(player);
		if (!account || !account.GetActiveCharacter())
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, "replacement refused: no registered character for this player");
			return LF_EIdReplace.NEVER_ISSUED;
		}

		LF_PlayerCharacter identity = account.GetActiveCharacter();
		return ReplaceFor(player, identity.GetId(), identity.GetFirstName(), identity.GetLastName(), identity.GetAge(), wallet, fee);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Replace with the identity given (the seam a WORLD test drives).
	static LF_EIdReplace ReplaceFor(IEntity player, string holderId, string firstName, string lastName, int age, LF_CashWallet wallet, int fee)
	{
		LF_EIdReplace check = LF_PhotoIdCard.CheckReplace(LF_PhotoIdCard.Current(holderId) != null, LF_IdCard.CarriesValid(player, holderId), wallet.Carried(), fee);
		if (check != LF_EIdReplace.OK)
		{
			LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("replacement refused for character %1: %2 (fee %3, carried %4)", holderId, typename.EnumToString(LF_EIdReplace, check), fee, wallet.Carried()));
			return check;
		}

		if (fee > 0 && wallet.Take(fee) != fee)
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, string.Format("replacement refused for character %1: the fee of %2 could not be taken", holderId, fee));
			return LF_EIdReplace.NOT_ENOUGH_CASH;
		}

		string voided;
		LF_PhotoIdRow old = LF_PhotoIdCard.Current(holderId);
		if (old)
			voided = old.m_sNumber;

		if (!LF_IdCard.Grant(player, holderId, firstName, lastName, age))
		{
			if (fee > 0)
				wallet.Put(fee);

			LF_Debug.Error(LF_PhotoIdCard.FEATURE, string.Format("replacement for character %1 not handed over; the fee of %2 was given back", holderId, fee));
			return LF_EIdReplace.NEVER_ISSUED;
		}

		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("replacement card %1 for character %2 for %3 cash; card %4 is void", LF_PhotoIdCard.Current(holderId).m_sNumber, holderId, fee, voided));
		return LF_EIdReplace.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The clerk's answer for an outcome.
	static string AnswerKey(LF_EIdReplace outcome)
	{
		if (outcome == LF_EIdReplace.OK)
			return "#LF-IdCard_ClerkReplaced";

		if (outcome == LF_EIdReplace.STILL_CARRIED)
			return "#LF-IdCard_ClerkStillCarried";

		if (outcome == LF_EIdReplace.NOT_ENOUGH_CASH)
			return "#LF-IdCard_ClerkNoCash";

		return "#LF-IdCard_ClerkNeverIssued";
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the player was issued a card and does not carry their valid one (the reply about
//! losing it is offered).
class LF_DialogueHook_IdCardMissing : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_PlayerAccount account = LF_PoliceUtils.GetAccountForEntity(player);
		if (!account || !account.GetActiveCharacter())
			return false;

		string holderId = account.GetActiveCharacter().GetId();
		return LF_PhotoIdCard.Current(holderId) != null && !LF_IdCard.CarriesValid(player, holderId);
	}
}

//------------------------------------------------------------------------------------------------
//! Action: "I have lost my identity card." The clerk replaces it for the fee.
class LF_DialogueHook_IdCardReplace : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		int fee = LF_PhotoIdCardConfig.ReplacementFee();
		LF_EIdReplace outcome = LF_IdCardClerk.Replace(player, new LF_EntityCashWallet(player, LF_IdCardClerk.LEDGER_REASON, string.Empty), fee);
		spoken.Insert(LF_IdCardClerk.AnswerKey(outcome));

		int playerId = LF_PlayerInteraction.PlayerIdOf(player);
		if (outcome == LF_EIdReplace.OK)
		{
			LF_PlayerInteraction.PlayHandoverSound(player, LF_PlayerInteraction.SOUND_HANDOVER);
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-ID_CardTitle", "#LF-IdCard_ReplacedToast", 5.0, LF_ENotificationType.INFO, LF_MoneyFormat.FormatMoney(fee));
		}
		else if (outcome == LF_EIdReplace.NOT_ENOUGH_CASH)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-ID_CardTitle", "#LF-IdCard_FeeToast", 5.0, LF_ENotificationType.WARNING, LF_MoneyFormat.FormatMoney(fee));
		}
	}
}
