//------------------------------------------------------------------------------------------------
//! Client->server asks for the organization war desk (#2668, M7 gang war W3). Same guard stack as the
//! hideout lease ask (LF_OrganizationCharacter.c): the Organizations systems switch, the PURCHASE rate
//! bucket, then the desk range. The server verbs are LF_OrgWarDesk; every refusal toasts the player and
//! LF_OrgWarDesk logs it with LF_Debug.Warn. On a listen host Rpc() runs the Server handler at once, so
//! the asks have no host branch (docs/architecture/rpc-routing.md).
//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Client: declare war on an organization by id.
	void LF_AskOrgWarDeclare(string targetOrgId)
	{
		Rpc(RpcAsk_LF_OrgWarDeclare, targetOrgId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: accept a declaration made on this character's organization.
	void LF_AskOrgWarAnswer(string warId)
	{
		Rpc(RpcAsk_LF_OrgWarAnswer, warId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrgWarDeclare(string targetOrgId)
	{
		if (!LF_PassOrgWarGuards("war declare"))
			return;

		string warId;
		string refusal = LF_OrgWarDesk.Declare(LF_OrganizationPlayer.CharacterId(this), LF_Utils.GetPlayerUid(this), targetOrgId, warId);
		LF_ToastOrgWar(refusal, "#LF-OrgWar_Declared");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_OrgWarAnswer(string warId)
	{
		if (!LF_PassOrgWarGuards("war answer"))
			return;

		string refusal = LF_OrgWarDesk.Answer(LF_OrganizationPlayer.CharacterId(this), warId);
		LF_ToastOrgWar(refusal, "#LF-OrgWar_Accepted");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: systems switch, rate bucket, desk range. Each guard logs and tells the player itself.
	//! \return true when the ask may reach the desk verbs.
	//! The checks are the lease ask's, called through their statics: this file compiles before
	//! LF_OrganizationCharacter.c in the modded chain, so its protected helpers are not visible here.
	protected bool LF_PassOrgWarGuards(string request)
	{
		int playerId = LF_PlayerInteraction.PlayerIdOf(this);
		if (LF_Systems.Refuses(LF_OrgRules.SYSTEM, "organization " + request))
		{
			if (playerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-OrgWar_Title", "#LF-Systems_Off", 4.0, LF_ENotificationType.WARNING);

			return false;
		}

		if (!LF_OrgLedgerGate.AdmitCharacter(this, request))
			return false;

		if (!LF_OrganizationPlayer.IsAtDesk(GetOrigin()))
		{
			LF_Debug.Warn(LF_OrgWarDesk.FEATURE, string.Format("%1 refused: not at an organization desk (uid %2)", request, LF_Utils.GetPlayerUid(this)));
			if (playerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-OrgWar_Title", "#EL-Org_TooFar", 3.0, LF_ENotificationType.WARNING);

			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the outcome toast. "" is the success text; anything else is the refusal's text.
	protected void LF_ToastOrgWar(string refusal, string success)
	{
		int playerId = LF_PlayerInteraction.PlayerIdOf(this);
		if (playerId <= 0)
			return;

		if (refusal.IsEmpty())
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-OrgWar_Title", success, 5.0, LF_ENotificationType.SUCCESS);
			return;
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-OrgWar_Title", LF_OrgWarDesk.RefusalText(refusal), 4.0, LF_ENotificationType.WARNING);
	}
}
