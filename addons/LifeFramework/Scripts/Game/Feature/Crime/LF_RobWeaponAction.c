class LF_RobWeaponAction : ScriptedUserAction
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Weapon prefab to steal", "et")]
	protected ResourceName m_WeaponPrefab;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	protected float m_fLastRobTime = LF_RobCooldown.NEVER;

	//------------------------------------------------------------------------------------------------
	//! Hidden while the Robbery system is off (#960).
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_RobAction.SYSTEM);
	}

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
		if (!LF_RobAction.IsFactionAllowedToRob(LF_Utils.GetPlayerUid(user)))
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

		if (CommitWeaponRobbery(LF_Utils.GetPlayerUid(pUserEntity), pUserEntity, m_WeaponPrefab))
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Weapon_Stolen", "#EL-Robbery_Successful", 3.0);
		else
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Inventory_Full", "#EL-Robbery_Failed", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of PerformAction. Puts the weapon in the user's inventory. When that works and
	//! an account resolves for playerUid, raises wanted by the catalog's weapon theft amount and alerts police.
	//! Nothing is paid as cash and the bank is never touched.
	//! \return True when the weapon was delivered.
	static bool CommitWeaponRobbery(string playerUid, IEntity user, ResourceName weaponPrefab)
	{
		if (LF_Systems.Refuses(LF_RobAction.SYSTEM, "weapon robbery"))
			return false;

		SCR_InventoryStorageManagerComponent inventoryManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventoryManager || !LF_InventoryUtils.AddItem(inventoryManager, weaponPrefab))
			return false;

		LF_PlayerAccount account = LF_RobAction.FindAccount(playerUid);
		if (account)
		{
			LF_CrimeCatalog.ApplyWanted(account, playerUid, LF_WantedReports.OFFENCE_WEAPON_THEFT);
			LF_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
			LF_Dispatch.Report(playerUid, user, LF_WantedReports.OFFENCE_WEAPON_THEFT, user.GetOrigin());
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetPoliceOnDutyCount()
	{
		int count = 0;
		LF_PlayerControllerManagerCompat playerManager = LF_PlayerControllerManagerCompat.GetInstance();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayerIds(playerIds);
			LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
			if (accountManager)
			{
				foreach (int playerId : playerIds)
				{
					PlayerController pc = LF_PlayerControllerManagerCompat.GetInstance().GetPlayerController(playerId);
					if (pc)
					{
						string playerUid = LF_Utils.GetPlayerUID(pc.GetControlledEntity());
						LF_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account && account.GetFaction() == LF_Faction.POLICE && account.IsOnDuty())
						{
							count++;
						}
					}
				}
			}
		}
		return count;
	}
};