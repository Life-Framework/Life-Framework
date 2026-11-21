[ComponentEditorProps(category: "LifeFramework/Bins", description: "Tags a container as a bin: it opens for anyone, is never saved, and the world reset empties it (#3323)")]
class LF_BinComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Tags a container prop as a bin (#3323). A server owner puts it on any prop that carries a vanilla
//! storage component and an open-storage action: the bin opens for anyone, takes items while the server
//! runs, and is a real item sink and a dead drop.
//!
//! A bin is never saved: it has no LF_PersistentStorageComponent and no section names it, so a restart
//! brings it back empty. The world reset (`LF_BinsWorldReset.c`) empties every live bin through
//! `EmptyAll`. Removal goes through the storage manager on the server, never by deleting the prop, so
//! the items are removed once and the bin stands.
class LF_BinComponent : ScriptComponent
{
	protected static ref array<EntityID> s_aBins;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aBins)
			s_aBins = new array<EntityID>();

		s_aBins.Insert(owner.GetID());
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aBins)
			s_aBins.RemoveItem(owner.GetID());

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! How many bins are live in the world.
	static int Count()
	{
		if (!s_aBins)
			return 0;

		return s_aBins.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Fills `into` with every live bin entity (the garbage round, #3325, builds its route from them).
	static int FindAllBins(notnull array<IEntity> into)
	{
		if (!s_aBins || !GetGame() || !GetGame().GetWorld())
			return 0;

		foreach (EntityID id : s_aBins)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (entity)
				into.Insert(entity);
		}

		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The items at the top level of the bin's storage (what a player sees when it opens), without the
	//! contents of a bag inside it.
	array<IEntity> GetItems()
	{
		array<IEntity> items = {};
		BaseInventoryStorageComponent storage = LF_Component<BaseInventoryStorageComponent>.Find(GetOwner());
		if (storage)
			storage.GetAll(items, false);

		return items;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deletes every item left in the bin, through its storage manager. A bag inside goes with
	//! what it holds. \return How many top-level items were removed.
	int Empty()
	{
		if (!Replication.IsServer())
			return 0;

		InventoryStorageManagerComponent manager = LF_Component<InventoryStorageManagerComponent>.Find(GetOwner());
		if (!manager)
		{
			LF_Debug.Error(LF_Bins.FEATURE, string.Format("bin %1 has no inventory storage manager, nothing emptied", GetOwner().GetName()));
			return 0;
		}

		int removed;
		foreach (IEntity item : GetItems())
		{
			if (manager.TryDeleteItem(item))
				removed++;
			else
				LF_Debug.Warn(LF_Bins.FEATURE, string.Format("bin %1 could not delete %2", GetOwner().GetName(), LF_Utils.GetPrefabName(item)));
		}

		if (removed > 0)
			LF_Debug.Info(LF_Bins.FEATURE, string.Format("bin %1 emptied: %2 items", GetOwner().GetName(), removed));

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: empties every live bin. \return How many items were removed in all.
	static int EmptyAll()
	{
		if (!s_aBins || !GetGame() || !GetGame().GetWorld())
			return 0;

		BaseWorld world = GetGame().GetWorld();
		int removed;
		foreach (EntityID id : s_aBins)
		{
			IEntity entity = world.FindEntityByID(id);
			if (!entity)
				continue;

			LF_BinComponent bin = LF_BinComponent.Cast(entity.FindComponent(LF_BinComponent));
			if (bin)
				removed += bin.Empty();
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBins = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BinComponent's statics (LF_StaticReset finds it by reflection).
class LF_BinComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BinComponent";
	}

	override void Apply()
	{
		LF_BinComponent.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_Bins
{
	static const string FEATURE = "Bins";
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Bins", description: "Inventory manager of a bin: the server empties the bin through it (#3323)")]
class LF_BinStorageManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The bin's storage manager. It adds no rule: anyone may put in and take out.
class LF_BinStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
}
