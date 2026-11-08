//------------------------------------------------------------------------------------------------
//! Search: an on-duty officer confiscates the weapons of the wanted player in front of them (the
//! action's owner), as one entry in the player interaction menu (LF_PlayerInteraction). Offer
//! checks read EL_AccountReplication (account cache on the server, replicated character state on
//! a client); the confiscation itself runs on the server and re-checks the officer against the
//! real account, the reach, and the suspect's wanted level.
class EL_ConfiscateAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlayerInteraction.Offers(LF_EPlayerAction.SEARCH, LF_PlayerInteraction.IsOnDutyPolice(user)) && LF_PlayerInteraction.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!EL_AccountStateBits.IsOnDutyPolice(EL_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#EL-Confiscate_OnDutyOnly");
			return false;
		}

		if (!IsWanted(GetOwner()))
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

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(pUserEntity, pOwnerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			EL_Debug.Warn("Police", "confiscate refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			return;
		}

		EL_PlayerAccount suspect = EL_PoliceUtils.GetAccountForEntity(pOwnerEntity);
		if (!suspect || suspect.GetWantedLevel() <= 0)
		{
			EL_Debug.Warn("Police", "confiscate refused: the person searched is not wanted");
			return;
		}

		SCR_InventoryStorageManagerComponent targetInventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(pOwnerEntity);
		if (!targetInventory)
		{
			EL_Debug.Warn("Police", "confiscate failed: target has no inventory");
			return;
		}

		// Collect first, then delete: deleting while walking the live list skips items.
		array<IEntity> allItems();
		targetInventory.FindItems(allItems, null);
		array<IEntity> weapons();
		foreach (IEntity item : allItems)
		{
			if (item && EL_Component<BaseWeaponComponent>.Find(item))
				weapons.Insert(item);
		}

		foreach (IEntity weapon : weapons)
		{
			EL_InventoryUtils.DropItem(pOwnerEntity, weapon);
			SCR_EntityHelper.DeleteEntityAndChildren(weapon);
		}

		EL_Debug.Log("Police", string.Format("confiscated %1 weapon(s) from %2", weapons.Count(), pOwnerEntity));
		EL_Utils.NotifyEntity(pUserEntity, "#EL-Police_Action", "#EL-Police_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Replicated wanted level above 0 on a player character.
	protected bool IsWanted(IEntity target)
	{
		if (LF_PlayerInteraction.PlayerIdOf(target) <= 0)
			return false;

		return EL_AccountStateBits.GetWantedLevel(EL_AccountReplication.GetState(target)) > 0;
	}
};
