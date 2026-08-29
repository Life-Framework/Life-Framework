//------------------------------------------------------------------------------------------------
//! Vehicle key item: identifies which vehicle this key opens by carrying that vehicle's
//! identifier (see EL_VehicleLockComponent for how identifiers are generated and matched).
//!
//! BINDING. Server only: BindToVehicleIdentifier / SetVehicleIdentifier are the runtime path
//! (a key-making or purchase flow), m_sDebugIdentifier is the editor path (map maker binds a key
//! to a whole prefab lineage). The item display name is rewritten to "Vehicle Key [<id>]" so
//! players can tell keys apart.
//!
//! PERSISTENCE GAP. The bound identifier is in-session state on the item. The item itself only
//! survives a restart when it is a persisted entity (SelfSpawn), which vehicles and their keys
//! are not. A key from before a restart is gone; the respawned vehicle carries a fresh
//! identifier anyway, so old keys could not match it regardless.
//!
//! STACKING. The key prefab sets EL_QuantityComponent.m_iMaxQuantity to 1, so keys never combine:
//! CanCombine is prefab-based and would merge keys bound to different vehicles into one stack.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/VehicleLock", description: "Binds an item to the vehicle it opens.")]
class EL_VehicleKeyComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleKeyComponent : ScriptComponent
{
	[Attribute("", desc: "Editor binding: vehicle identifier this key opens. Leave empty to bind at runtime.")]
	protected string m_sDebugIdentifier;

	[RplProp()]
	protected string m_sVehicleIdentifier;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Direct field write: the initial RplProp value broadcasts on registration, so BumpMe is
		// not needed here. Same pattern as EL_LicensePlateManagerComponent.
		if (!m_sDebugIdentifier.IsEmpty() && Replication.IsServer())
			m_sVehicleIdentifier = m_sDebugIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The identifier of the vehicle this key opens, empty when unbound.
	string GetVehicleIdentifier()
	{
		return m_sVehicleIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! Binds this key to the vehicle carrying the given identifier.
	//! \param identifier The vehicle identifier from EL_VehicleLockComponent.GetVehicleIdentifier.
	void BindToVehicleIdentifier(string identifier)
	{
		SetVehicleIdentifier(identifier);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the server binds a key.
	//! \param identifier The vehicle identifier this key opens.
	void SetVehicleIdentifier(string identifier)
	{
		if (!Replication.IsServer())
			return;

		if (m_sVehicleIdentifier == identifier)
			return;

		m_sVehicleIdentifier = identifier;
		Replication.BumpMe();
		UpdateDisplayName();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateDisplayName()
	{
		InventoryItemComponent inventoryItem = EL_Component<InventoryItemComponent>.Find(GetOwner());
		if (!inventoryItem)
			return;

		UIInfo uiInfo = inventoryItem.GetUIInfo();
		if (uiInfo)
			uiInfo.SetName("Vehicle Key [" + m_sVehicleIdentifier + "]");
	}
}