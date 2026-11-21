//------------------------------------------------------------------------------------------------
//! "Search" on a loot container (#3343): a short held action. The server fills the container from its
//! kind's loot table (LF_LootContainerComponent.Search) and tells the searcher what happened; the
//! container then opens through the normal "Open storage" action. The client's prompt is only a hint:
//! the server re-decides every search, so a forged perform cannot refill early.
class LF_LootSearchAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_LootContainerComponent container = LF_Component<LF_LootContainerComponent>.Find(GetOwner());
		if (!container)
			return false;

		return container.CanOfferSearch();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity || !Replication.IsServer())
			return;

		LF_LootContainerComponent container = LF_Component<LF_LootContainerComponent>.Find(pOwnerEntity);
		if (!container)
		{
			LF_Debug.Error(LF_LootContainerComponent.FEATURE, string.Format("search on %1 which has no loot container component", pOwnerEntity.GetName()));
			return;
		}

		LF_ELootSearch result = container.SearchNow();
		Tell(pUserEntity, container, result);
	}

	//------------------------------------------------------------------------------------------------
	//! One toast for the searcher; a refill still on cooldown says how long to wait.
	protected static void Tell(IEntity user, LF_LootContainerComponent container, LF_ELootSearch result)
	{
		if (result == LF_ELootSearch.FILLED)
		{
			LF_Utils.NotifyEntity(user, "#LF-Loot_Found", "", 3.0);
			return;
		}

		if (result == LF_ELootSearch.STILL_STOCKED)
		{
			LF_Utils.NotifyEntity(user, "#LF-Loot_StillStocked", "", 3.0);
			return;
		}

		if (result == LF_ELootSearch.COOLDOWN)
		{
			int minutes = Math.Ceil(container.SecondsUntilRefill(GetGame().GetWorld().GetWorldTime() / 1000) / 60);
			if (minutes < 1)
				minutes = 1;

			LF_Utils.NotifyEntity(user, "#LF-Loot_Cooldown", "", 4.0, minutes.ToString());
		}
	}
}
