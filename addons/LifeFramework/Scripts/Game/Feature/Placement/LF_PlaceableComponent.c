//------------------------------------------------------------------------------------------------
//! The placement service's data (#1026, docs/roadmap/building-foundation.md section 4): on a carried
//! item that the player puts into the world (the folded spike strip first). It names what is placed,
//! the box it needs, how it finds its spot, who may place it and how long it stays.
//! LF_PlacementService plans, probes and confirms from it; nothing here runs by itself.
//------------------------------------------------------------------------------------------------

//! How a placeable finds its spot in front of the player.
enum LF_EPlacementSnap
{
	//! On the ground ahead, along the player's facing.
	GROUND,
	//! On the ground ahead, turned straight across the nearest road when there is one (a spike strip).
	ACROSS_ROAD,
	//! On a building floor (#1028).
	FLOOR,
	//! Against a wall face (#1028).
	WALL
}

//! Who may place it, until the per-kind policy from a .conf takes over (#1030).
enum LF_EPlacementWho
{
	ANYONE,
	ON_DUTY_POLICE
}

//! What happens to it after it is placed.
enum LF_EPlacementLifetime
{
	//! Goes by itself (the placed prefab's own timer) and on the world reset.
	TEMPORARY,
	//! Stays until it is picked up (#1032 saves it).
	PERMANENT
}

[ComponentEditorProps(category: "LifeFramework/Placement", description: "Carried item a player places into the world through LF_PlacementService.")]
class LF_PlaceableComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PlaceableComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab put into the world when this item is placed", "et")]
	protected ResourceName m_sPlacedPrefab;

	[Attribute("-0.2 0.1 -1.75", UIWidgets.Coords, "Footprint box, minimum corner, in the placed prefab's frame (x across, y up, z forward). Its bottom is lifted off the surface so the ground it lies on does not count; the ground is sampled at the z ends and the middle")]
	protected vector m_vFootprintMins;

	[Attribute("0.2 0.6 1.75", UIWidgets.Coords, "Footprint box, maximum corner, in the placed prefab's frame")]
	protected vector m_vFootprintMaxs;

	[Attribute("1", UIWidgets.Auto, "Metres between the player and the near edge of the footprint")]
	protected float m_fReach;

	[Attribute("0", UIWidgets.ComboBox, desc: "How the spot is found", enums: ParamEnumArray.FromEnum(LF_EPlacementSnap))]
	protected LF_EPlacementSnap m_eSnap;

	[Attribute("0", UIWidgets.ComboBox, desc: "Who may place it (until the placement policy, #1030)", enums: ParamEnumArray.FromEnum(LF_EPlacementWho))]
	protected LF_EPlacementWho m_eWho;

	[Attribute("0", UIWidgets.ComboBox, desc: "Temporary (goes by itself) or permanent (stays until picked up)", enums: ParamEnumArray.FromEnum(LF_EPlacementLifetime))]
	protected LF_EPlacementLifetime m_eLifetime;

	//------------------------------------------------------------------------------------------------
	ResourceName GetPlacedPrefab()
	{
		return m_sPlacedPrefab;
	}

	//------------------------------------------------------------------------------------------------
	vector GetFootprintMins()
	{
		return m_vFootprintMins;
	}

	//------------------------------------------------------------------------------------------------
	vector GetFootprintMaxs()
	{
		return m_vFootprintMaxs;
	}

	//------------------------------------------------------------------------------------------------
	//! Length of the footprint along the placed prefab's forward axis.
	float GetLength()
	{
		return m_vFootprintMaxs[2] - m_vFootprintMins[2];
	}

	//------------------------------------------------------------------------------------------------
	float GetReach()
	{
		return m_fReach;
	}

	//------------------------------------------------------------------------------------------------
	LF_EPlacementSnap GetSnap()
	{
		return m_eSnap;
	}

	//------------------------------------------------------------------------------------------------
	LF_EPlacementWho GetWho()
	{
		return m_eWho;
	}

	//------------------------------------------------------------------------------------------------
	LF_EPlacementLifetime GetLifetime()
	{
		return m_eLifetime;
	}
}
