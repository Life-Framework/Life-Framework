//------------------------------------------------------------------------------------------------
//! Confiscates the weapons of the nearest wanted player. Offer checks read EL_AccountReplication
//! (account cache on the server, replicated character state on a client); the confiscation
//! itself runs on the server and re-checks the officer against the real account.
class EL_ConfiscateAction : ScriptedUserAction
{
	[Attribute(defvalue: "5.0", uiwidget: UIWidgets.Auto, desc: "Maximum distance to confiscate")]
	protected float m_fMaxDistance;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!EL_AccountStateBits.IsOnDutyPolice(EL_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#EL-Confiscate_OnDutyOnly");
			return false;
		}

		// Check if there's a wanted player nearby
		IEntity target = GetNearestWantedPlayer(user);
		if (!target)
		{
			SetCannotPerformReason("#EL-Confiscate_NoTarget");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!EL_PoliceUtils.IsOnDutyOfficer(EL_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			EL_Debug.Warn("Police", string.Format("confiscate refused: %1 is not an on-duty officer", pUserEntity));
			return;
		}

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

		EL_Debug.Log("Police", string.Format("confiscated weapons from %1", target));
		EL_Utils.NotifyEntity(pUserEntity, "#EL-Police_Action", "#EL-Police_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest player character within range whose (replicated) wanted level is above 0.
	//! Characters are queried by sphere, so a client sees the same set it has streamed in.
	protected IEntity GetNearestWantedPlayer(IEntity user)
	{
		if (!user || !GetGame() || !GetGame().GetWorld())
			return null;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return null;

		vector userPos = user.GetOrigin();
		float minDist = m_fMaxDistance;
		IEntity nearest = null;

		array<IEntity> characters = EL_Utils.GetNearbyCharacters(userPos, m_fMaxDistance);
		foreach (IEntity entity : characters)
		{
			if (!entity || entity == user)
				continue;

			if (playerManager.GetPlayerIdFromControlledEntity(entity) <= 0)
				continue;

			if (EL_AccountStateBits.GetWantedLevel(EL_AccountReplication.GetState(entity)) <= 0)
				continue;

			float dist = vector.Distance(userPos, entity.GetOrigin());
			if (dist < minDist)
			{
				minDist = dist;
				nearest = entity;
			}
		}

		return nearest;
	}
};
