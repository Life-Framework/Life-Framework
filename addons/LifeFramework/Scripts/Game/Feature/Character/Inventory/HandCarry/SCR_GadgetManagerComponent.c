modded class SCR_GadgetManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override void SetGadgetMode(IEntity gadget, EGadgetMode targetMode, bool doFocus = false)
	{
		LF_HandInventoryStorageComponent handStorage = LF_Component<LF_HandInventoryStorageComponent>.Find(GetOwner());
		if (handStorage) handStorage.OnGadgetModeSet(gadget, targetMode, doFocus);
		super.SetGadgetMode(gadget, targetMode, doFocus);
	}
}
