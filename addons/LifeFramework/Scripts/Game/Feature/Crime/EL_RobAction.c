class EL_RobAction : ScriptedUserAction
{
	[Attribute(defvalue: "100", uiwidget: UIWidgets.Auto, desc: "Amount of money to steal")]
	protected int m_iMoneyAmount;

	[Attribute(defvalue: "10", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	protected float m_fLastRobTime;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		float currentTime = GetGame().GetWorld().GetWorldTime();
		if (currentTime - m_fLastRobTime < m_fCooldown)
		{
			SetCannotPerformReason("Cooldown active");
			return false;
		}

		// Only civilians can rob
		EL_PlayerAccount account = GetPlayerAccount(user);
		if (account && account.GetFaction() != EL_Faction.CIVILIAN)
		{
			SetCannotPerformReason("Only civilians can rob");
			return false;
		}

		// Check minimum police on duty
		int policeOnDuty = GetPoliceOnDutyCount();
		if (policeOnDuty < m_iMinPoliceOnDuty)
		{
			SetCannotPerformReason(string.Format("Need at least %1 police on duty", m_iMinPoliceOnDuty));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		m_fLastRobTime = GetGame().GetWorld().GetWorldTime();

		// Give money to player
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		if (atmManager)
		{
			string playerUid = EL_Utils.GetPlayerUid(pUserEntity);
			atmManager.Deposit(playerUid, m_iMoneyAmount);

			EL_Utils.Notify(string.Format("#EL-Stole_Money", m_iMoneyAmount), "Robbery Successful", 3.0);

			// Increase wanted level
			EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
			if (account)
			{
				account.IncreaseWantedLevel(1);
				EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

				// Alert police
				EL_Utils.AlertPolice("A robbery has occurred!", pUserEntity.GetOrigin());
			}
		}

		// TODO: Alert police
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount GetPlayerAccount(IEntity user)
	{
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			string playerUid = EL_Utils.GetPlayerUid(user);
			return accountManager.GetAccount(playerUid);
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetPoliceOnDutyCount()
	{
		int count = 0;
		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayerIds(playerIds);
			
			EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
			if (accountManager)
			{
				foreach (int playerId : playerIds)
				{
					PlayerController pc = playerManager.GetPlayerController(playerId);
					if (pc)
					{
						string playerUid = EL_Utils.GetPlayerUID(pc.GetControlledEntity());
						EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account && account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty())
						{
							count++;
							// Send alert (global notify; per-player parameter omitted)
							EL_Utils.Notify("#EL-Police_Alert_Robbery", "Police Alert", 5.0);
						}
					}
				}
			}
		}
		return count;
	}
};