//! Why a short container will not open for a player (#4195).
enum LF_EStashTake
{
	OK,
	NO_PLAYER,
	NO_KEY,
	EMPTY,
	NO_STOCK
}

//------------------------------------------------------------------------------------------------
//! One line of a short container's stock: a prefab and how many of it the container holds when full.
[BaseContainerProps()]
class LF_StashContainerEntry
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item the container holds", "et")]
	ResourceName m_sPrefab;

	[Attribute("1", UIWidgets.EditBox, "How many of the item the container holds when full", "1 100 1")]
	int m_iCount;
}

//------------------------------------------------------------------------------------------------
//! The rules of a short container (crime B3, #4195): a container a tally clerk leaves short on the
//! books. Only a player who carries the clerk's key opens it. The whole stock moves to that player in
//! one take; the container is then empty until the refill timer runs out, and the same stock is back.
//! Times are world seconds passed in by the caller, so the rules carry no clock of their own. The
//! state lives for the session: a restart starts the container full, which is the clerk's next week.
class LF_StashContainerStock
{
	static const string FEATURE = "StashContainer";
	//! Sentinel for "never emptied": the container starts full.
	static const float NEVER = -1;

	protected ResourceName m_sKeyPrefab;
	protected ref array<ref LF_StashContainerEntry> m_aEntries = {};
	protected float m_fRefillSeconds = 2700;
	protected float m_fEmptiedAtSec = NEVER;

	//------------------------------------------------------------------------------------------------
	void SetKeyPrefab(ResourceName keyPrefab)
	{
		m_sKeyPrefab = keyPrefab;
	}

	//------------------------------------------------------------------------------------------------
	void SetRefillSeconds(float seconds)
	{
		if (seconds < 1)
			seconds = 1;

		m_fRefillSeconds = seconds;
	}

	//------------------------------------------------------------------------------------------------
	float GetRefillSeconds()
	{
		return m_fRefillSeconds;
	}

	//------------------------------------------------------------------------------------------------
	void AddEntry(ResourceName prefab, int count)
	{
		if (prefab.IsEmpty() || count <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("stock entry skipped: prefab='%1' count=%2", prefab, count));
			return;
		}

		LF_StashContainerEntry entry = new LF_StashContainerEntry();
		entry.m_sPrefab = prefab;
		entry.m_iCount = count;
		m_aEntries.Insert(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Copy configured lines (a component's attribute list) into the stock, skipping broken ones.
	void AddEntries(array<ref LF_StashContainerEntry> entries)
	{
		if (!entries)
			return;

		foreach (LF_StashContainerEntry entry : entries)
		{
			if (entry)
				AddEntry(entry.m_sPrefab, entry.m_iCount);
		}
	}

	//------------------------------------------------------------------------------------------------
	int CountItems()
	{
		int total;
		foreach (LF_StashContainerEntry entry : m_aEntries)
		{
			total += entry.m_iCount;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! World second at which an emptied container is full again, or NEVER while it is full.
	float RefillAtSec()
	{
		if (m_fEmptiedAtSec < 0)
			return NEVER;

		return m_fEmptiedAtSec + m_fRefillSeconds;
	}

	//------------------------------------------------------------------------------------------------
	bool IsStocked(float nowSec)
	{
		if (m_fEmptiedAtSec < 0)
			return true;

		return nowSec >= m_fEmptiedAtSec + m_fRefillSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the player carries the clerk's key anywhere in their inventory, nested storage included.
	bool HasKey(IEntity player)
	{
		if (!player || m_sKeyPrefab.IsEmpty())
			return false;

		return LF_InventoryUtils.GetAmount(player, m_sKeyPrefab) > 0;
	}

	//------------------------------------------------------------------------------------------------
	LF_EStashTake Check(IEntity player, float nowSec)
	{
		if (!player)
			return LF_EStashTake.NO_PLAYER;

		if (m_aEntries.IsEmpty())
			return LF_EStashTake.NO_STOCK;

		if (!HasKey(player))
			return LF_EStashTake.NO_KEY;

		if (!IsStocked(nowSec))
			return LF_EStashTake.EMPTY;

		return LF_EStashTake.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move the whole stock into the player's inventory and start the refill timer. What does
	//! not fit lands at the player's feet, so a take never loses goods.
	//! \return how many items moved; 0 when the check refuses.
	int Take(IEntity player, float nowSec)
	{
		if (!Replication.IsServer())
			return 0;

		LF_EStashTake check = Check(player, nowSec);
		if (check != LF_EStashTake.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("take refused: %1", typename.EnumToString(LF_EStashTake, check)));
			return 0;
		}

		// Empty the books first: a second take in the same frame finds nothing.
		m_fEmptiedAtSec = nowSec;

		int moved;
		foreach (LF_StashContainerEntry entry : m_aEntries)
		{
			int added = LF_InventoryUtils.AddAmount(player, entry.m_sPrefab, entry.m_iCount, true);
			if (added < entry.m_iCount)
				LF_Debug.Error(FEATURE, string.Format("take short: %1 of %2 %3", added, entry.m_iCount, entry.m_sPrefab));

			moved += added;
		}

		LF_Debug.Info(FEATURE, string.Format("taken: %1 items, refill at %2 s", moved, RefillAtSec()));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! The string key a player reads when the container will not open.
	static string RefusalKey(LF_EStashTake check)
	{
		if (check == LF_EStashTake.NO_KEY)
			return "#LF-StashContainer_NeedKey";

		if (check == LF_EStashTake.EMPTY)
			return "#LF-StashContainer_Empty";

		return "#LF-StashContainer_Closed";
	}
}
