//! Faction-specific starter loadout. When a player's account faction has a matching
//! LF_FactionLoadout configured on the spawn logic, its items replace the generic
//! m_aDefaultCharacterItems so the two factions spawn with relevant gear.
[BaseContainerProps()]
class LF_FactionLoadout
{
	[Attribute(defvalue: LF_Faction.CIVILIAN.ToString(), uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LF_Faction))]
	LF_Faction m_eFaction;

	//! Loadout-slot items (clothing, backpack, ...). Top-level entries must use
	//! EStoragePurpose.PURPOSE_LOADOUT_PROXY, matching the generic default items.
	[Attribute()]
	ref array<ref LF_DefaultLoadoutItem> m_aItems;

	//! Items inserted by best-slot search (TryInsertItem with no purpose) rather than into a
	//! specific loadout slot - weapons, magazines, food, drink. Anything that fails to insert is
	//! dropped by the fail-safe.
	[Attribute()]
	ref array<ResourceName> m_aDirectItems;

	//------------------------------------------------------------------------------------------------
	//! The loadout a role spawns with: for a civilian whose catalogue has a `starting_kit` outlet with
	//! offers (#1767), a loadout built from them; otherwise `configured`, unchanged (null stays null,
	//! so the spawn logic falls back to the game mode's default items). Never edits `configured`.
	//! \param[in] configured The loadout the game mode names for the role, or null.
	static LF_FactionLoadout WithStartingKit(LF_FactionLoadout configured, LF_Faction role)
	{
		if (role != LF_Faction.CIVILIAN)
			return configured;

		return WithStartingKitFrom(configured, role, LF_ItemCatalogueConfig.Get());
	}

	//------------------------------------------------------------------------------------------------
	//! WithStartingKit over a given catalogue (tests pass one built in script).
	static LF_FactionLoadout WithStartingKitFrom(LF_FactionLoadout configured, LF_Faction role, LF_ItemCatalogueConfig config)
	{
		if (role != LF_Faction.CIVILIAN)
			return configured;

		array<ResourceName> clothing = {};
		array<ResourceName> direct = {};
		if (!LF_StartingKit.Resolve(config, clothing, direct))
			return configured;

		LF_FactionLoadout kit = new LF_FactionLoadout();
		kit.m_eFaction = role;
		kit.m_aItems = new array<ref LF_DefaultLoadoutItem>();
		kit.m_aDirectItems = new array<ResourceName>();
		foreach (ResourceName prefab : clothing)
		{
			// Script-made, so the attribute defaults are not applied: set what the spawn reads.
			LF_DefaultLoadoutItem item = new LF_DefaultLoadoutItem();
			item.m_rPrefab = prefab;
			item.m_iAmount = 1;
			item.m_ePurpose = EStoragePurpose.PURPOSE_LOADOUT_PROXY;
			kit.m_aItems.Insert(item);
		}

		foreach (ResourceName prefab : direct)
			kit.m_aDirectItems.Insert(prefab);

		LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("civilian spawns with the catalogue starting kit: %1 worn, %2 carried", clothing.Count(), direct.Count()));
		return kit;
	}
}
