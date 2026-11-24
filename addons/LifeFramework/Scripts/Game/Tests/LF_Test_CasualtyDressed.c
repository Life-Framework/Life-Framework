// red-proof: 2026-11-24 `tools/cli.mjs test --no-build --red LF_Test_CasualtySpot,LF_Test_CasualtyDressed,LF_Test_CasualtyBed`: RED PROOF OK (all 3 listed test(s) fail with the hook on) (RedProof() skips the worn check, so "the casualty wears" every garment of the outfit fails).
//! medic/casualty-dressed (#4863): a casualty the generator raises wears the work outfit of its row
//! (every garment of the preset is on the body, not the default underwear), lies downed on clear ground
//! (nothing built under it, nothing over it), and a row with no outfit leaves the resident as the prefab
//! made them.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CasualtyDressed : LF_Test
{
	protected static const string KIND = "test_casualty_dressed_site";
	protected static const ResourceName OUTFIT = "{FE3FF19CE38781F8}Configs/NpcOutfits/LF_Outfit_Doctor.conf";
	protected static const int MEDIC_ID = 9483;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/casualty-dressed";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CasualtyCalls.ClearAll();
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyCallConfig config = new LF_CasualtyCallConfig();
		config.m_bEnabled = true;
		config.m_iIntervalSeconds = 1;
		config.m_iPerMedic = 1;
		config.m_iMaxOpen = 2;
		config.m_fSpotRadius = 6;
		config.m_sPrefab = LF_CasualtyCallConfig.DEFAULT_PREFAB;
		config.m_aRows = {};
		config.m_aRows.Insert(LF_CasualtyCallRow.Make(KIND, "test casualty: dressed", 1, OUTFIT));
		LF_CasualtyCallConfig.Override(config);
		LF_ServiceBoard.Install();

		IEntity place = LF_TestCharacterFixture.Spawn(ctx);
		if (place)
		{
			bool registered = LF_Places.Register(KIND, "dressed-1", place);
			ctx.True(registered, "the test place registers");
			if (registered)
				RunDressed(ctx);
			LF_Places.Unregister(KIND, "dressed-1");
		}

		LF_CasualtyCalls.ClearAll();
		LF_TestCharacterFixture.Delete(place);
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyCallConfig.ResetStatics();
		LF_Downed.ResetStatics();
		LF_CasualtyCalls.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDressed(LF_TestContext ctx)
	{
		array<ref LF_ServiceCandidate> medics = {};
		medics.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.MEDIC));
		LF_Casualty casualty = LF_CasualtyCalls.Tick(System.GetTickCount() + 2000, medics);
		ctx.True(casualty != null, "the generator raises a casualty");
		if (!casualty)
			return;

		IEntity body = LF_CasualtyCalls.Body(casualty);
		ctx.True(body != null, "the casualty has a body");
		if (!body)
			return;

		array<ResourceName> garments = LF_NpcOutfitDresser.Garments(OUTFIT);
		ctx.True(garments.Count() >= 3, "the outfit names at least a torso, trousers and shoes");
		foreach (ResourceName garment : garments)
		{
			bool worn;
			if (!RedProof())
				worn = LF_NpcOutfitDresser.IsWearing(body, garment);

			ctx.True(worn, "the casualty wears " + garment);
		}

		ctx.True(LF_Downed.IsDowned(body), "the casualty is downed");
		LF_CasualtySpotProbe probe = LF_CasualtySpot.ProbeWorld(body.GetOrigin()[0], body.GetOrigin()[2]);
		ctx.EqualStr("", LF_CasualtySpot.Verdict(probe, false), "the casualty lies on clear ground");
	}
}
