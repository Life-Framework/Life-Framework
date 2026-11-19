//------------------------------------------------------------------------------------------------
//! The hauling bypass (#3265 review): a log must ride the shoulder or a truck bed, never a backpack or a
//! pocket. The F-key pick-up of a log is LF_ShoulderHoistAction (hands only); this closes the inventory
//! screen, which asks `InsertItem` to drop a ground item into a pack. An item whose LF_ShoulderCarryComponent
//! is a hauling load is only inserted into the hand-carry storage; anything else is refused and logged.
//! Scripted inserts (a shop or a sawmill chain handing out goods) call `TryInsertItem...` and are unchanged.
//------------------------------------------------------------------------------------------------
class LF_CargoHaulingGate
{
	//------------------------------------------------------------------------------------------------
	//! True when `item` is a hauling load and `target` is not the hand-carry storage. A null target means "wherever
	//! the inventory finds room", which is a pack, so it is refused too.
	static bool Refuses(IEntity item, BaseInventoryStorageComponent target)
	{
		if (!item || !LF_Component<LF_ShoulderCarryComponent>.Find(item))
			return false;

		return LF_HandInventoryStorageComponent.Cast(target) == null;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override void InsertItem(IEntity pItem, BaseInventoryStorageComponent pStorageTo = null, BaseInventoryStorageComponent pStorageFrom = null, SCR_InvCallBack cb = null)
	{
		if (LF_CargoHaulingGate.Refuses(pItem, pStorageTo))
		{
			LF_Debug.Warn(LF_CargoBed.FEATURE, "a hauling load cannot go into a pack or a pocket: shoulder or a truck bed only");
			return;
		}

		super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
	}
}
