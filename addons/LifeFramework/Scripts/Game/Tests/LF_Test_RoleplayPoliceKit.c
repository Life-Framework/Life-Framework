// red-proof: 2026-11-22 `toolsi test --no-build --red LF_Test_RoleplayPoliceKit`: `RED-OK spawn/roleplay-duty-kits (fails with the hook on, 6 assertions)` (RedProof reads the CIVILIAN row, which the base has none of, as the unfixed base had no POLICE row).

//! rc 0.2.0 (overnight island smoke): the live game mode GameMode_Roleplay.et (the island and every server built on the
//! core) had no POLICE or MEDIC kit, only the debug child did, so an officer clocking in on the island got the generic
//! civilian items and every cuffing was refused NO_CUFFS. The base prefab now carries both duty kits; the police kit
//! holds the handcuffs, the station car key and a radio, the medic kit a medical kit.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RoleplayPoliceKit : LF_Test
{
	protected static const ResourceName GAME_MODE = "{03913608C59017A3}Prefabs/MP/Modes/Roleplay/GameMode_Roleplay.et";
	protected static const ResourceName CUFFS = "{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et";
	protected static const ResourceName CAR_KEY = "{976BD4B5B482A8D9}Prefabs/Items/Roleplay/LF_PoliceCarKey.et";
	protected static const ResourceName RADIO = "{73950FBA2D7DB5C5}Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et";
	protected static const ResourceName MEDKIT = "{46FD263F0F368A92}Prefabs/Items/Equipment/Kits/MedicalKit_01/MedicalKit_01_base.et";

	override string GetName()
	{
		return "spawn/roleplay-duty-kits";
	}

	//------------------------------------------------------------------------------------------------
	//! The direct items of the base game mode's kit for \p faction, or null when it has none.
	protected array<ResourceName> DirectItems(BaseContainer spawnLogic, LF_Faction faction)
	{
		BaseContainerList kits = spawnLogic.GetObjectArray("m_aFactionCharacterItems");
		if (!kits)
			return null;

		for (int i = 0; i < kits.Count(); i++)
		{
			BaseContainer kit = kits.Get(i);
			int kitFaction;
			if (!kit || !kit.Get("m_eFaction", kitFaction) || kitFaction != faction)
				continue;

			array<ResourceName> items = {};
			kit.Get("m_aDirectItems", items);
			return items;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource resource = BaseContainerTools.LoadContainer(GAME_MODE);
		BaseContainer gameMode;
		if (resource && resource.GetResource())
			gameMode = resource.GetResource().ToBaseContainer();

		ctx.True(gameMode != null, "GameMode_Roleplay.et loads");
		if (!gameMode)
			return;

		BaseContainer spawnLogic;
		BaseContainerList components = gameMode.GetObjectArray("components");
		for (int i = 0; components && i < components.Count(); i++)
		{
			BaseContainer component = components.Get(i);
			if (component && component.GetClassName() == "SCR_RespawnSystemComponent")
				spawnLogic = component.GetObject("m_SpawnLogic");
		}

		ctx.True(spawnLogic != null, "the base game mode has its LF_SpawnLogic");
		if (!spawnLogic)
			return;

		LF_Faction police = LF_Faction.POLICE;
		if (RedProof())
			police = LF_Faction.CIVILIAN;

		array<ResourceName> policeKit = DirectItems(spawnLogic, police);
		ctx.NotNull(policeKit, "the base game mode has a POLICE kit");
		if (policeKit)
		{
			ctx.True(policeKit.Contains(CUFFS), "the POLICE kit carries handcuffs");
			ctx.True(policeKit.Contains(CAR_KEY), "the POLICE kit carries the station car key");
			ctx.True(policeKit.Contains(RADIO), "the POLICE kit carries a radio");
		}

		array<ResourceName> medicKit = DirectItems(spawnLogic, LF_Faction.MEDIC);
		ctx.NotNull(medicKit, "the base game mode has a MEDIC kit");
		if (medicKit)
			ctx.True(medicKit.Contains(MEDKIT), "the MEDIC kit carries a medical kit");
	}
}
