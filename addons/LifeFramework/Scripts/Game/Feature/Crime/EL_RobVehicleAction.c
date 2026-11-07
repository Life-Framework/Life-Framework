class EL_RobVehicleAction : ScriptedUserAction
{
	//! Wanted levels an accepted vehicle robbery adds. The highest of the three rob actions.
	static const int WANTED_INCREASE = 3;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Vehicle prefab to steal", "et")]
	protected ResourceName m_VehiclePrefab;

	[Attribute(defvalue: "300", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
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
		if (!EL_RobAction.IsFactionAllowedToRob(EL_Utils.GetPlayerUid(user)))
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

		if (CommitVehicleRobbery(EL_Utils.GetPlayerUid(pUserEntity), pUserEntity, m_VehiclePrefab))
			EL_Utils.Notify("#EL-Vehicle_Stolen", "#EL-Robbery_Successful", 3.0);
		else
			EL_Utils.Notify("#EL-Spawn_Failed", "#EL-Robbery_Failed", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of PerformAction. Spawns the vehicle 5 m from the user. When that works and an
	//! account resolves for playerUid, raises wanted by WANTED_INCREASE and alerts police.
	//! Nothing is paid as cash and the bank is never touched.
	//! \return The spawned vehicle, or null when nothing spawned.
	static IEntity CommitVehicleRobbery(string playerUid, IEntity user, ResourceName vehiclePrefab)
	{
		if (!user)
		{
			EL_Debug.Error("Crime", "vehicle robbery skipped: no user entity");
			return null;
		}

		vector spawnPos = user.GetOrigin() + Vector(5, 0, 0);
		EntitySpawnParams params = EntitySpawnParams();
		params.Transform[3] = spawnPos;
		IEntity vehicle = GetGame().SpawnEntityPrefab(Resource.Load(vehiclePrefab), null, params);
		if (!vehicle)
			return null;

		EL_PlayerAccount account = EL_RobAction.FindAccount(playerUid);
		if (account)
		{
			account.IncreaseWantedLevel(WANTED_INCREASE);
			EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
			EL_Utils.AlertPolice("#EL-Police_Alert_Vehicle", user.GetOrigin());
		}

		return vehicle;
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
					   PlayerController pc = EL_PlayerControllerManagerCompat.GetInstance().GetPlayerController(playerId); // playerId es int, correcto
					   if (pc)
					   {
						   string playerUid = EL_Utils.GetPlayerUID(pc.GetControlledEntity());
						   EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						   if (account && account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty())
						   {
							   count++;
							   // Send alert (global notify; per-player parameter omitted)
							   EL_Utils.Notify("#EL-Police_Alert_Vehicle", "#EL-PoliceAlert_Title", 5.0);
						}
					}
				}
			}
		}
		return count;
	}
};