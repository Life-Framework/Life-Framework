//! Faction-specific starter loadout. When a player's account faction has a matching
//! EL_FactionLoadout configured on the spawn logic, its items replace the generic
//! m_aDefaultCharacterItems so the two factions spawn with relevant gear.
[BaseContainerProps()]
class EL_FactionLoadout
{
	[Attribute(defvalue: EL_Faction.CIVILIAN.ToString(), uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EL_Faction))]
	EL_Faction m_eFaction;

	//! Loadout-slot items (clothing, backpack, ...). Top-level entries must use
	//! EStoragePurpose.PURPOSE_LOADOUT_PROXY, matching the generic default items.
	[Attribute()]
	ref array<ref EL_DefaultLoadoutItem> m_aItems;

	//! Items inserted by best-slot search (TryInsertItem with no purpose) rather than into a
	//! specific loadout slot - weapons, magazines, food, drink. Anything that fails to insert is
	//! dropped by the fail-safe.
	[Attribute()]
	ref array<ResourceName> m_aDirectItems;
}