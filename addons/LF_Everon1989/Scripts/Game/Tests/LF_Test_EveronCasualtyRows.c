// red-proof: 2026-11-24 `tools/cli.mjs test --no-build --red LF_Test_EveronCasualtyRows`: RED-OK (RedProof() skips LF_CasualtyCallConfig.Get(), so the config never loads and the row and place-kind checks fail).
//! everon/casualty-rows (#4829): under the Everon 1989 header (the pack leg) the casualty-call config the pack
//! overlay fills is on and has five rows (mine, woodlot, farm compound, dock, roadside fuel stop), each with a
//! weight, a place kind the island layers place and a summary in the pack string table (US English, "down at" or
//! "hurt at" a place). The generator's cap and interval give about three calls an hour with one medic on duty.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCasualtyRows : LF_Test
{
	//! Place kinds the MainWorld layers place: "mine" (Place_eastrise_mine, Place_highstone_mine), "LogPile"
	//! (Place_saint-philippe_woodlot), "FarmCompound" (Place_farmcompound_compound), "Dock"
	//! (Place_saint-philippe-dock_dock) and "RoadFuel" (Place_roadfuel_fuel).
	protected static const string ISLAND_KINDS = "mine,LogPile,FarmCompound,Dock,RoadFuel";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/casualty-rows";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CasualtyCallConfig.ResetStatics();
		LF_CasualtyCallConfig config;
		if (!RedProof())
			config = LF_CasualtyCallConfig.Get();

		ctx.NotNull(config, "the casualty config loads");
		if (!config)
			return;

		ctx.True(config.m_bEnabled, "the pack turns the generator on");
		ctx.Equal(5, config.m_aRows.Count(), "the pack lists five casualty rows");
		ctx.True(config.m_iIntervalSeconds >= 900 && config.m_iIntervalSeconds <= 1800, "a roll every 15 to 30 minutes: about three calls an hour");
		ctx.Equal(1, config.m_iPerMedic, "one open casualty per medic on duty");
		ctx.True(config.ExpirySeconds() >= 480 && config.ExpirySeconds() <= 720, "a casualty lasts 8 to 12 minutes: read the board, draw an ambulance, drive (#4852)");

		array<string> islandKinds = {};
		ISLAND_KINDS.Split(",", islandKinds, false);
		int weights;
		foreach (LF_CasualtyCallRow row : config.m_aRows)
		{
			ctx.True(islandKinds.Find(row.m_sPlaceKind) != -1, "the island places that kind: " + row.m_sPlaceKind);
			ctx.True(row.m_iWeight > 0, "the row is picked: " + row.m_sPlaceKind);
			weights += row.m_iWeight;

			ctx.True(row.m_sSummary.StartsWith("#"), "the summary is a string key: " + row.m_sPlaceKind);
			string text = WidgetManager.Translate(row.m_sSummary);
			ctx.True(!text.IsEmpty() && text != row.m_sSummary, "the summary is in the pack string table: " + row.m_sSummary);
			ctx.True(text.Contains(" at ") || text.Contains(" on "), "the summary says where: " + text);

			// #4863: the casualty wears a work outfit that fits the place, from the pack's outfit presets.
			ctx.True(row.m_sOutfit.Contains("Configs/NpcOutfits/"), "the row names an outfit preset: " + row.m_sPlaceKind);
			ctx.True(LF_NpcOutfitDresser.Garments(row.m_sOutfit).Count() >= 3, "the outfit has a torso, trousers and shoes: " + row.m_sPlaceKind);
		}

		ctx.True(weights > 0, "the rows have a weight in all");
	}
}
