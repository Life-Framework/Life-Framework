//------------------------------------------------------------------------------------------------
//! House key item: identifies which house this key opens by carrying that house's identifier
//! (see LF_LockComponent for how identifiers are generated and matched).
//!
//! BINDING. Server only: SetHouseIdentifier is the runtime path (a key-making or purchase flow),
//! m_sDebugIdentifier is the editor path (map maker binds a key to a whole house lineage). The
//! item display name is rewritten to "House Key [<id>]" so players can tell keys apart.
//!
//! STACKING. The HouseKeyItem prefab sets LF_QuantityComponent.m_iMaxQuantity to 1, so keys
//! never combine: CanCombine is prefab-based and would merge keys bound to different houses
//! into one stack.
//!
//! PERSISTENCE. A carried key's identifier is saved with the carried snapshot (LF_StorageSnapshot,
//! the "itemkeys" payload) and bound again when the key is restored (#2410).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Houses", description: "Binds an item to the house it opens.")]
class LF_KeyComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_KeyComponent : ScriptComponent
{
	[Attribute("", desc: "Editor binding: house identifier this key opens. Leave empty to bind at runtime.")]
	protected string m_sDebugIdentifier;

	[RplProp()]
	protected string m_sHouseIdentifier;

	//------------------------------------------------------------------------------------------------
	//! A ScriptComponent gets EOnInit only when it asks for it. Without this an editor-bound house key
	//! (m_sDebugIdentifier) stayed unbound and opened nothing. Same as LF_VehicleKeyComponent.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_sDebugIdentifier.IsEmpty())
			SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Direct field write: the initial RplProp value broadcasts on registration, so BumpMe is
		// not needed here. Same pattern as LF_VehicleKeyComponent.
		if (!m_sDebugIdentifier.IsEmpty() && Replication.IsServer())
			m_sHouseIdentifier = m_sDebugIdentifier;

		if (Replication.IsServer())
			LF_Debug.Log("Houses", string.Format("key bound: identifier=%1", m_sHouseIdentifier));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The identifier of the house this key opens, empty when unbound.
	string GetHouseIdentifier()
	{
		return m_sHouseIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the server binds a key.
	//! \param identifier The house identifier this key opens.
	void SetHouseIdentifier(string identifier)
	{
		if (!Replication.IsServer())
			return;

		if (m_sHouseIdentifier == identifier)
			return;

		m_sHouseIdentifier = identifier;
		Replication.BumpMe();
		UpdateDisplayName();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateDisplayName()
	{
		InventoryItemComponent inventoryItem = LF_Component<InventoryItemComponent>.Find(GetOwner());
		if (!inventoryItem)
			return;

		UIInfo uiInfo = inventoryItem.GetUIInfo();
		if (uiInfo)
			uiInfo.SetName("House Key [" + m_sHouseIdentifier + "]");
	}
}