//------------------------------------------------------------------------------------------------
//! A police car is not offered to a civilian: the vanilla Get in prompts of a service vehicle show
//! only to an officer on duty (first-2h review C18: a civilian next to a locked police car was offered
//! "Get in" and every try failed silently). A plain locked car keeps its greyed prompt with the vanilla
//! reason, because its owner can unlock it; a service car has no such way in.
//! The server's entry guard (LF_VehicleEntryGuard) is unchanged and still refuses a modified client.
//------------------------------------------------------------------------------------------------
class LF_GetInServiceCar
{
	//------------------------------------------------------------------------------------------------
	//! Pure: whether a Get in prompt shows. \param service The vehicle is a service car (police). \param driver The
	//! user is an officer on duty (the only one a service car opens for).
	static bool Shows(bool service, bool driver)
	{
		if (!service)
			return true;

		return driver;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `user` is offered a seat in the vehicle that owns this compartment entity.
	static bool ShowsFor(IEntity compartmentOwner, IEntity user)
	{
		if (!compartmentOwner || !user)
			return true;

		IEntity vehicle = SCR_EntityHelper.GetMainParent(compartmentOwner, true);
		if (!vehicle)
			return true;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return true;

		return Shows(vehicleLock.IsServiceVehicle(), vehicleLock.IsServiceDriver(user));
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_GetInUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		return LF_GetInServiceCar.ShowsFor(GetOwner(), user);
	}
}
