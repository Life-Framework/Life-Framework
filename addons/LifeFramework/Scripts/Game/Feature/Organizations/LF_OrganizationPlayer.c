//------------------------------------------------------------------------------------------------
//! Cash boundary for an organization fund. MoneyStack leaves the character and
//! the fund is credited, or the fund is debited and MoneyStack is paid out.
//! A short removal is returned. A short payout puts the fund back. The personal
//! ATM balance is never read or written here.
//------------------------------------------------------------------------------------------------
class LF_OrganizationPlayer
{
	//! How far the player may stand from the desk's origin when the server moves fund cash.
	//! The desk action shows at 3 m; the extra 2 m absorbs movement and latency.
	static const float DESK_RANGE = 5.0;

	//------------------------------------------------------------------------------------------------
	//! \param origin World position of the player.
	//! \return true when an organization desk stands within DESK_RANGE of origin.
	static bool IsAtDesk(vector origin)
	{
		return LF_ActionSite.IsNear(origin, DESK_RANGE, "LF_OrganizationDeskAction");
	}

	//------------------------------------------------------------------------------------------------
	static string CharacterId(IEntity user)
	{
		if (!user)
		{
			LF_Debug.Error("Organizations", "rejected: no player");
			return "";
		}

		string uid = LF_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Organizations", "rejected: player has no account id");
			return "";
		}

		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetAccount(uid);
		if (!account)
		{
			LF_Debug.Error("Organizations", string.Format("rejected: no account for %1", uid));
			return "";
		}

		LF_PlayerCharacter character = account.GetActiveCharacter();
		if (!character || character.GetId().IsEmpty())
		{
			LF_Debug.Error("Organizations", string.Format("rejected: %1 has no active character", uid));
			return "";
		}

		return character.GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Founds an organization: the name is checked, then LF_OrgRules.FOUNDING_FEE leaves the
	//! founder's carried MoneyStack. A refused registration gives the fee back.
	//! \return Empty on success, else the localization key the ledger shows.
	static string Found(IEntity user, string name)
	{
		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return "#LF-Org_NoCharacter";

		if (LF_OrganizationManager.GetByCharacter(characterId))
		{
			LF_Debug.Info("Organizations", "found refused: already a member");
			return "#LF-Org_AlreadyMember";
		}

		string refusal = LF_OrgRules.RefuseName(name);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info("Organizations", string.Format("found refused: name '%1' (%2)", name, refusal));
			return refusal;
		}

		if (LF_OrganizationManager.IsNameTaken(name))
		{
			LF_Debug.Info("Organizations", string.Format("found refused: name '%1' is taken", name));
			return "#LF-Org_NameTaken";
		}

		int fee = LF_OrgRules.FOUNDING_FEE;
		int removed = LF_MoneyUtils.RemoveCash(user, fee, "org-fee");
		if (removed != fee)
		{
			if (removed > 0)
				LF_MoneyUtils.AddCash(user, removed, "org-fee-rollback");

			LF_Debug.Info("Organizations", string.Format("found refused: fee %1, carried %2", fee, removed));
			return "#LF-Org_NoFee";
		}

		if (!LF_OrganizationManager.Create(characterId, name))
		{
			LF_MoneyUtils.AddCash(user, fee, "org-fee-rollback");
			LF_Debug.Error("Organizations", "found failed after the fee; fee returned");
			return "#LF-Org_FoundFailed";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! "Name|RankKey" per member, rows joined by ";", founder first. Offline members are listed by the
	//! name on their character record.
	static string MemberLines(LF_Organization org)
	{
		if (!org)
			return string.Empty;

		array<string> ids = {};
		org.GetMemberIds(ids);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		string lines;
		foreach (string memberId : ids)
		{
			string memberName = "?";
			LF_PlayerCharacter character;
			if (accounts)
				character = accounts.FindCharacter(memberId);
			if (character)
				memberName = (character.GetFirstName() + " " + character.GetLastName()).Trim();

			memberName.Replace("|", " ");
			memberName.Replace(";", " ");

			if (!lines.IsEmpty())
				lines += ";";

			lines += memberName + "|" + LF_OrgRules.RankKey(LF_OrgRules.RightsOf(org, memberId));
		}

		return lines;
	}

	//------------------------------------------------------------------------------------------------
	static bool DepositCash(IEntity user, int amount)
	{
		if (!user)
		{
			LF_Debug.Error("Organizations", "deposit cash refused: no player");
			return false;
		}

		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			LF_Debug.Error("Organizations", "deposit cash refused: no organization");
			return false;
		}

		if (!LF_OrganizationManager.IsValidAmount(amount))
		{
			LF_Debug.Error("Organizations", string.Format("deposit cash refused: invalid amount %1", amount));
			return false;
		}

		int removed = LF_MoneyUtils.RemoveCash(user, amount, "org-deposit");
		if (removed != amount)
		{
			if (removed > 0)
				LF_MoneyUtils.AddCash(user, removed, "org-deposit-rollback");

			LF_Debug.Error("Organizations", string.Format("deposit cash refused: removed %1 of %2", removed, amount));
			return false;
		}

		if (LF_OrganizationManager.Deposit(org.GetId(), characterId, amount))
			return true;

		LF_MoneyUtils.AddCash(user, amount, "org-deposit-rollback");
		LF_Debug.Error("Organizations", "deposit cash restored: fund credit failed");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool WithdrawCash(IEntity user, int amount)
	{
		if (!user)
		{
			LF_Debug.Error("Organizations", "withdraw cash refused: no player");
			return false;
		}

		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			LF_Debug.Error("Organizations", "withdraw cash refused: no organization");
			return false;
		}

		if (!LF_OrganizationManager.Withdraw(org.GetId(), characterId, amount))
			return false;

		int given = LF_MoneyUtils.AddCash(user, amount, "org-withdraw");
		if (given == amount)
			return true;

		LF_OrganizationManager.Deposit(org.GetId(), characterId, amount);
		if (given > 0)
			LF_MoneyUtils.RemoveCash(user, given, "org-withdraw-rollback");

		LF_Debug.Error("Organizations", string.Format("withdraw cash restored: paid %1 of %2", given, amount));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool BuyUpgrade(IEntity user, LF_EOrganizationUpgrade upgrade)
	{
		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			LF_Debug.Error("Organizations", "upgrade refused: no organization");
			return false;
		}

		return LF_OrganizationManager.BuyUpgrade(org.GetId(), characterId, upgrade);
	}
}
