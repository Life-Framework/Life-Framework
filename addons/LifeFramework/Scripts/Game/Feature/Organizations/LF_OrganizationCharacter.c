//------------------------------------------------------------------------------------------------
//! Client->server bridge for the registry ledger (LF_OrgLedger). The character is the entity
//! the player owns, so the client can RPC it. Every request that changes the register (found,
//! cash, upgrade, lease, leave) is refused away from the desk; the server checks the character,
//! moves MoneyStack, and answers with the ledger page.
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//! Rate limit for the ledger asks (review 2026-11-17 items 30, 31, #2415). View, Deposit, Withdraw,
//! Leave and Lease share the PURCHASE bucket with founding and upgrades: they are the same desk
//! and the same money, and View walks every member of the organisation. The test org sign has no
//! bucket of its own to hide behind, so it is not touched here.
class LF_OrgLedgerGate
{
	//------------------------------------------------------------------------------------------------
	//! Server. Counts one ledger ask against the player's bucket. A refusal logs LF_RateLimit's warning.
	//! \param request Short name for the log line, e.g. "view".
	//! \return true when the ask may proceed.
	static bool Admit(int playerId, string request, bool notify = true)
	{
		return LF_RateLimit.Allow(playerId, LF_ERequestKind.PURCHASE, "organization " + request, notify);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Admit for an ask that arrived on a character.
	static bool AdmitCharacter(IEntity character, string request)
	{
		return LF_RateLimit.AllowCharacter(character, LF_ERequestKind.PURCHASE, "organization " + request);
	}
}
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationView()
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationView();
			return;
		}

		Rpc(RpcAsk_LF_OrganizationView);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationCreate(string name)
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationCreate(name);
			return;
		}

		Rpc(RpcAsk_LF_OrganizationCreate, name);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationDeposit(int amount)
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationDeposit(amount);
			return;
		}

		Rpc(RpcAsk_LF_OrganizationDeposit, amount);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationWithdraw(int amount)
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationWithdraw(amount);
			return;
		}

		Rpc(RpcAsk_LF_OrganizationWithdraw, amount);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationBuy(int upgrade)
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationBuy(upgrade);
			return;
		}

		Rpc(RpcAsk_LF_OrganizationBuy, upgrade);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationLeave()
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationLeave();
			return;
		}

		Rpc(RpcAsk_LF_OrganizationLeave);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskOrganizationLease()
	{
		// A listen host is the server: its own RPC never reaches its handler, so call it directly.
		if (Replication.IsServer())
		{
			RpcAsk_LF_OrganizationLease();
			return;
		}

		Rpc(RpcAsk_LF_OrganizationLease);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationView()
	{
		if (LF_RefuseOrganizationsOff("view"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "view"))
			return;

		LF_PushOrganizationView("");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationCreate(string name)
	{
		if (LF_RefuseOrganizationsOff("found"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "founding"))
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
		if (LF_RefuseOrganizationsOff("deposit"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "deposit"))
			return;

		string notice = "#LF-Org_CashRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("deposit", amount) && LF_OrganizationPlayer.DepositCash(this, amount))
			notice = "#LF-Org_Deposited";

		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationWithdraw(int amount)
	{
		if (LF_RefuseOrganizationsOff("withdraw"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "withdraw"))
			return;

		string notice = "#LF-Org_CashRefused";
		if (!LF_RefuseAwayFromOrganizationDesk("withdraw", amount) && LF_OrganizationPlayer.WithdrawCash(this, amount))
			notice = "#LF-Org_Withdrawn";

		LF_PushOrganizationView(notice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrganizationLeave()
	{
		if (LF_RefuseOrganizationsOff("leave"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "leave"))
			return;

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
		if (LF_RefuseOrganizationsOff("lease"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "lease"))
			return;

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
	//! Server: a request while Organizations is off (#1074) is refused with the caption, and the ledger
	//! page the player may still have open shows it.
	//! \return true when the request was refused.
	protected bool LF_RefuseOrganizationsOff(string request)
	{
		if (!LF_Systems.Refuses(LF_OrgRules.SYSTEM, "organization " + request))
			return false;

		LF_PushOrganizationView("#LF-Systems_Off");
		return true;
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
		if (LF_RefuseOrganizationsOff("buy"))
			return;

		if (!LF_OrgLedgerGate.AdmitCharacter(this, "upgrade"))
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
		// A listen host owns its own character and an Owner RPC to itself has no wire hop, so the
		// page is applied directly there (as LF_CharacterATMComponent.ReplyResult does).
		if (LF_OwnsOrganizationLedger())
		{
			RpcDo_LF_OrganizationLedger(name, fund, rights, processing, lease, members, rank, notice);
			return;
		}

		Rpc(RpcDo_LF_OrganizationLedger, name, fund, rights, processing, lease, members, rank, notice);
	}

	//------------------------------------------------------------------------------------------------
	//! True on the machine that owns this character: the listen host for its own character. A
	//! dedicated server owns no player's character, so it replies over the wire.
	protected bool LF_OwnsOrganizationLedger()
	{
		RplComponent rpl = RplComponent.Cast(FindComponent(RplComponent));
		if (!rpl)
			return false;

		return rpl.IsOwner();
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
