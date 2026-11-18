//------------------------------------------------------------------------------------------------
//! Organization war desk verbs (M7 gang war W3, #2668; record #2666, rules #2667). The server checks
//! behind the desk: a rank holding DECLARE_WAR declares on another organization and pays the fee from
//! the fund; a DECLARE_WAR holder of the target answers inside the window. Both sides need the rules'
//! member minimum. The fee is a sink: it is not refunded when the declaration lapses. The RPCs that
//! reach these verbs are in LF_OrgWarCharacter.c; there is no UI here (W7).
//------------------------------------------------------------------------------------------------
class LF_OrgWarDesk
{
	static const string FEATURE = "OrgWars";

	//! The LF_OnMoneyMoved source of the fee. The event has no account side: the fund is not a
	//! personal cash or bank account, so the ledger skips it; the incident log keeps it under the player
	//! uid it names.
	static const string FEE_SOURCE = "org-war-fee";

	//! Refusal keys of the desk (LF_OrgWars adds its own). "" means accepted.
	static const string REFUSE_NO_CHARACTER = "no_character";
	static const string REFUSE_NO_ORG = "no_org";
	static const string REFUSE_NO_RIGHT = "no_right";
	static const string REFUSE_TOO_FEW = "too_few";
	static const string REFUSE_TARGET_TOO_FEW = "target_too_few";
	static const string REFUSE_FUND = "fund";
	static const string REFUSE_NOT_TARGET = "not_target";
	static const string REFUSE_NOT_SERVER = "not_server";

	//------------------------------------------------------------------------------------------------
	//! Server: the character declares war on targetOrgId under the configured rules, now. playerUid is
	//! the asking player's account uid, the subject of the fee's money event.
	static string Declare(string characterId, string playerUid, string targetOrgId, out string warId)
	{
		return DeclareAt(characterId, playerUid, targetOrgId, System.GetUnixTime(), LF_OrgWarConfig.Rules(), warId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character answers (accepts) warId under the configured rules, now.
	static string Answer(string characterId, string warId)
	{
		return AnswerAt(characterId, warId, System.GetUnixTime(), LF_OrgWarConfig.Rules());
	}

	//------------------------------------------------------------------------------------------------
	//! Declare with an explicit clock and rules (tests). Every check runs before the fund is touched;
	//! the fee is debited, then the record is made, and a record refusal puts the fee back in the same
	//! call, so the fund never pays for a refused declaration. The war table is memory only until W6
	//! persists it: a restart before then keeps the debit and drops the war.
	//! \return "" and the new war id, or a refusal key (logged) with the fund unchanged.
	static string DeclareAt(string characterId, string playerUid, string targetOrgId, int now, notnull LF_OrgWarRules rules, out string warId)
	{
		warId = string.Empty;
		if (!Replication.IsServer())
			return Refused("declare", REFUSE_NOT_SERVER, characterId, targetOrgId);

		if (characterId.IsEmpty())
			return Refused("declare", REFUSE_NO_CHARACTER, characterId, targetOrgId);

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
			return Refused("declare", REFUSE_NO_ORG, characterId, targetOrgId);

		if (!org.Allows(characterId, LF_EOrganizationPermission.DECLARE_WAR))
			return Refused("declare", REFUSE_NO_RIGHT, characterId, targetOrgId);

		if (org.GetId() == targetOrgId)
			return Refused("declare", LF_OrgWars.REFUSE_SELF, characterId, targetOrgId);

		LF_Organization target = LF_OrganizationManager.Get(targetOrgId);
		if (!target)
			return Refused("declare", LF_OrgWars.REFUSE_UNKNOWN_ORG, characterId, targetOrgId);

		if (org.GetMemberCount() < rules.m_iMinMembers)
			return Refused("declare", REFUSE_TOO_FEW, characterId, targetOrgId);

		if (target.GetMemberCount() < rules.m_iMinMembers)
			return Refused("declare", REFUSE_TARGET_TOO_FEW, characterId, targetOrgId);

		int fee = rules.m_iFee;
		if (fee < 0)
			fee = 0;

		if (org.GetFund() < fee)
			return Refused("declare", REFUSE_FUND, characterId, targetOrgId);

		if (fee > 0 && !org.Debit(fee))
			return Refused("declare", REFUSE_FUND, characterId, targetOrgId);

		LF_OrgWarRecord war;
		string refusal = LF_OrgWars.Declare(org.GetId(), targetOrgId, now, rules, war);
		if (!refusal.IsEmpty() || !war)
		{
			if (fee > 0)
				org.Credit(fee);

			if (refusal.IsEmpty())
				refusal = LF_OrgWars.REFUSE_UNKNOWN_WAR;

			// LF_OrgWars logged the reason; this line says the fee went back.
			LF_Debug.Warn(FEATURE, string.Format("declare refused after the fee: %1 (%2 on %3), fee %4 returned, fund %5", refusal, org.GetId(), targetOrgId, fee, org.GetFund()));
			return refusal;
		}

		warId = war.m_sWarId;
		if (fee > 0)
		{
			LF_OnMoneyMoved moved = LF_OnMoneyMoved.Create(playerUid, fee, FEE_SOURCE);
			moved.m_bCredit = false;
			moved.m_iBalanceAfter = org.GetFund();
			LF_EventBus.Publish(moved);
		}

		LF_Debug.Info(FEATURE, string.Format("%1 declared at the desk by %2 for %3: fee %4 from the fund, fund %5", warId, characterId, org.GetId(), fee, org.GetFund()));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Answer with an explicit clock and rules (tests). The answering character's rank in the target
	//! organization holds DECLARE_WAR and both sides still have the member minimum.
	//! \return "" or a refusal key (logged).
	static string AnswerAt(string characterId, string warId, int now, notnull LF_OrgWarRules rules)
	{
		if (!Replication.IsServer())
			return Refused("answer", REFUSE_NOT_SERVER, characterId, warId);

		if (characterId.IsEmpty())
			return Refused("answer", REFUSE_NO_CHARACTER, characterId, warId);

		LF_OrgWarRecord war = LF_OrgWars.Get(warId);
		if (!war)
			return Refused("answer", LF_OrgWars.REFUSE_UNKNOWN_WAR, characterId, warId);

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
			return Refused("answer", REFUSE_NO_ORG, characterId, warId);

		if (org.GetId() != war.m_sTargetOrgId)
			return Refused("answer", REFUSE_NOT_TARGET, characterId, warId);

		if (!org.Allows(characterId, LF_EOrganizationPermission.DECLARE_WAR))
			return Refused("answer", REFUSE_NO_RIGHT, characterId, warId);

		if (org.GetMemberCount() < rules.m_iMinMembers)
			return Refused("answer", REFUSE_TOO_FEW, characterId, warId);

		LF_Organization declarer = LF_OrganizationManager.Get(war.m_sDeclarerOrgId);
		if (!declarer)
			return Refused("answer", LF_OrgWars.REFUSE_UNKNOWN_ORG, characterId, warId);

		if (declarer.GetMemberCount() < rules.m_iMinMembers)
			return Refused("answer", REFUSE_TARGET_TOO_FEW, characterId, warId);

		string refusal = LF_OrgWars.Accept(warId, now, rules);
		if (!refusal.IsEmpty())
			return refusal;

		LF_Debug.Info(FEATURE, string.Format("%1 answered at the desk by %2 for %3", warId, characterId, org.GetId()));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The toast text (a localization key) for a refusal key from the desk or from LF_OrgWars.
	static string RefusalText(string key)
	{
		if (key == REFUSE_NO_ORG)
			return "#LF-OrgWar_RefuseNoOrg";
		if (key == REFUSE_NO_RIGHT)
			return "#LF-OrgWar_RefuseNoRight";
		if (key == REFUSE_TOO_FEW)
			return "#LF-OrgWar_RefuseTooFew";
		if (key == REFUSE_TARGET_TOO_FEW)
			return "#LF-OrgWar_RefuseOtherTooFew";
		if (key == REFUSE_FUND)
			return "#LF-OrgWar_RefuseFund";
		if (key == REFUSE_NOT_TARGET)
			return "#LF-OrgWar_RefuseNotTarget";
		if (key == LF_OrgWars.REFUSE_SELF)
			return "#LF-OrgWar_RefuseSelf";
		if (key == LF_OrgWars.REFUSE_PENDING)
			return "#LF-OrgWar_RefusePending";
		if (key == LF_OrgWars.REFUSE_COOLDOWN)
			return "#LF-OrgWar_RefuseCooldown";
		if (key == LF_OrgWars.REFUSE_EXPIRED)
			return "#LF-OrgWar_RefuseExpired";
		if (key == LF_OrgWars.REFUSE_UNKNOWN_ORG || key == LF_OrgWars.REFUSE_UNKNOWN_WAR || key == LF_OrgWars.REFUSE_NOT_PENDING)
			return "#LF-OrgWar_RefuseGone";

		return "#LF-OrgWar_Refused";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Refused(string what, string key, string who, string subject)
	{
		LF_Debug.Warn(FEATURE, string.Format("desk %1 refused: %2 (%3, %4)", what, key, who, subject));
		return key;
	}
}
