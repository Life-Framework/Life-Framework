class EL_ConfiscateAction : ScriptedUserAction
{
	[Attribute(defvalue: "5.0", uiwidget: UIWidgets.Auto, desc: "Maximum distance to confiscate")]
	protected float m_fMaxDistance;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		EL_PlayerAccount account = GetPlayerAccount(user);
		if (!account || account.GetFaction() != EL_Faction.POLICE || !account.IsOnDuty())
		{
			SetCannotPerformReason("Only on-duty police can confiscate");
			return false;
		}

		// Check if there's a wanted player nearby
		IEntity target = GetNearestWantedPlayer(user);
		if (!target)
		{
			SetCannotPerformReason("No wanted player nearby");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		IEntity target = GetNearestWantedPlayer(pUserEntity);
		if (!target)
		{
			EL_Debug.Warn("Police", "confiscate failed: no wanted player in range");
			return;
		}

		// Confiscate all weapons
		SCR_InventoryStorageManagerComponent targetInventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(target);
		if (!targetInventory)
		{
			EL_Debug.Warn("Police", "confiscate failed: target has no inventory");
			return;
		}

		array<IEntity> allItems();
		targetInventory.FindItems(allItems, null); // Get all items
		foreach (IEntity item : allItems)
		{
			if (EL_Component<BaseWeaponComponent>.Find(item))
			{
				EL_InventoryUtils.DropItem(target, item);
				SCR_EntityHelper.DeleteEntityAndChildren(item);
			}
		}
		EL_Utils.Notify("#EL-Police_Action", "#EL-Police_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetNearestWantedPlayer(IEntity user)
	{
		vector userPos = user.GetOrigin();
		float minDist = m_fMaxDistance;
		IEntity nearest = null;

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
						if (entity && entity != user)
						{
							string playerUid = EL_Utils.GetPlayerUID(entity);
							EL_PlayerAccount account = accountManager.GetAccount(playerUid);
							if (account && account.GetWantedLevel() > 0)
							{
								float dist = vector.Distance(userPos, entity.GetOrigin());
								if (dist < minDist)
								{
									minDist = dist;
									nearest = entity;
								}
							}
						}
					}
				}
			}
		}
		return nearest;
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
};