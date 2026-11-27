[ComponentEditorProps(category: "LifeFramework/Jobs", description: "Export order crane: where a hauler hands in an export order's goods (#4410)")]
class LF_ExportOrderCraneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The export order crane (#4410). Registers its owner with LF_Places as kind `export_crane`, so the
//! board and the hand-in find it by component, never by name or coordinate. On the server it ticks the
//! book every game minute or so, so a missed deadline closes even when nobody reads the board.
class LF_ExportOrderCraneComponent : ScriptComponent
{
	static const string PLACE_KIND = "export_crane";
	protected static const int TICK_MS = 15000;	// lf-fixed: how often the book closes overdue orders, not a tunable

	[Attribute("dock", UIWidgets.EditBox, "Export order crane: the place id (unique per crane)")]
	protected string m_sPlaceId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		LF_Places.Claim(PLACE_KIND, m_sPlaceId, owner);
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(TickBook, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TickBook);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickBook()
	{
		LF_ExportOrderBook.Tick(LF_ExportOrderBook.CurrentMinute());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` hands in the goods of the order it holds, from a vehicle backed up to `crane` and
	//! from its own pockets. The goods are removed before the order pays; a short count takes nothing.
	//! \param[out] paid The cash handed over, 0 on a refusal.
	//! \return Empty on success, else the refusal as a string key.
	static string HandIn(IEntity user, IEntity crane, out int paid)
	{
		int missing;
		return HandInCounted(user, crane, paid, missing);
	}

	//------------------------------------------------------------------------------------------------
	//! HandIn that also says how far short a refused hand-in was.
	//! \param[out] missing Units still needed when the refusal is a short count (REFUSE_SHORT), else 0.
	static string HandInCounted(IEntity user, IEntity crane, out int paid, out int missing)
	{
		paid = 0;
		missing = 0;
		if (!user || !crane || (Replication.IsRunning() && !Replication.IsServer()))
			return LF_ExportOrderBook.REFUSE_NONE_HELD;

		// Checked before any goods move: a switched-off Jobs system must not take goods it cannot pay for.
		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "export order hand-in"))
			return LF_ExportOrderBook.REFUSE_NONE_HELD;

		string uid = LF_Utils.GetPlayerUid(user);
		LF_ExportOrderBook.Tick(LF_ExportOrderBook.CurrentMinute());
		LF_ExportOrder order = LF_ExportOrderBook.HeldBy(uid);
		if (!order)
		{
			LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("hand-in refused: %1 holds no order", uid));
			return LF_ExportOrderBook.REFUSE_NONE_HELD;
		}

		InventoryStorageManagerComponent vehicleStorage;
		IEntity vehicle = LF_FeedFromVehicle.FindVehicle(crane);
		if (vehicle && !LF_FeedFromVehicle.IsLocked(vehicle))
			vehicleStorage = LF_FeedFromVehicle.StorageOf(vehicle);

		InventoryStorageManagerComponent userStorage = InventoryStorageManagerComponent.Cast(user.FindComponent(InventoryStorageManagerComponent));
		int inVehicle;
		int withUser;
		if (vehicleStorage)
			inVehicle = LF_InventoryUtils.GetAmount(vehicleStorage, order.m_Goods);
		if (userStorage)
			withUser = LF_InventoryUtils.GetAmount(userStorage, order.m_Goods);

		if (inVehicle + withUser < order.m_iCount)
		{
			LF_Debug.Info(LF_ExportOrderBook.FEATURE, string.Format("hand-in refused: %1 brought %2 of %3 for order %4", uid, inVehicle + withUser, order.m_iCount, order.m_iId));
			missing = order.m_iCount - (inVehicle + withUser);
			return LF_ExportOrderBook.REFUSE_SHORT;
		}

		// The goods go first: the vehicle's, then the pockets'. Only what was really removed counts.
		int fromVehicle = LF_Utils.MinInt(inVehicle, order.m_iCount);
		int removed;
		if (fromVehicle > 0)
			removed += LF_InventoryUtils.RemoveAmount(vehicleStorage, order.m_Goods, fromVehicle);
		if (removed < order.m_iCount && userStorage)
			removed += LF_InventoryUtils.RemoveAmount(userStorage, order.m_Goods, order.m_iCount - removed);

		if (removed < order.m_iCount)
		{
			// Counting and removal disagreed: hand back what was taken, pay nothing, keep the order.
			LF_Debug.Error(LF_ExportOrderBook.FEATURE, string.Format("hand-in short after removal: %1 of %2 for order %3 (%4); the goods go back", removed, order.m_iCount, order.m_iId, uid));
			if (removed > 0)
				LF_InventoryUtils.AddAmount(user, order.m_Goods, removed, true);
			return LF_ExportOrderBook.REFUSE_SHORT;
		}

		paid = LF_ExportOrderBook.HandIn(uid, removed);
		if (paid <= 0)
			return LF_ExportOrderBook.REFUSE_SHORT;

		// Overflow drops at the player's feet: the goods are gone, so the cash must land.
		int given = LF_MoneyUtils.AddCash(user, paid, "export-order", "", true);
		if (given != paid)
			LF_Debug.Error(LF_ExportOrderBook.FEATURE, string.Format("export pay short: %1 of %2 for %3", given, paid, uid));
		LF_ExportOrderConfig config = LF_ExportOrderConfig.Get();
		if (config && config.m_iXp > 0)
			LF_JobLevels.AddXp(uid, config.m_sJob, config.m_iXp, "export_order");

		return string.Empty;
	}
}
