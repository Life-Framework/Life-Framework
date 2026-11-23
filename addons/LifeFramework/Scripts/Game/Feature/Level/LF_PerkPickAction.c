//------------------------------------------------------------------------------------------------
//! One row on the labour office counter (WK-W8b, #4416), or on a duty board (police, clinic, #4573): "Take Double yield (Miner)" or "Change to
//! Swift hands (Miner), $500". Each row is one perk at one pick level; a take row shows when that perk
//! is on offer in some trade (the first in counter order), a change row names its own trade
//! (`m_sTrade`, #4525), so two trades that both hold a pick each get a change row. A row is a free take or a
//! paid change (`m_bChange`), never both, so a stale label cannot turn a take into a charge. The client sends no
//! trade or price: the server reads the level and perk stores, takes the fee and says how it went.
class LF_PerkPickAction : ScriptedUserAction
{
	[Attribute("10", UIWidgets.EditBox, "Pick level this row belongs to (10 or 20)", "1 100 1")]
	protected int m_iAtLevel;

	[Attribute("double_yield", UIWidgets.EditBox, "Perk id this row takes (LF_PerkEffects)")]
	protected string m_sPerk;

	[Attribute("0", UIWidgets.CheckBox, "A paid change row; off for a free first pick")]
	protected bool m_bChange;

	[Attribute("500", UIWidgets.EditBox, "Cash a change costs; a first pick is free", "0 100000 1")]
	protected int m_iFee;

	[Attribute("", UIWidgets.EditBox, "Trade (job id) a change row serves; empty serves the first trade that offers the perk")]
	protected string m_sTrade;

	[Attribute("#LF-PerkPick_Title", UIWidgets.EditBox, "Toast title: the place the pick is made (labor office, police station, clinic)")]
	protected string m_sTitle;

	//! The offer this row showed last, for the name.
	protected ref LF_PerkPickOffer m_Shown;

	//------------------------------------------------------------------------------------------------
	//! The offer this row stands for, for a user; null hides the row. The server (and a listen host)
	//! reads the stores; a client reads its owner sync (LF_PerkSync, sent at spawn). Before the sync
	//! arrives a take row shows in general words and the server answers; a trade's change row hides.
	LF_PerkPickOffer OfferFor(IEntity user)
	{
		if (!user)
			return null;

		array<ref LF_PerkPickOffer> offers = {};
		if (!Replication.IsRunning() || Replication.IsServer())
		{
			LF_PerkPick.Offers(LF_Utils.GetPlayerUID(user), offers);
		}
		else if (!LF_PerkPick.OffersSynced(LF_Utils.GetPlayerUID(user), offers))
		{
			if (!m_sTrade.IsEmpty())
				return null;

			return LF_PerkPickOffer.Make(string.Empty, m_iAtLevel, m_sPerk, m_bChange);
		}

		return LF_PerkPick.Find(offers, m_iAtLevel, m_sPerk, m_bChange, m_sTrade);
	}

	//------------------------------------------------------------------------------------------------
	//! The row's words for an offer.
	string NameOf(LF_PerkPickOffer offer)
	{
		string perk = WidgetManager.Translate(LF_PerkPick.PerkNameKey(m_sPerk));
		if (!offer || offer.m_sTrade.IsEmpty())
		{
			if (m_bChange)
				return WidgetManager.Translate("#LF-PerkPick_ChangeAny", perk, LF_MoneyFormat.FormatMoney(m_iFee));
			return WidgetManager.Translate("#LF-PerkPick_TakeAny", perk);
		}

		string trade = WidgetManager.Translate(LF_ItemCatalogue.JobNameKey(offer.m_sTrade));
		if (offer.m_bChange)
			return WidgetManager.Translate("#LF-PerkPick_Change", perk, trade, LF_MoneyFormat.FormatMoney(m_iFee));

		return WidgetManager.Translate("#LF-PerkPick_Take", perk, trade);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		m_Shown = OfferFor(user);
		return m_Shown != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = NameOf(m_Shown);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		if (!pUserEntity)
		{
			LF_Debug.Error(LF_PerkPick.FEATURE, "counter row performed without a user");
			return;
		}

		LF_EPerkPickResult result = LF_PerkPick.Take(pUserEntity, m_iAtLevel, m_sPerk, m_iFee, m_bChange, m_sTrade);

		int playerId = 0;
		if (GetGame().GetPlayerManager())
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		LF_ENotificationType type = LF_ENotificationType.WARNING;
		if (result == LF_EPerkPickResult.TAKEN || result == LF_EPerkPickResult.CHANGED)
			type = LF_ENotificationType.SUCCESS;
		LF_NotificationManagerComponent.NotifyPlayer(playerId, m_sTitle, LF_PerkPick.ResultKey(result), 5.0, type, LF_MoneyFormat.FormatMoney(m_iFee));
	}

	//------------------------------------------------------------------------------------------------
	int GetAtLevel()
	{
		return m_iAtLevel;
	}

	//------------------------------------------------------------------------------------------------
	string GetPerk()
	{
		return m_sPerk;
	}

	//------------------------------------------------------------------------------------------------
	bool IsChange()
	{
		return m_bChange;
	}

	//------------------------------------------------------------------------------------------------
	string GetTrade()
	{
		return m_sTrade;
	}
}
