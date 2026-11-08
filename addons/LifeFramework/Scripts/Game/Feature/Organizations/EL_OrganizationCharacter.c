//------------------------------------------------------------------------------------------------
//! Client->server bridge for the registry ledger (LF_OrgLedger). The character is the entity
//! the player owns, so the client can RPC it. Every request that changes the register (found,
//! cash, upgrade, lease, leave) is refused away from the desk; the server checks the character,
//! moves MoneyStack, and answers with the ledger page.
//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationView()
	{
		Rpc(RpcAsk_EL_OrganizationView);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationCreate(string name)
	{
		Rpc(RpcAsk_EL_OrganizationCreate, name);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationDeposit(int amount)
	{
		Rpc(RpcAsk_EL_OrganizationDeposit, amount);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationWithdraw(int amount)
	{
		Rpc(RpcAsk_EL_OrganizationWithdraw, amount);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationBuy(int upgrade)
	{
		Rpc(RpcAsk_EL_OrganizationBuy, upgrade);
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
	protected void RpcAsk_EL_OrganizationView()
	{
		EL_PushOrganizationView("");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationCreate(string name)
	{
		if (EL_RefuseAwayFromOrganizationDesk("found", 0))
		{
			EL_PushOrganizationView("#EL-Org_TooFar");
			return;
		}

		string refusal = EL_OrganizationPlayer.Found(this, name);
		LF_OrgReplication.Publish(this);
		if (refusal.IsEmpty())
		{
			EL_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(this), "#LF-Org_Title", "#LF-Org_Founded", 5.0, EL_ENotificationType.SUCCESS, name.Trim());
			EL_PushOrganizationView("#LF-Org_FoundedLine");
			return;
		}

		EL_PushOrganizationView(refusal);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationDeposit(int amount)
	{
		string notice = "#LF-Org_CashRefused";
		if (!EL_RefuseAwayFromOrganizationDesk("deposit", amount) && EL_OrganizationPlayer.DepositCash(this, amount))
			notice = "#LF-Org_Deposited";

		EL_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationWithdraw(int amount)
	{
		string notice = "#LF-Org_CashRefused";
		if (!EL_RefuseAwayFromOrganizationDesk("withdraw", amount) && EL_OrganizationPlayer.WithdrawCash(this, amount))
			notice = "#LF-Org_Withdrawn";

		EL_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationLeave()
	{
		string notice = "#LF-Org_LeaveRefused";
		if (!EL_RefuseAwayFromOrganizationDesk("leave", 0))
		{
			string characterId = EL_OrganizationPlayer.CharacterId(this);
			if (!characterId.IsEmpty() && EL_OrganizationManager.Leave(characterId))
				notice = "#LF-Org_Left";
		}

		LF_OrgReplication.Publish(this);
		EL_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationLease()
	{
		string notice = "#LF-Org_LeaseRefused";
		if (!EL_RefuseAwayFromOrganizationDesk("lease", 0))
		{
			string characterId = EL_OrganizationPlayer.CharacterId(this);
			if (!characterId.IsEmpty() && LF_OrgHideoutLease.Lease(characterId, LF_Hideout.HIDEOUT_ID))
				notice = "#LF-Org_Leased";
		}

		EL_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	//! Register changes happen only at the desk. The ledger stays open after the player walks off,
	//! and a client can send the RPC without it, so the server looks for the desk itself.
	//! \return true when the request was refused.
	protected bool EL_RefuseAwayFromOrganizationDesk(string request, int amount)
	{
		if (EL_OrganizationPlayer.IsAtDesk(GetOrigin()))
			return false;

		EL_Debug.Warn("Organizations", string.Format("%1 refused: not at an organization desk (amount=%2 uid %3)", request, amount, EL_Utils.GetPlayerUid(this)));
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int playerId = playerManager.GetPlayerIdFromControlledEntity(this);
			if (playerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Org_Title", "#EL-Org_TooFar", 3.0, EL_ENotificationType.WARNING);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationBuy(int upgrade)
	{
		EL_EOrganizationUpgrade kind = EL_EOrganizationUpgrade.PROCESSING_SLOT;
		if (upgrade == EL_EOrganizationUpgrade.EXTRA_STORAGE)
			kind = EL_EOrganizationUpgrade.EXTRA_STORAGE;
		else if (upgrade == EL_EOrganizationUpgrade.PARKED_VEHICLE)
			kind = EL_EOrganizationUpgrade.PARKED_VEHICLE;
		else if (upgrade != EL_EOrganizationUpgrade.PROCESSING_SLOT)
		{
			EL_Debug.Error("Organizations", string.Format("upgrade refused: unknown id %1", upgrade));
			EL_PushOrganizationView("#LF-Org_BuyRefused");
			return;
		}

		string notice = "#LF-Org_BuyRefused";
		if (!EL_RefuseAwayFromOrganizationDesk("buy", 0) && EL_OrganizationPlayer.BuyUpgrade(this, kind))
			notice = "#LF-Org_Bought";

		EL_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the whole ledger page for this player, plus a notice key for the ink line.
	protected void EL_PushOrganizationView(string notice)
	{
		if (!Replication.IsServer())
			return;

		bool hasOrg = false;
		string name = "";
		int fund = 0;
		int rights = 0;
		bool processing = false;
		int lease = LF_EOrgLease.FREE;
		string members = "";

		string characterId = EL_OrganizationPlayer.CharacterId(this);
		EL_Organization org;
		if (!characterId.IsEmpty())
			org = EL_OrganizationManager.GetByCharacter(characterId);

		if (org)
		{
			hasOrg = true;
			name = org.GetName();
			fund = org.GetFund();
			rights = LF_OrgRules.RightsOf(org, characterId);
			processing = org.HasProcessingSlot();
			lease = LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.HIDEOUT_ID);
			members = EL_OrganizationPlayer.MemberLines(org);
		}

		Rpc(RpcDo_LF_OrganizationLedger, hasOrg, name, fund, rights, processing, lease, members, notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OrganizationLedger(bool hasOrg, string name, int fund, int rights, bool processing, int lease, string members, string notice)
	{
		LF_OrgLedger ledger = LF_OrgLedger.Current();
		if (!ledger)
			return;

		ledger.ApplyView(LF_OrgLedgerPage.Create(hasOrg, name, fund, rights, processing, lease, members), notice);
	}
}
