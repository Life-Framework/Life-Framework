class EL_RobVehicleAction : ScriptedUserAction
{
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

		// Spawn vehicle near player
		vector spawnPos = pUserEntity.GetOrigin() + Vector(5, 0, 0);
		EntitySpawnParams params = EntitySpawnParams();
		params.Transform[3] = spawnPos;
		IEntity vehicle = GetGame().SpawnEntityPrefab(Resource.Load(m_VehiclePrefab), null, params);
		if (vehicle)
		{
			EL_Utils.Notify("#EL-Vehicle_Stolen", "Robbery Successful", 3.0);

			// Increase wanted level
			EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
			if (account)
			{
				account.IncreaseWantedLevel(3); // Highest for vehicles
				EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);

				// Alert police
				EL_Utils.AlertPolice("Vehicle theft alert!", pUserEntity.GetOrigin());
			}
		}
		else
		{
			EL_Utils.Notify("#EL-Spawn_Failed", "Robbery Failed", 3.0);
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
							   // Send alert (global notify; per-player parameter omitted)
							   EL_Utils.Notify("#EL-Police_Alert_Vehicle", "Police Alert", 5.0);
						}
					}
				}
			}
		}
		return count;
	}
};