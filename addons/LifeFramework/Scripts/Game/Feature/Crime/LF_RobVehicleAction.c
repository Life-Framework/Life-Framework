class LF_RobVehicleAction : ScriptedUserAction
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Vehicle prefab to steal", "et")]
	protected ResourceName m_VehiclePrefab;

	[Attribute(defvalue: "300", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
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
			SetCannotPerformReason(LF_Terms.Text("#LF-Rob_NeedPoliceCount", m_iMinPoliceOnDuty.ToString()));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// A prompt shown just before Robbery was turned off (#960): no cooldown, and the right words.
		if (!LF_Systems.IsEnabled(LF_RobAction.SYSTEM))
		{
			if (Replication.IsServer() && LF_Systems.Refuses(LF_RobAction.SYSTEM, "vehicle robbery"))
				LF_Utils.NotifyEntity(pUserEntity, "#LF-Systems_Off", "#EL-Robbery_Failed", 3.0);
			return;
		}

		// Recorded on both sides so the performing client's CanBePerformed also sees the cooldown.
		m_fLastRobTime = GetGame().GetWorld().GetWorldTime();

		// The spawn, wanted level, and alert are authority-only.
		if (!Replication.IsServer())
			return;

		if (CommitVehicleRobbery(LF_Utils.GetPlayerUid(pUserEntity), pUserEntity, m_VehiclePrefab))
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Vehicle_Stolen", "#EL-Robbery_Successful", 3.0);
		else
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Spawn_Failed", "#EL-Robbery_Failed", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of PerformAction. Spawns the vehicle 5 m from the user. When that works and an
	//! account resolves for playerUid, raises wanted by the catalog's carjacking amount and alerts police.
	//! Nothing is paid as cash and the bank is never touched.
	//! \return The spawned vehicle, or null when nothing spawned.
	static IEntity CommitVehicleRobbery(string playerUid, IEntity user, ResourceName vehiclePrefab)
	{
		if (LF_Systems.Refuses(LF_RobAction.SYSTEM, "vehicle robbery"))
			return null;

		if (!user)
		{
			LF_Debug.Error("Crime", "vehicle robbery skipped: no user entity");
			return null;
		}

		if (vehiclePrefab.IsEmpty())
		{
			LF_Debug.Error("Crime", "vehicle robbery failed: no vehicle prefab configured on the action");
			return null;
		}

		// The helper logs LF_Debug.Error when the prefab does not resolve.
		IEntity vehicle = LF_Utils.SpawnEntityPrefab(vehiclePrefab, user.GetOrigin() + Vector(5, 0, 0));
		if (!vehicle)
			return null;

		LF_PlayerAccount account = LF_RobAction.FindAccount(playerUid);
		if (account)
		{
			LF_CrimeCatalog.ApplyWanted(account, playerUid, LF_WantedReports.OFFENCE_CARJACKING);
			LF_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
			LF_Dispatch.Report(playerUid, user, LF_WantedReports.OFFENCE_CARJACKING, user.GetOrigin());
		}

		return vehicle;
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
					   PlayerController pc = LF_PlayerControllerManagerCompat.GetInstance().GetPlayerController(playerId); // playerId es int, correcto
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