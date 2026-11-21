// red-proof: 2026-11-19 RedProof() cuts the pond table back to its first species (the trout-only table of #2995) and the perch and carp checks FAIL; the plain run is green.
//! fishing/pond-table: the shipped `pond` table that Spring Pond and Black Lake spots use gives trout, perch and carp (#3667), never an ocean fish, and
//! carp keep their dawn and evening hours (a midday cast gives trout or perch).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FishingPondTable : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/pond-table";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FishingConfig.Reload();
		LF_FishingConfig shipped = LF_FishingConfig.Get();
		ctx.True(shipped != null, "the core fishing config resolves");
		if (!shipped)
			return;

		LF_FishTableDef pond = shipped.FindTable("pond");
		ctx.True(pond != null, "the pond table exists");
		if (!pond)
		{
			LF_FishingConfig.Reload();
			return;
		}

		if (RedProof() && pond.m_aSpecies.Count() > 1)
			pond.m_aSpecies.Resize(1);

		int trout = 0;
		int perch = 0;
		int carpDawn = 0;
		int carpNoon = 0;
		int ocean = 0;
		for (int step = 0; step < 50; step++)
		{
			float roll = step / 50.0;
			LF_FishCatch dawn = LF_FishingRules.PickCatch(shipped, "pond", 5, "POND", 6, 3, null, roll, roll);
			LF_FishCatch noon = LF_FishingRules.PickCatch(shipped, "pond", 5, "POND", 12, 3, null, roll, roll);
			if (dawn && dawn.m_Species)
			{
				if (dawn.m_Species.m_sId == "trout")
					trout++;
				if (dawn.m_Species.m_sId == "perch")
					perch++;
				if (dawn.m_Species.m_sId == "carp")
					carpDawn++;
				if (dawn.m_Species.LivesIn("OCEAN"))
					ocean++;
			}

			if (noon && noon.m_Species)
			{
				if (noon.m_Species.m_sId == "carp")
					carpNoon++;
				if (noon.m_Species.LivesIn("OCEAN"))
					ocean++;
			}
		}

		ctx.True(trout > 0, "a pond cast at dawn can land a trout");
		ctx.True(perch > 0, "a pond cast at dawn can land a perch");
		ctx.True(carpDawn > 0, "a pond cast at dawn can land a carp");
		ctx.Equal(0, carpNoon, "carp keep to dawn and evening, so a midday cast gives none");
		ctx.Equal(0, ocean, "the pond never gives an ocean fish");
		LF_FishingConfig.Reload();
	}
}
