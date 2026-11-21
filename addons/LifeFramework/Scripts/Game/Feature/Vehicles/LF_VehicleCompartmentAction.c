//------------------------------------------------------------------------------------------------
//! "Open the panel" on a car with a hidden compartment (#3434). Shown to the car's owner only (by
//! the replicated owner key, LF_VehicleCompartment.IsOwnerOf); everyone else sees no row, so the
//! compartment is not advertised. Opens the compartment's storage on the owner's own client.
//! The row lands on the vehicle prefab where the pack fits a compartment (Q11), not in the core.
class LF_VehicleCompartmentAction : SCR_InventoryAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		IEntity vehicle = GetOwner();
		if (!LF_VehicleCompartment.Find(vehicle))
			return false;

		return LF_VehicleCompartment.IsOwnerOf(user, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-VehicleCompartment_Open";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected void PerformActionInternal(SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		IEntity panel = LF_VehicleCompartment.Find(pOwnerEntity);
		if (!panel || !LF_VehicleCompartment.IsOwnerOf(pUserEntity, pOwnerEntity))
		{
			LF_Debug.Warn(LF_VehicleCompartment.FEATURE, "open the panel refused: no compartment or not the owner");
			return;
		}

		manager.SetStorageToOpen(panel);
		manager.OpenInventory();
	}
}
