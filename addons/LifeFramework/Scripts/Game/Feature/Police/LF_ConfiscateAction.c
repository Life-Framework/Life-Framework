//------------------------------------------------------------------------------------------------
//! Search: an on-duty officer searches the player in front of them (the action's owner): chain
//! goods on anyone (LF_ChainSearch, #419) and the weapons of a wanted player, as one entry in the
//! player interaction menu (LF_PlayerInteraction). Offer checks read LF_AccountReplication
//! (account cache on the server, replicated character state on a client); the search itself runs
//! on the server and re-checks the officer against the real account and the reach.
class LF_ConfiscateAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlayerInteraction.Offers(LF_EPlayerAction.SEARCH, LF_PlayerInteraction.IsOnDutyPolice(user)) && LF_PlayerInteraction.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#EL-Confiscate_OnDutyOnly");
			return false;
		}

		if (LF_PlayerInteraction.PlayerIdOf(GetOwner()) <= 0)
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

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn("Police", string.Format("confiscate refused: %1 is not an on-duty officer", pUserEntity));
			return;
		}

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(pUserEntity, pOwnerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Warn("Police", "confiscate refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			return;
		}

		// Any player can be searched at a stop for chain goods (#419); the weapon and scanner
		// confiscation below stays for a wanted suspect.
		// A repeat inside the cooldown rolls nothing and reads clean (#472).
		int goodsSeized = 0;
		if (LF_ChainSearch.TryBeginSearch(LF_ChainSearch.PersonKey(pOwnerEntity), GetGame().GetWorld().GetWorldTime()))
			goodsSeized = LF_ChainSearch.SearchPerson(pOwnerEntity, Math.RandomFloat01());
		LF_PlayerAccount suspect = LF_PoliceUtils.GetAccountForEntity(pOwnerEntity);
		if (!suspect || suspect.GetWantedLevel() <= 0)
		{
			if (goodsSeized > 0)
				LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Found", "#EL-Police_Title", 3.0);
			else
				LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Clean", "#EL-Police_Title", 3.0);

			return;
		}

		SCR_InventoryStorageManagerComponent targetInventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(pOwnerEntity);
		if (!targetInventory)
		{
			LF_Debug.Warn("Police", "confiscate failed: target has no inventory");
			return;
		}

		// Collect first, then delete: deleting while walking the live list skips items.
		array<IEntity> allItems();
		targetInventory.FindItems(allItems, null);
		array<IEntity> weapons();
		foreach (IEntity item : allItems)
		{
			if (item && LF_Component<BaseWeaponComponent>.Find(item))
				weapons.Insert(item);
		}

		foreach (IEntity weapon : weapons)
		{
			LF_InventoryUtils.DropItem(pOwnerEntity, weapon);
			SCR_EntityHelper.DeleteEntityAndChildren(weapon);
		}

		LF_Debug.Log("Police", string.Format("confiscated %1 weapon(s) from %2", weapons.Count(), pOwnerEntity));

		// A scanner is contraband (#253): the search takes it and files the offence once.
		array<LF_ScannerComponent> scanners = {};
		LF_Scanner.FindCarried(pOwnerEntity, scanners);
		array<string> offences = {};
		LF_ScannerLaw.SearchOffences(scanners.Count(), LF_ServiceCallsConfig.ScannerIllegal(), offences);
		if (!offences.IsEmpty())
		{
			foreach (LF_ScannerComponent scanner : scanners)
			{
				IEntity scannerItem = scanner.GetOwner();
				LF_InventoryUtils.DropItem(pOwnerEntity, scannerItem);
				SCR_EntityHelper.DeleteEntityAndChildren(scannerItem);
			}

			string suspectUid = LF_Utils.GetPlayerUID(LF_PlayerInteraction.PlayerIdOf(pOwnerEntity));
			foreach (string offence : offences)
			{
				LF_WantedReports.Report(suspectUid, pOwnerEntity, offence);
			}

			LF_Debug.Info("Police", string.Format("confiscated %1 scanner(s) from %2: offence filed", scanners.Count(), suspectUid));
		}
		LF_Utils.NotifyEntity(pUserEntity, "#EL-Police_Action", "#EL-Police_Title", 3.0);
		if (goodsSeized > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Found", "#EL-Police_Title", 3.0);
	}
};
