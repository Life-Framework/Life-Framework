//------------------------------------------------------------------------------------------------
//! Client->server bridge for the registry ledger (LF_OrgLedger). The character is the entity
//! the player owns, so the client can RPC it. Every request that changes the register (found,
//! cash, upgrade, lease, leave) is refused away from the desk; the server checks the character,
//! moves MoneyStack, and answers with the ledger page.
//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationView()
	{
		Rpc(RpcAsk_LF_OrganizationView);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationCreate(string name)
	{
		Rpc(RpcAsk_LF_OrganizationCreate, name);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationDeposit(int amount)
	{
		Rpc(RpcAsk_LF_OrganizationDeposit, amount);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationWithdraw(int amount)
	{
		Rpc(RpcAsk_LF_OrganizationWithdraw, amount);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationBuy(int upgrade)
	{
		Rpc(RpcAsk_LF_OrganizationBuy, upgrade);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationLeave()
	{
		Rpc(RpcAsk_LF_OrganizationLeave);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationLease()
	{
		Rpc(RpcAsk_LF_OrganizationLease);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationView()
	{
		LF_PushOrganizationView("");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationCreate(string name)
	{
		if (!LF_RateLimit.AllowCharacter(this, LF_ERequestKind.PURCHASE, "organization founding"))
			return;

		if (LF_RefuseAwayFromOrganizationDesk("found", 0))
		{
			LF_PushOrganizationView("#EL-Org_TooFar");
			return;
		}

		string refusal = LF_OrganizationPlayer.Found(this, name);
		LF_OrgReplication.Publish(this);
		if (refusal.IsEmpty())
		{
			LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(this), "#LF-Org_Title", "#LF-Org_Founded", 5.0, LF_ENotificationType.SUCCESS, name.Trim());
			LF_PushOrganizationView("#LF-Org_FoundedLine");
			return;
		}

		LF_PushOrganizationView(refusal);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationDeposit(int amount)
	{
		string notice = "#LF-Org_CashRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("deposit", amount) && LF_OrganizationPlayer.DepositCash(this, amount))
			notice = "#LF-Org_Deposited";

		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationWithdraw(int amount)
	{
		string notice = "#LF-Org_CashRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("withdraw", amount) && LF_OrganizationPlayer.WithdrawCash(this, amount))
			notice = "#LF-Org_Withdrawn";

		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationLeave()
	{
		string notice = "#LF-Org_LeaveRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("leave", 0))
		{
			string characterId = LF_OrganizationPlayer.CharacterId(this);
			if (!characterId.IsEmpty() && LF_OrganizationManager.Leave(characterId))
				notice = "#LF-Org_Left";
		}

		LF_OrgReplication.Publish(this);
		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationLease()
	{
		string notice = "#LF-Org_LeaseRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("lease", 0))
		{
			string characterId = LF_OrganizationPlayer.CharacterId(this);
			if (!characterId.IsEmpty() && LF_OrgHideoutLease.Lease(characterId, LF_Hideout.OrgHideoutId()))
				notice = "#LF-Org_Leased";
		}

		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	//! Register changes happen only at the desk. The ledger stays open after the player walks off,
	//! and a client can send the RPC without it, so the server looks for the desk itself.
	//! \return true when the request was refused.
	protected bool LF_RefuseAwayFromOrganizationDesk(string request, int amount)
	{
		if (LF_OrganizationPlayer.IsAtDesk(GetOrigin()))
			return false;

		LF_Debug.Warn("Organizations", string.Format("%1 refused: not at an organization desk (amount=%2 uid %3)", request, amount, LF_Utils.GetPlayerUid(this)));
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int playerId = playerManager.GetPlayerIdFromControlledEntity(this);
			if (playerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Org_Title", "#EL-Org_TooFar", 3.0, LF_ENotificationType.WARNING);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationBuy(int upgrade)
	{
		if (!LF_RateLimit.AllowCharacter(this, LF_ERequestKind.PURCHASE, "organization upgrade"))
			return;

		LF_EOrganizationUpgrade kind = LF_EOrganizationUpgrade.PROCESSING_SLOT;
		if (upgrade == LF_EOrganizationUpgrade.EXTRA_STORAGE)
			kind = LF_EOrganizationUpgrade.EXTRA_STORAGE;
		else if (upgrade == LF_EOrganizationUpgrade.PARKED_VEHICLE)
			kind = LF_EOrganizationUpgrade.PARKED_VEHICLE;
		else if (upgrade != LF_EOrganizationUpgrade.PROCESSING_SLOT)
		{
			LF_Debug.Error("Organizations", string.Format("upgrade refused: unknown id %1", upgrade));
			LF_PushOrganizationView("#LF-Org_BuyRefused");
			return;
		}

		string notice = "#LF-Org_BuyRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("buy", 0) && LF_OrganizationPlayer.BuyUpgrade(this, kind))
			notice = "#LF-Org_Bought";

		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the whole ledger page for this player, plus a notice key for the ink line.
	protected void LF_PushOrganizationView(string notice)
	{
		if (!Replication.IsServer())
			return;

		string name = "";
		int fund = 0;
		int rights = 0;
		bool processing = false;
		int lease = LF_EOrgLease.FREE;
		string members = "";
		string rank = "";

		string characterId = LF_OrganizationPlayer.CharacterId(this);
		LF_Organization org;
		if (!characterId.IsEmpty())
			org = LF_OrganizationManager.GetByCharacter(characterId);

		if (org)
		{
			name = org.GetName();
			fund = org.GetFund();
			rights = LF_OrgRules.RightsOf(org, characterId);
			processing = org.HasProcessingSlot();
			lease = LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.OrgHideoutId());
			members = LF_OrganizationPlayer.MemberLines(org);
			rank = org.GetRankName(characterId);
		}

		// No hasOrg argument: a registered name is never empty, so the name says it (Rpc takes eight).
		Rpc(RpcDo_LF_OrganizationLedger, name, fund, rights, processing, lease, members, rank, notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OrganizationLedger(string name, int fund, int rights, bool processing, int lease, string members, string rank, string notice)
	{
		LF_OrgLedger ledger = LF_OrgLedger.Current();
		if (!ledger)
			return;

		ledger.ApplyView(LF_OrgLedgerPage.Create(!name.IsEmpty(), name, fund, rights, processing, lease, members, rank), notice);
	}
}
