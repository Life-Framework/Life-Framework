//------------------------------------------------------------------------------------------------
//! Inventory name of a house or vehicle key, read per item (#2470).
//!
//! WHY. Every key of a prefab shares one UIInfo, so writing the bound name into it (the old
//! UpdateDisplayName) gave every key the last name written, and only on the server. The inventory
//! calls GetInventoryItemName(item) on the client with the item in hand: this reads that item's own
//! replicated identifier (LF_KeyComponent.m_sHouseIdentifier / LF_VehicleKeyComponent.m_sVehicleIdentifier,
//! both RplProp, so a late joiner receives them with the item). Nothing is written back to the UIInfo.
//! An unbound key falls back to the prefab name.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_KeyItemUIInfo : SCR_InventoryUIInfo
{
	//------------------------------------------------------------------------------------------------
	override string GetInventoryItemName(InventoryItemComponent item)
	{
		if (!item || !item.GetOwner())
			return super.GetInventoryItemName(item);

		IEntity owner = item.GetOwner();
		LF_KeyComponent houseKey = LF_Component<LF_KeyComponent>.Find(owner);
		if (houseKey && !houseKey.GetHouseIdentifier().IsEmpty())
			return WidgetManager.Translate("#LF-ItemName_HouseKeyOf", houseKey.GetHouseIdentifier());

		LF_VehicleKeyComponent vehicleKey = LF_Component<LF_VehicleKeyComponent>.Find(owner);
		if (vehicleKey && !vehicleKey.GetVehicleIdentifier().IsEmpty())
			return WidgetManager.Translate("#LF-ItemName_VehicleKeyOf", vehicleKey.GetVehicleIdentifier());

		return super.GetInventoryItemName(item);
	}
}
