//------------------------------------------------------------------------------------------------
//! What one search of a loot container did (#3343, slice of #1020).
enum LF_ELootSearch
{
	FILLED,       //! The container was empty and unsearched (or its cooldown ran out): it was filled from the table
	STILL_STOCKED, //! It still holds what the last fill left: nothing added, it opens as normal storage
	COOLDOWN,     //! Emptied, but the refill time has not passed: nothing added
	NO_TABLE,     //! No usable loot table for this container's kind (logged): nothing added
	NO_STORAGE    //! The prop has no storage to fill (logged): nothing added
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Loot", description: "A searchable loot container: filled from its kind's loot table on the first search, refilled after the cooldown once emptied (#3343)")]
class LF_LootContainerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A container that names its loot table kind (LF_LootConfig). The first Search fills its storage on the
//! server from the table; afterwards it is normal storage. Once emptied it refills after the table's
//! cooldown. Nothing is saved: after a restart every container starts unsearched. Filling happens in one
//! synchronous server call that stamps the fill time before it spawns anything, so two characters
//! searching at once fill it once. Items are spawned into the storage, never handed to the searcher.
class LF_LootContainerComponent : ScriptComponent
{
	static const string FEATURE = "Loot";

	[Attribute("crate", UIWidgets.EditBox, "Container kind: names the loot table in LF_Loot.conf")]
	protected string m_sKind;

	//! Replicated so a client hides "Search" once the container holds its fill. Server-written.
	[RplProp()]
	protected bool m_bFilled;

	//! Server only: seconds of the last fill on the world clock; negative = never filled this session.
	protected int m_iLastFillSeconds = -1;

	//------------------------------------------------------------------------------------------------
	string GetKind()
	{
		return m_sKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Client hint for the prompt: show "Search" while the container has never been filled or holds nothing.
	//! The server decides for real in Search.
	bool CanOfferSearch()
	{
		if (!m_bFilled)
			return true;

		return IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected BaseInventoryStorageComponent FindStorage()
	{
		return LF_Component<BaseInventoryStorageComponent>.Find(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! True when the container's storage holds no item (a prop with no storage counts as empty).
	bool IsEmpty()
	{
		BaseInventoryStorageComponent storage = FindStorage();
		if (!storage)
			return true;

		array<IEntity> items = {};
		return storage.GetAll(items) < 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: search once at the world clock with a seed taken from it.
	LF_ELootSearch SearchNow()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return LF_ELootSearch.NO_STORAGE;

		int nowMs = world.GetWorldTime();
		return Search(nowMs / 1000, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one search at `nowSeconds` (the caller's clock) rolling from `seed`. Fills when the container
	//! never filled, or is empty and its cooldown has passed; otherwise adds nothing.
	LF_ELootSearch Search(int nowSeconds, int seed)
	{
		if (!Replication.IsServer())
			return LF_ELootSearch.NO_STORAGE;

		LF_LootConfig config = LF_LootConfig.Get();
		LF_LootTableDef table;
		if (config)
			table = config.FindTable(m_sKind);

		if (!table)
		{
			LF_Debug.Error(FEATURE, string.Format("container %1 has no loot table for kind '%2', nothing filled", GetOwner().GetName(), m_sKind));
			return LF_ELootSearch.NO_TABLE;
		}

		BaseInventoryStorageComponent storage = FindStorage();
		InventoryStorageManagerComponent manager = LF_Component<InventoryStorageManagerComponent>.Find(GetOwner());
		if (!storage || !manager)
		{
			LF_Debug.Error(FEATURE, string.Format("container %1 has no storage or storage manager, nothing filled", GetOwner().GetName()));
			return LF_ELootSearch.NO_STORAGE;
		}

		if (m_iLastFillSeconds >= 0)
		{
			if (!IsEmpty())
				return LF_ELootSearch.STILL_STOCKED;

			if (!LF_LootRules.CanRefill(table, m_iLastFillSeconds, nowSeconds))
				return LF_ELootSearch.COOLDOWN;
		}

		array<ref LF_LootDrop> drops = {};
		if (LF_LootRules.Roll(table, seed, drops) < 1)
			return LF_ELootSearch.NO_TABLE;

		// Stamp first: the fill below is one synchronous call, but a later search must never see it half done.
		m_iLastFillSeconds = nowSeconds;
		m_bFilled = true;
		Replication.BumpMe();

		int inserted;
		foreach (LF_LootDrop drop : drops)
		{
			for (int i = 0; i < drop.m_iCount; i++)
			{
				if (!LF_InventoryUtils.AddItem(manager, drop.m_Prefab))
				{
					LF_Debug.Warn(FEATURE, string.Format("container %1 could not take %2 (full?)", GetOwner().GetName(), drop.m_Prefab));
					break;
				}

				inserted++;
			}
		}

		LF_Debug.Info(FEATURE, string.Format("container %1 (%2) filled with %3 items", GetOwner().GetName(), m_sKind, inserted));
		return LF_ELootSearch.FILLED;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until an emptied container may refill; 0 when it may now, never filled, or has no table.
	int SecondsUntilRefill(int nowSeconds)
	{
		LF_LootConfig config = LF_LootConfig.Get();
		if (!config)
			return 0;

		return LF_LootRules.SecondsUntilRefill(config.FindTable(m_sKind), m_iLastFillSeconds, nowSeconds);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Loot", description: "Inventory manager of a loot container: the server fills its storage through it (#3343)")]
class LF_LootStorageManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_LootStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
}
