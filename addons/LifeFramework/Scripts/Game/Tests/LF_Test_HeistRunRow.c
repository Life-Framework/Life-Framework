// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_HeistRunRow` printed `RED-OK crime/heist-run-row (fails with the hook on, 11 assertions)`; RedProof() drops the getaway stage, so the row builds no run.
//! #3748: a heist run built from a configured job row (LF_HeistSiteRow) takes its door and vault ids,
//! getaway radius, clock, police minimum and clerk hands-up flag from the row; a getaway with no
//! radius keeps the site's own; a row with no vault stage, or no row at all, builds no run. The
//! core config ships no job, so no breach target is refused for police here.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HeistRunRow : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/heist-run-row";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HeistStageRow Stage(string kind, string targetId, float radius, bool clerk)
	{
		LF_HeistStageRow stage = new LF_HeistStageRow();
		stage.m_sKind = kind;
		stage.m_sTargetId = targetId;
		stage.m_fRadius = radius;
		stage.m_bClerkHandsUp = clerk;
		return stage;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_HeistSiteRow row = LF_HeistSiteRow.Make("row-test");
		row.m_iClockSeconds = 300;
		row.m_iMinPoliceOnDuty = 3;
		row.m_aStages.Insert(Stage(LF_HeistKinds.DOOR, "row-test-door", 0, true));
		row.m_aStages.Insert(Stage(LF_HeistKinds.VAULT, "row-test-vault", 0, false));
		if (!RedProof())
			row.m_aStages.Insert(Stage(LF_HeistKinds.GETAWAY, "", 75, false));

		LF_HeistRun run = LF_HeistRun.FromRow("row-site", row, "0 0 0", 40);
		ctx.NotNull(run, "a complete job row builds a run");
		if (run)
		{
			ctx.EqualStr("row-test-door", run.GetDoorId(), "the door id comes from the door stage");
			ctx.EqualStr("row-test-vault", run.GetVaultId(), "the vault id comes from the vault stage");
			ctx.True(run.GetRadius() == 75, "the getaway radius comes from the getaway stage");
			ctx.Equal(300000, run.GetClockMs(), "the clock comes from the row");
			ctx.Equal(3, run.GetMinPolice(), "the police minimum comes from the row");
			ctx.True(run.ClerkMayHandsUp(), "the clerk flag comes from the door stage");
			ctx.Equal(LF_EHeistState.IDLE, run.GetState(), "a fresh run is idle");
		}

		// A getaway stage with no radius keeps the site component's own.
		LF_HeistSiteRow plain = LF_HeistSiteRow.Make("row-plain");
		plain.m_aStages.Insert(Stage(LF_HeistKinds.DOOR, "plain-door", 0, false));
		plain.m_aStages.Insert(Stage(LF_HeistKinds.VAULT, "plain-vault", 0, false));
		plain.m_aStages.Insert(Stage(LF_HeistKinds.GETAWAY, "", 0, false));
		LF_HeistRun fallback = LF_HeistRun.FromRow("plain-site", plain, "0 0 0", 40);
		ctx.NotNull(fallback, "a getaway with no radius still builds");
		if (fallback)
		{
			ctx.True(fallback.GetRadius() == 40, "no row radius: the site's radius stands");
			ctx.False(fallback.ClerkMayHandsUp(), "no clerk flag on the door: none on the run");
		}

		// Refused rows: no vault stage, no row.
		LF_HeistSiteRow noVault = LF_HeistSiteRow.Make("row-novault");
		noVault.m_aStages.Insert(Stage(LF_HeistKinds.DOOR, "nv-door", 0, false));
		noVault.m_aStages.Insert(Stage(LF_HeistKinds.GETAWAY, "", 50, false));
		ctx.True(LF_HeistRun.FromRow("nv-site", noVault, "0 0 0", 40) == null, "a job with no vault stage builds no run");
		ctx.True(LF_HeistRun.FromRow("none-site", null, "0 0 0", 40) == null, "no row builds no run");

		// The site component: an unknown heist id builds nothing; no id keeps the attribute path.
		ctx.True(LF_HeistRunComponent.BuildRun("c-site", "row-test-unknown-job", "", "", "0 0 0", 40, 600) == null, "an unknown heist id builds no run");
		LF_HeistRun attr = LF_HeistRunComponent.BuildRun("c-site", "", "c-door", "c-vault", "0 0 0", 40, 600);
		ctx.NotNull(attr, "no heist id: the attributes build the run");
		if (attr)
			ctx.Equal(0, attr.GetMinPolice(), "an attribute run has no police minimum");

		// The core ships no job: no breach target waits for police.
		ctx.EqualStr("", LF_HeistRun.ArmRefusal("row-test-door", 0), "a door no configured job names is never refused");
	}
}
