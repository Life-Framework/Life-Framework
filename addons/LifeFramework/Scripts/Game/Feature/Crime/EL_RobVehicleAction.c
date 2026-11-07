class EL_RobVehicleAction : ScriptedUserAction
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Vehicle prefab to steal", "et")]
	protected ResourceName m_VehiclePrefab;

	[Attribute(defvalue: "300", uiwidget: UIWidgets.Auto, desc: "Cooldown in seconds")]
	protected float m_fCooldown;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.Auto, desc: "Minimum police on duty required")]
	protected int m_iMinPoliceOnDuty;

	protected float m_fLastRobTime = LF_RobCooldown.NEVER;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		float currentTime = GetGame().GetWorld().GetWorldTime();
		if (LF_RobCooldown.IsActive(currentTime, m_fLastRobTime, m_fCooldown))
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
		// Recorded on both sides so the performing client's CanBePerformed also sees the cooldown.
		m_fLastRobTime = GetGame().GetWorld().GetWorldTime();

		// The spawn, wanted level, and alert are authority-only.
		if (!Replication.IsServer())
			return;

		if (m_VehiclePrefab.IsEmpty())
		{
			EL_Debug.Error("Crime", "vehicle robbery failed: no vehicle prefab configured on the action");
			EL_Utils.Notify("#EL-Spawn_Failed", "#EL-Robbery_Failed", 3.0);
			return;
		}

		// Spawn vehicle near player (the helper logs EL_Debug.Error when the prefab does not resolve)
		vector spawnPos = pUserEntity.GetOrigin() + Vector(5, 0, 0);
		IEntity vehicle = EL_Utils.SpawnEntityPrefab(m_VehiclePrefab, spawnPos);
		if (vehicle)
		{
			EL_Utils.Notify("#EL-Vehicle_Stolen", "#EL-Robbery_Successful", 3.0);

			// Increase wanted level
			EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
			if (account)
			{
				account.IncreaseWantedLevel(3); // Highest for vehicles
				EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

				// Alert police
				EL_Utils.AlertPolice("#EL-Police_Alert_Vehicle", pUserEntity.GetOrigin());
			}
		}
		else
		{
			EL_Utils.Notify("#EL-Spawn_Failed", "#EL-Robbery_Failed", 3.0);
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
					   PlayerController pc = EL_PlayerControllerManagerCompat.GetInstance().GetPlayerController(playerId); // playerId es int, correcto
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
};