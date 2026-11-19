//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Vehicles", description: "Inventory manager of a car's hidden compartment (#3434). The compartment is a child entity of the car, not part of the car's own inventory tree.")]
class LF_VehicleCompartmentStorageComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marks the storage of a car's hidden compartment. LF_VehicleCompartment finds it by this component.
class LF_VehicleCompartmentStorageComponent : ScriptedInventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Whether this inventory may take from the compartment: its own manager, or the car owner's.
	//! "Open the panel" only hides the row; vanilla's nearby list can still show the storage.
	bool AllowsAccessFrom(InventoryStorageManagerComponent instigatorManager)
	{
		if (!instigatorManager || instigatorManager == this)
			return true;

		return LF_VehicleCompartment.IsOwnerOf(instigatorManager.GetOwner(), GetOwner().GetParent());
	}

	//------------------------------------------------------------------------------------------------
	//! The engine asks this when a client's inventory request moves or removes an item the
	//! compartment holds. Only the car's owner may; everyone else is refused and it is logged.
	override protected bool HandleAccessValidation(InventoryStorageManagerComponent instigatorManager, IEntity item, EInventoryManagerOperation operation)
	{
		if (!Replication.IsServer() || AllowsAccessFrom(instigatorManager))
			return true;

		LF_Debug.Warn(LF_VehicleCompartment.FEATURE, string.Format("compartment kept %1: the taker is not the car's owner", LF_Utils.GetPrefabName(item)));
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A car's hidden compartment (#3434, Beat 2 of #1035): a small extra storage only its owner's
//! "Open the panel" reaches. It is a child entity of the car (LF_VehicleCompartment.et), so the
//! quick search, which walks the car's own inventory tree (LF_ChainSearch.Collect), never lists it.
//! The full search (SearchFull, the detective's option) walks the compartment too and seizes what
//! it finds. Fitting one is the pack's business (Q11): the core only offers Fit and the search.
//! Server only. The goods live in the compartment entity; the saved size (LF_VehicleOwnership
//! compartment level) says whether a car should be fitted.
class LF_VehicleCompartment
{
	static const string FEATURE = "Vehicle";
	static const ResourceName PREFAB = "{4E2A7C91B3D58F06}Prefabs/Vehicles/LF_VehicleCompartment.et";

	//------------------------------------------------------------------------------------------------
	//! The car's compartment entity, or null for a stock car.
	static IEntity Find(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		IEntity child = vehicle.GetChildren();
		while (child)
		{
			if (child.FindComponent(LF_VehicleCompartmentStorageComponent))
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fits the car with a compartment. Idempotent: a fitted car keeps the one it has.
	//! \return The compartment entity; null when not the server or the prefab does not spawn.
	static IEntity Fit(IEntity vehicle)
	{
		if (!Replication.IsServer() || !vehicle)
			return null;

		IEntity existing = Find(vehicle);
		if (existing)
			return existing;

		IEntity panel = LF_Utils.SpawnEntityPrefab(PREFAB, vehicle.GetOrigin());
		if (!panel)
		{
			LF_Debug.Error(FEATURE, "compartment not fitted: the prefab did not spawn");
			return null;
		}

		vehicle.AddChild(panel, -1, EAddChildFlags.AUTO_TRANSFORM);
		LF_Debug.Info(FEATURE, string.Format("compartment fitted to %1", vehicle));
		return panel;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the user is the car's owner, by the replicated owner key. A display gate for the
	//! "Open the panel" row; empty keys never match.
	static bool IsOwnerOf(IEntity user, IEntity vehicle)
	{
		if (!user || !vehicle)
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return false;

		string owner = vehicleLock.GetOwnerUid();
		return !owner.IsEmpty() && owner == LF_Utils.GetPlayerUID(user);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the full search of a car (the detective's option). The normal storage is searched as
	//! the quick search does (LF_ChainSearch.SearchVehicle: seizure, suspects, offence), then the
	//! compartment is rolled on its own load; a find deletes its goods and charges the same
	//! suspects. A compartment's goods are seized once: they are gone afterwards.
	//! \return Units seized in all.
	static int SearchFull(IEntity vehicle, float roll)
	{
		if (!vehicle)
			return 0;

		int seized = LF_ChainSearch.SearchVehicle(vehicle, roll);
		IEntity panel = Find(vehicle);
		if (!panel)
			return seized;

		int units;
		int load;
		array<IEntity> goods = {};
		LF_ChainSearch.Collect(panel, goods, units, load);
		if (goods.IsEmpty() || !LF_ChainSearch.Finds(load, roll))
		{
			LF_Debug.Info(FEATURE, string.Format("full search found nothing in the compartment (%1 flagged unit(s), roll %2)", units, roll));
			return seized;
		}

		array<IEntity> suspects = {};
		LF_ChainSearch.SuspectsOf(vehicle, suspects);
		foreach (IEntity item : goods)
		{
			if (item)
				SCR_EntityHelper.DeleteEntityAndChildren(item);
		}

		foreach (IEntity suspect : suspects)
		{
			LF_WantedReports.Report(LF_Utils.GetPlayerUID(LF_PlayerInteraction.PlayerIdOf(suspect)), suspect, LF_WantedReports.OFFENCE_CHAIN_GOODS);
		}

		LF_Debug.Info(FEATURE, string.Format("full search seized %1 flagged unit(s) from the compartment; %2 suspect(s) charged", units, suspects.Count()));
		return seized + units;
	}
}
