// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_StartingKit`: RED-OK catalogue/starting-kit (RedProof() skips the kit lookup; fails 'a kit outlet gives a loadout of its own'). Green with `--only LF_Test_StartingKit,LF_Test_FactionLoadoutConfig`.
// tier: LOGIC
//! The catalogue's `starting_kit` outlet (#1767): a civilian's spawn loadout is built from its offers
//! (clothing into loadout slots, the rest carried), and with no such outlet the loadout the game mode
//! names is returned unchanged. Other roles never read the kit.
class LF_Test_StartingKit : LF_Test
{
	static const ResourceName COAT = "{7F1451790F3D75BD}Prefabs/Characters/Uniforms/Jacket_Raincoat_01/Jacket_Raincoat_01_green.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "catalogue/starting-kit";
	}

	//------------------------------------------------------------------------------------------------
	//! A catalogue with a coat and an apple on the kit outlet, the pistol on a counter only.
	protected LF_ItemCatalogueConfig MakeConfig(bool withKit)
	{
		LF_ItemCatalogueConfig config = new LF_ItemCatalogueConfig();
		config.m_aItems = {};
		config.m_aOffers = {};
		config.m_aItems.Insert(LF_CatalogueItem.Make("coat", COAT, "clothing", 60, 30, "legal"));
		config.m_aItems.Insert(LF_CatalogueItem.Make("apple", APPLE, "equipment", 2, 1, "legal"));
		config.m_aItems.Insert(LF_CatalogueItem.Make("pistol_m9", PISTOL, "weapon", 500, 250, "licensed"));
		config.m_aOffers.Insert(LF_OutletOffer.Make("gun_counter", "pistol_m9"));
		if (withKit)
		{
			config.m_aOffers.Insert(LF_OutletOffer.Make(LF_StartingKit.OUTLET, "coat"));
			config.m_aOffers.Insert(LF_OutletOffer.Make(LF_StartingKit.OUTLET, "apple"));
		}

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_FactionLoadout MakeConfigured()
	{
		LF_FactionLoadout configured = new LF_FactionLoadout();
		configured.m_eFaction = LF_Faction.CIVILIAN;
		configured.m_aItems = {};
		configured.m_aDirectItems = {};
		configured.m_aDirectItems.Insert(PISTOL);
		return configured;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FactionLoadout configured = MakeConfigured();

		// A kit outlet with offers: the civilian's loadout is the kit, the game mode's list is gone.
		LF_FactionLoadout kit = configured;
		if (!RedProof())
			kit = LF_FactionLoadout.WithStartingKitFrom(configured, LF_Faction.CIVILIAN, MakeConfig(true));
		ctx.True(kit != configured, "a kit outlet gives a loadout of its own");
		if (kit == configured)
			return;

		ctx.Equal(1, kit.m_aItems.Count(), "the coat is the one worn item");
		ctx.True(kit.m_aItems[0].m_rPrefab == COAT, "the worn item is the catalogue's coat");
		ctx.True(kit.m_aItems[0].m_ePurpose == EStoragePurpose.PURPOSE_LOADOUT_PROXY, "worn items go to loadout slots");
		ctx.Equal(1, kit.m_aDirectItems.Count(), "the apple is the one carried item");
		ctx.True(kit.m_aDirectItems[0] == APPLE, "the carried item is the catalogue's apple");
		ctx.Equal(1, configured.m_aDirectItems.Count(), "the game mode's own loadout is left as it was");
		ctx.True(configured.m_aDirectItems[0] == PISTOL, "and still carries the pistol");

		// A removed offer drops out of the kit.
		LF_ItemCatalogueConfig trimmed = MakeConfig(true);
		trimmed.FindOffer(LF_StartingKit.OUTLET, "apple").m_bRemoved = true;
		array<ResourceName> clothing = {};
		array<ResourceName> direct = {};
		ctx.True(LF_StartingKit.Resolve(trimmed, clothing, direct), "one live offer still makes a kit");
		ctx.Equal(1, clothing.Count(), "the coat stays");
		ctx.Equal(0, direct.Count(), "the removed apple is gone");

		// No kit outlet: the faction loadout list comes back unchanged, and null stays null.
		LF_FactionLoadout plain = LF_FactionLoadout.WithStartingKitFrom(configured, LF_Faction.CIVILIAN, MakeConfig(false));
		ctx.True(plain == configured, "no kit outlet leaves the faction loadout as it is");
		ctx.True(LF_FactionLoadout.WithStartingKitFrom(null, LF_Faction.CIVILIAN, MakeConfig(false)) == null, "no kit and no loadout stays null (the game mode default applies)");
		ctx.True(LF_FactionLoadout.WithStartingKitFrom(configured, LF_Faction.CIVILIAN, null) == configured, "no catalogue leaves the loadout as it is");

		// A kit with no configured civilian loadout still applies.
		LF_FactionLoadout fromNothing = LF_FactionLoadout.WithStartingKitFrom(null, LF_Faction.CIVILIAN, MakeConfig(true));
		ctx.True(fromNothing != null && fromNothing.m_aItems.Count() == 1, "the kit applies where the game mode names no civilian loadout");

		// Another role never reads the kit.
		LF_FactionLoadout police = new LF_FactionLoadout();
		police.m_eFaction = LF_Faction.POLICE;
		ctx.True(LF_FactionLoadout.WithStartingKitFrom(police, LF_Faction.POLICE, MakeConfig(true)) == police, "the police kit is not replaced by the civilian starting kit");
	}
}
