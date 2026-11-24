// red-proof: 2026-11-24 `tools/cli.mjs test --no-build --red LF_Test_CasualtySpot,LF_Test_CasualtyDressed,LF_Test_CasualtyBed`: RED PROOF OK (all 3 listed test(s) fail with the hook on) (RedProof() leaves the crate out, so "a crate's spot reads as a built surface", "a crate's spot is refused" and "no clear spot at the crate itself" fail).
//! medic/casualty-spot (#4863): where a casualty may lie. The pure verdict refuses water, a prop or floor, a
//! roof, uneven ground and a crowded spot, and a built deck row lets the surface and the roof pass. In the
//! world a search near the test place finds open ground, and a spot with a crate on it is refused (the first
//! casualty of the medic hour lay on a hay bale).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CasualtySpot : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/casualty-spot";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunVerdicts(ctx);

		IEntity place = LF_TestCharacterFixture.Spawn(ctx);
		if (!place)
			return;

		RunWorld(ctx, place);
		LF_TestCharacterFixture.Delete(place);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVerdicts(LF_TestContext ctx)
	{
		LF_CasualtySpotProbe clear = new LF_CasualtySpotProbe();
		ctx.EqualStr("", LF_CasualtySpot.Verdict(clear, false), "open level ground is clear");

		LF_CasualtySpotProbe prop = new LF_CasualtySpotProbe();
		prop.m_bBuiltSurface = true;
		ctx.True(!LF_CasualtySpot.Verdict(prop, false).IsEmpty(), "a prop or floor under the body is refused");
		ctx.EqualStr("", LF_CasualtySpot.Verdict(prop, true), "a built deck row accepts a built surface");

		LF_CasualtySpotProbe roofed = new LF_CasualtySpotProbe();
		roofed.m_bRoofed = true;
		ctx.True(!LF_CasualtySpot.Verdict(roofed, false).IsEmpty(), "a spot under a roof is refused");
		ctx.EqualStr("", LF_CasualtySpot.Verdict(roofed, true), "a built deck row accepts a roof");

		LF_CasualtySpotProbe water = new LF_CasualtySpotProbe();
		water.m_bWater = true;
		ctx.True(!LF_CasualtySpot.Verdict(water, false).IsEmpty(), "water is refused");
		ctx.True(!LF_CasualtySpot.Verdict(water, true).IsEmpty(), "water is refused on a deck row too");

		LF_CasualtySpotProbe uneven = new LF_CasualtySpotProbe();
		uneven.m_fSpread = LF_CasualtySpot.MAX_SPREAD_M + 0.5;
		ctx.True(!LF_CasualtySpot.Verdict(uneven, true).IsEmpty(), "uneven ground is refused on a deck row too");

		LF_CasualtySpotProbe crowded = new LF_CasualtySpotProbe();
		crowded.m_bCrowded = true;
		ctx.True(!LF_CasualtySpot.Verdict(crowded, true).IsEmpty(), "a crowded spot is refused on a deck row too");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorld(LF_TestContext ctx, IEntity place)
	{
		vector spot;
		string why;
		bool found = LF_CasualtySpot.Find(place.GetOrigin(), 12, false, spot, why);
		ctx.True(found, "a search near the test place finds open ground (" + why + ")");
		if (!found)
			return;

		LF_CasualtySpotProbe open = LF_CasualtySpot.ProbeWorld(spot[0], spot[2]);
		ctx.EqualStr("", LF_CasualtySpot.Verdict(open, false), "the found spot reads clear again");

		// A crate on that very spot: the body would lie on a prop.
		IEntity crate;
		if (!RedProof())
			crate = LF_Utils.SpawnEntityPrefab(LF_TestCasualtyProp.CRATE, spot);

		ctx.True(crate != null || RedProof(), "the crate spawns");
		LF_CasualtySpotProbe onCrate = LF_CasualtySpot.ProbeWorld(spot[0], spot[2]);
		ctx.True(onCrate.m_bBuiltSurface, "a crate's spot reads as a built surface");
		ctx.True(!LF_CasualtySpot.Verdict(onCrate, false).IsEmpty(), "a crate's spot is refused");

		vector again;
		string whyNot;
		ctx.False(LF_CasualtySpot.Find(spot, 0, false, again, whyNot), "no clear spot at the crate itself: " + whyNot);

		if (crate)
			SCR_EntityHelper.DeleteEntityAndChildren(crate);
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestCasualtyProp
{
	static const ResourceName CRATE = "{ABA40158860E5B6B}Prefabs/Props/Crates/Crate_01_brown.et";
}
