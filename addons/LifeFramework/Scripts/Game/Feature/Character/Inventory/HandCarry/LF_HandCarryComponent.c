[EntityEditorProps(category: "EveronLife/HandCarry", description: "Items that can only be carried in hands", color: "0 0 255 255")]
class LF_HandCarryComponentClass: SCR_GadgetComponentClass
{
}

class LF_HandCarryComponent : SCR_GadgetComponent
{
	//------------------------------------------------------------------------------------------------
	//! A hand-carried item that leaves a slot lands on the ground; a parcel marks itself there.
	override void OnParentSlotChanged(InventoryStorageSlot oldSlot, InventoryStorageSlot newSlot)
	{
		super.OnParentSlotChanged(oldSlot, newSlot);

		LF_ParcelComponent parcel = LF_Component<LF_ParcelComponent>.Find(GetOwner());
		if (parcel)
			parcel.OnSlotChanged(oldSlot, newSlot);
	}
}
