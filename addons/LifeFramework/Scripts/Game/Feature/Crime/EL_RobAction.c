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

		// Pay in cash, never into the bank: money changes hands physically and
		// only the ATM moves cash <-> account. A short payout is logged, never minted.
		int paid = EL_MoneyUtils.AddCash(pUserEntity, m_iMoneyAmount);
		if (paid != m_iMoneyAmount)
		{
			if (paid > 0)
				EL_MoneyUtils.RemoveCash(pUserEntity, paid);
			EL_Debug.Warn("Crime", string.Format("robbery payout short: paid %1 of %2 (player %3)", paid, m_iMoneyAmount, pUserEntity));
		}
		else
		{
			EL_Debug.Log("Crime", string.Format("robbery +%1 cash to player %2", m_iMoneyAmount, pUserEntity));
			EL_Utils.Notify(WidgetManager.Translate("#EL-Stole_Money", m_iMoneyAmount), "#EL-Robbery_Successful", 3.0);
		}

		// A robbery happened either way; the wanted bump and police alert are not
		// contingent on the payout fitting in the robber's pockets.
		EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
		if (account)
		{
			account.IncreaseWantedLevel(1);
			EL_Debug.Log("Crime", string.Format("wanted level increased for %1", pUserEntity));
			EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

			// Alert police
			EL_Utils.AlertPolice("#EL-Police_Alert_Robbery", pUserEntity.GetOrigin());
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
							EL_Utils.Notify("#EL-Police_Alert_Robbery", "#EL-PoliceAlert_Title", 5.0);
						}
					}
				}
			}
		}
		return count;
	}
};