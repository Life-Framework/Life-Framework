//------------------------------------------------------------------------------------------------
//! Cash boundary for an organization fund. MoneyStack leaves the character and
//! the fund is credited, or the fund is debited and MoneyStack is paid out.
//! A short removal is returned. A short payout puts the fund back. The personal
//! ATM balance is never read or written here.
//------------------------------------------------------------------------------------------------
class EL_OrganizationPlayer
{
	//------------------------------------------------------------------------------------------------
	static string CharacterId(IEntity user)
	{
		if (!user)
		{
			EL_Debug.Error("Organizations", "rejected: no player");
			return "";
		}

		string uid = EL_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Organizations", "rejected: player has no account id");
			return "";
		}

		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetAccount(uid);
		if (!account)
		{
			EL_Debug.Error("Organizations", string.Format("rejected: no account for %1", uid));
			return "";
		}

		EL_PlayerCharacter character = account.GetActiveCharacter();
		if (!character || character.GetId().IsEmpty())
		{
			EL_Debug.Error("Organizations", string.Format("rejected: %1 has no active character", uid));
			return "";
		}

		return character.GetId();
	}

	//------------------------------------------------------------------------------------------------
	static bool Create(IEntity user, string name)
	{
		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		return EL_OrganizationManager.Create(characterId, name) != null;
	}

	//------------------------------------------------------------------------------------------------
	static bool DepositCash(IEntity user, int amount)
	{
		if (!user)
		{
			EL_Debug.Error("Organizations", "deposit cash refused: no player");
			return false;
		}

		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		EL_Organization org = EL_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			EL_Debug.Error("Organizations", "deposit cash refused: no organization");
			return false;
		}

		if (!EL_OrganizationManager.IsValidAmount(amount))
		{
			EL_Debug.Error("Organizations", string.Format("deposit cash refused: invalid amount %1", amount));
			return false;
		}

		int removed = EL_MoneyUtils.RemoveCash(user, amount);
		if (removed != amount)
		{
			if (removed > 0)
				EL_MoneyUtils.AddCash(user, removed);

			EL_Debug.Error("Organizations", string.Format("deposit cash refused: removed %1 of %2", removed, amount));
			return false;
		}

		if (EL_OrganizationManager.Deposit(org.GetId(), characterId, amount))
			return true;

		EL_MoneyUtils.AddCash(user, amount);
		EL_Debug.Error("Organizations", "deposit cash restored: fund credit failed");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool WithdrawCash(IEntity user, int amount)
	{
		if (!user)
		{
			EL_Debug.Error("Organizations", "withdraw cash refused: no player");
			return false;
		}

		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		EL_Organization org = EL_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			EL_Debug.Error("Organizations", "withdraw cash refused: no organization");
			return false;
		}

		if (!EL_OrganizationManager.Withdraw(org.GetId(), characterId, amount))
			return false;

		int given = EL_MoneyUtils.AddCash(user, amount);
		if (given == amount)
			return true;

		EL_OrganizationManager.Deposit(org.GetId(), characterId, amount);
		if (given > 0)
			EL_MoneyUtils.RemoveCash(user, given);

		EL_Debug.Error("Organizations", string.Format("withdraw cash restored: paid %1 of %2", given, amount));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool BuyUpgrade(IEntity user, EL_EOrganizationUpgrade upgrade)
	{
		string characterId = CharacterId(user);
		if (characterId.IsEmpty())
			return false;

		EL_Organization org = EL_OrganizationManager.GetByCharacter(characterId);
		if (!org)
		{
			EL_Debug.Error("Organizations", "upgrade refused: no organization");
			return false;
		}

		return EL_OrganizationManager.BuyUpgrade(org.GetId(), characterId, upgrade);
	}
}
