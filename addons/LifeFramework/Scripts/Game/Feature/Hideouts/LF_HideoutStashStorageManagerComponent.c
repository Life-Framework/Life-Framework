//------------------------------------------------------------------------------------------------
//! Inventory manager of a hideout stash. The items in the stash are real entities in its
//! storage; this manager is the server-side gate on them. Only members of the organization
//! that owns the hideout take an item out (HandleAccessValidation, which the engine runs on a
//! client's inventory request) or put one in (LF_HideoutStashStorageComponent.CanStoreItem).
//! Everyone else is refused and the refusal is logged. The door's lock state does not matter: a breached door does not open the stash.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Hideouts", description: "Inventory manager of a hideout stash. Only members of the owning organization move items in or out.")]
class LF_HideoutStashStorageManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HideoutStashStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
	[Attribute("Roadmap_Hideout", UIWidgets.EditBox, desc: "Hideout id. Members of the organization that owns it use the stash.")]
	protected string m_sHideoutId;

	//------------------------------------------------------------------------------------------------
	string GetHideoutId()
	{
		if (!m_sHideoutId.IsEmpty())
			return m_sHideoutId;

		return LF_Hideout.HIDEOUT_ID;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether this character entity may use the stash.
	bool AllowsUser(IEntity user, bool report)
	{
		return LF_Hideout.AllowsStash(LF_Hideout.CharacterIdOf(user), GetHideoutId(), report);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether another manager (a player's inventory) may take items out of this stash.
	bool AllowsAccessFrom(InventoryStorageManagerComponent instigatorManager)
	{
		if (!instigatorManager || instigatorManager == this)
			return true;

		return AllowsUser(instigatorManager.GetOwner(), true);
	}

	//------------------------------------------------------------------------------------------------
	//! The engine asks this when a client's inventory request moves or removes an item the stash
	//! holds. The client has no organization data, so only the server decides.
	override protected bool HandleAccessValidation(InventoryStorageManagerComponent instigatorManager, IEntity item, EInventoryManagerOperation operation)
	{
		if (!Replication.IsServer() || AllowsAccessFrom(instigatorManager))
			return true;

		LF_Debug.Warn("Hideout", string.Format("stash %1 kept %2: the taker is not a member", GetHideoutId(), LF_Utils.GetPrefabName(item)));
		return false;
	}
}
