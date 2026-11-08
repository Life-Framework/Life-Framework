class EL_RobWeaponAction : ScriptedUserAction
{
	//! Wanted levels an accepted weapon robbery adds. Higher than cash, lower than a vehicle.
	static const int WANTED_INCREASE = 2;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Weapon prefab to steal", "et")]
	protected ResourceName m_WeaponPrefab;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	protected float m_fLastRobTime = LF_RobCooldown.NEVER;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		float currentTime = GetGame().GetWorld().GetWorldTime();
		if (LF_RobCooldown.IsActive(currentTime, m_fLastRobTime, m_fCooldown))
		{
			SetCannotPerformReason("#LF-Rob_RecentlyRobbed");
			return false;
		}

		// Only civilians can rob
		if (!EL_RobAction.IsFactionAllowedToRob(EL_Utils.GetPlayerUid(user)))
		{
			SetCannotPerformReason("#EL-Rob_CiviliansOnly");
			return false;
		}

		// Check minimum police on duty
		int policeOnDuty = GetPoliceOnDutyCount();
		if (policeOnDuty < m_iMinPoliceOnDuty)
		{
			SetCannotPerformReason(WidgetManager.Translate("#LF-Rob_NeedPoliceCount", m_iMinPoliceOnDuty));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Recorded on both sides so the performing client's CanBePerformed also sees the cooldown.
		m_fLastRobTime = GetGame().GetWorld().GetWorldTime();

		// The item grant, wanted level, and alert are authority-only.
		if (!Replication.IsServer())
			return;

		if (CommitWeaponRobbery(EL_Utils.GetPlayerUid(pUserEntity), pUserEntity, m_WeaponPrefab))
			EL_Utils.NotifyEntity(pUserEntity, "#EL-Weapon_Stolen", "#EL-Robbery_Successful", 3.0);
		else
			EL_Utils.NotifyEntity(pUserEntity, "#EL-Inventory_Full", "#EL-Robbery_Failed", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of PerformAction. Puts the weapon in the user's inventory. When that works and
	//! an account resolves for playerUid, raises wanted by WANTED_INCREASE and alerts police.
	//! Nothing is paid as cash and the bank is never touched.
	//! \return True when the weapon was delivered.
	static bool CommitWeaponRobbery(string playerUid, IEntity user, ResourceName weaponPrefab)
	{
		SCR_InventoryStorageManagerComponent inventoryManager = EL_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventoryManager || !EL_InventoryUtils.AddItem(inventoryManager, weaponPrefab))
			return false;

		EL_PlayerAccount account = EL_RobAction.FindAccount(playerUid);
		if (account)
		{
			account.IncreaseWantedLevel(WANTED_INCREASE);
			EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
			AlertPolice("#EL-Police_Alert_Weapon", user.GetOrigin());
		}

		return true;
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
	protected static void AlertPolice(string message, vector position)
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