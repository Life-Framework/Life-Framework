class EL_RobWeaponAction : ScriptedUserAction
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Weapon prefab to steal", "et")]
	protected ResourceName m_WeaponPrefab;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
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

		// Spawn weapon in inventory
		SCR_InventoryStorageManagerComponent inventoryManager = EL_Component<SCR_InventoryStorageManagerComponent>.Find(pUserEntity);
		if (inventoryManager && EL_InventoryUtils.AddItem(inventoryManager, m_WeaponPrefab))
		{
			EL_Utils.Notify("#EL-Weapon_Stolen", "#EL-Robbery_Successful", 3.0);

			// Increase wanted level
			EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
			if (account)
			{
				account.IncreaseWantedLevel(2); // Higher for weapons
				EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

				// Alert police
				AlertPolice("#EL-Police_Alert_Weapon", pUserEntity.GetOrigin());
			}
		}
		else
		{
			EL_Utils.Notify("#EL-Inventory_Full", "#EL-Robbery_Failed", 3.0);
		}
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
					PlayerController pc = EL_PlayerControllerManagerCompat.GetInstance().GetPlayerController(playerId);
					if (pc)
					{
						string playerUid = EL_Utils.GetPlayerUID(pc.GetControlledEntity());
						EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account && account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty())
						{
							count++;
						}
					}
				}
			}
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void AlertPolice(string message, vector position)
	{
		// Send notification to all on-duty police
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
						IEntity entity = pc.GetControlledEntity();
						if (entity)
						{
							string playerUid = EL_Utils.GetPlayerUID(entity);
							EL_PlayerAccount account = accountManager.GetAccount(playerUid);
							if (account && account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty())
							{
								// Send police alert notification using static method
								EL_NotificationManagerComponent.NotifyPlayer(
									playerId, 
									"#EL-PoliceAlert_Title", 
									message, 
									10.0, 
									EL_ENotificationType.POLICE_ALERT
								);
							}
						}
					}
				}
			}
		}
	}
};