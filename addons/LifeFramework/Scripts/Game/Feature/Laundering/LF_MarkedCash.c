//! Marked notes (#2850, framework #644: money stays physical, there is no dirty balance). Stolen
//! money is its own item, a stack of the `markedcash` slot prefab (a pack swaps the look). It sits in
//! an inventory like cash and is saved like any other item, but it is a different prefab, so
//! LF_MoneyUtils.GetCash, shops, the ATM and hand-overs never count or spend it.
//!
//! Same add/count/take shape as LF_MoneyUtils. A reason is passed in and logged, but nothing is
//! published as LF_OnMoneyMoved: the cash ledger never hears of marked notes.
class LF_MarkedCash
{
	//! The prefab slot (#964) of the marked notes: Configs/PrefabSlots/LF_PrefabSlots.conf.
	static const string SLOT = "markedcash";
	static const string FEATURE = "Laundering";

	//! Tests only: a slot id to read instead of SLOT (empty in play).
	protected static string s_sSlotForTests;
	//! Why the last Add gave nothing, or empty.
	protected static string s_sLastError;

	//------------------------------------------------------------------------------------------------
	//! The marked-notes prefab, or empty when the slot has no usable prefab (LF_PrefabSlots logs it).
	static ResourceName Prefab()
	{
		if (!s_sSlotForTests.IsEmpty())
			return LF_PrefabSlots.Get(s_sSlotForTests);

		return LF_PrefabSlots.Get(SLOT);
	}

	//------------------------------------------------------------------------------------------------
	//! The marked notes the target holds across all storages.
	//! \return the count, 0 with no usable prefab, or -1 when the target has no resolvable storage
	static int Count(InventoryStorageManagerComponent target)
	{
		if (!target)
			return -1;

		ResourceName prefab = Prefab();
		if (prefab.IsEmpty())
			return 0;

		return LF_InventoryUtils.GetAmount(target, prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref Count(InventoryStorageManagerComponent)
	static int Count(IEntity target)
	{
		return Count(LF_InventoryUtils.GetResponsibleStorageManager(target));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts `amount` marked notes in the target's inventory. Never touches cash or the ledger.
	//! \param reason The reason key logged with the change (a LF_Ledger reason, e.g. a robbery's)
	//! \param dropOverflow Notes that do not fit are left at the target's feet and count as added
	//! \return the amount added, or 0 on a faulty call (logged)
	static int Add(InventoryStorageManagerComponent target, int amount, string reason, bool dropOverflow = false)
	{
		s_sLastError = "";
		if (!target || amount <= 0)
			return Fail(string.Format("add of %1 (%2) refused: no inventory or nothing to add", amount, reason));

		ResourceName prefab = Prefab();
		if (prefab.IsEmpty())
			return Fail(string.Format("add of %1 (%2) refused: the '%3' slot has no usable prefab", amount, reason, SLOT));

		int added = LF_InventoryUtils.AddAmount(target, prefab, amount, dropOverflow);
		LF_Debug.Info(FEATURE, string.Format("%1 marked notes added to %2 (%3; asked %4)", added, Who(target.GetOwner()), reason, amount));
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! \ref Add(InventoryStorageManagerComponent, int, string, bool)
	static int Add(IEntity target, int amount, string reason, bool dropOverflow = false)
	{
		return Add(LF_InventoryUtils.GetResponsibleStorageManager(target), amount, reason, dropOverflow);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes up to `amount` marked notes. A take larger than held takes what is there.
	//! \return the amount actually removed
	static int Take(InventoryStorageManagerComponent target, int amount, string reason = "")
	{
		if (!target || amount <= 0)
			return 0;

		ResourceName prefab = Prefab();
		if (prefab.IsEmpty())
			return 0;

		int removed = LF_InventoryUtils.RemoveAmount(target, prefab, amount);
		LF_Debug.Info(FEATURE, string.Format("%1 marked notes taken from %2 (%3; asked %4)", removed, Who(target.GetOwner()), reason, amount));
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! \ref Take(InventoryStorageManagerComponent, int, string)
	static int Take(IEntity target, int amount, string reason = "")
	{
		return Take(LF_InventoryUtils.GetResponsibleStorageManager(target), amount, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Why the last Add gave nothing, or empty after a good one.
	static string GetLastError()
	{
		return s_sLastError;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: read `slot` instead of SLOT; empty restores the real slot.
	static void SetSlotForTests(string slot)
	{
		s_sSlotForTests = slot;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sSlotForTests = "";
		s_sLastError = "";
	}

	//------------------------------------------------------------------------------------------------
	protected static int Fail(string message)
	{
		s_sLastError = message;
		LF_Debug.Error(FEATURE, message);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Who(IEntity owner)
	{
		if (!owner)
			return "nobody";

		string uid = LF_Utils.GetPlayerUID(owner);
		if (uid.IsEmpty())
			return owner.GetID().ToString();

		return uid;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MarkedCash's statics (LF_StaticReset finds it by reflection).
class LF_MarkedCash_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MarkedCash";
	}

	override void Apply()
	{
		LF_MarkedCash.ResetStatics();
	}
}
