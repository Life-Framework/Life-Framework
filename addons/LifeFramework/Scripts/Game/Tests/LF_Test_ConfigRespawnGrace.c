// red-proof: Observed 2026-11-16: `tools\cli test --no-build --red LF_Test_ConfigRespawnGrace` printed RED PROOF OK (with the fixture skipped, rejoinAtLastPosition, rejoinMaxAgeHours and grace minutes were not read).
// tier: WORLD
//! The `respawn` and `grace` owner sections (#1779, chapter Arrival #1007). The flagship's config.json turns
//! rejoin-at-the-saved-spot on and gives a new character 30 minutes of grace; the core .conf files stay off.
//! A config without the sections keeps the .conf values; a bad value keeps the .conf value.
class LF_Test_ConfigRespawnGrace : LF_Test
{
	static const string FIXTURE = "{\"schemaVersion\":1,\"respawn\":{\"rejoinAtLastPosition\":true,\"rejoinMaxAgeHours\":6},\"grace\":{\"minutes\":30}}";
	static const string BAD = "{\"schemaVersion\":1,\"respawn\":{\"rejoinAtLastPosition\":\"yes\",\"rejoinMaxAgeHours\":-4},\"grace\":{\"minutes\":9000}}";

	override string GetName()
	{
		return "config/respawn-grace";
	}

	protected void Reload()
	{
		LF_RespawnConfig.Reload();
		LF_GraceConfig.ResetStatics();
	}

	override void Run(LF_TestContext ctx)
	{
		// No sections: the shipped .conf values (rejoin off, 12 h, no grace).
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test:respawn-none");
		Reload();
		LF_RespawnConfig respawn = LF_RespawnConfig.Get();
		ctx.True(respawn != null, "Configs/Spawning/LF_Respawn.conf resolves");
		if (respawn)
		{
			ctx.False(respawn.m_bRejoinAtLastPosition, "rejoin is off without the section");
			ctx.Equal(12, respawn.m_iRejoinMaxAgeHours, "the stale limit is 12 hours without the section");
		}
		ctx.Equal(0, LF_GraceConfig.Minutes(), "no grace without the section");

		// The flagship shape.
		LF_ConfigService.Reset();
		if (!RedProof())
			LF_ConfigService.LoadOwnerString(FIXTURE, "test:respawn-full");
		else
			LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test:respawn-red");
		Reload();
		respawn = LF_RespawnConfig.Get();
		if (respawn)
		{
			ctx.True(respawn.m_bRejoinAtLastPosition, "rejoinAtLastPosition read");
			ctx.Equal(6, respawn.m_iRejoinMaxAgeHours, "rejoinMaxAgeHours read");
			ctx.True(respawn.m_bWakeInHospital, "a key left out keeps the .conf value");
		}
		ctx.Equal(30, LF_GraceConfig.Minutes(), "grace minutes read");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_RespawnConfig.SECTION), "respawn is a known top-level key");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_GraceConfig.SECTION), "grace is a known top-level key");

		// Bad values keep the .conf ones.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(BAD, "test:respawn-bad");
		Reload();
		respawn = LF_RespawnConfig.Get();
		if (respawn)
		{
			ctx.False(respawn.m_bRejoinAtLastPosition, "a wrong-type rejoin keeps the .conf value");
			ctx.Equal(12, respawn.m_iRejoinMaxAgeHours, "a negative age keeps the .conf value");
		}
		ctx.Equal(0, LF_GraceConfig.Minutes(), "a grace over the limit keeps the .conf value");

		LF_ConfigService.Reset();
		Reload();
	}
}
