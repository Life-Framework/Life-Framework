// red-proof: 2026-11-16 `tools\cli test --no-build --only LF_Test_PolicePromoteRung` on the unfixed code printed [LFTEST] FAIL police/promote-rung: - rank 1 cannot promote rank 0 into rank 1; - rank 2 cannot promote rank 1 into rank 2 (equal, not above); - rank 1 is not offered the promote action on a five-rung ladder. Green after the fix.
// tier: LOGIC
//! #2095: on a ladder longer than two rungs the promoter must hold a rank above the rung the target
//! is promoted INTO. A one- or two-rung ladder keeps the old rule (any senior may promote).
class LF_Test_PolicePromoteRung : LF_Test
{
	override string GetName()
	{
		return "police/promote-rung";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PoliceConfig Ladder(int rungs)
	{
		LF_PoliceConfig config = new LF_PoliceConfig();
		config.m_aRanks = {};
		for (int i = 0; i < rungs; i++)
		{
			config.m_aRanks.Insert(LF_PoliceRank.Create(string.Format("rung%1", i), "EL-Rank_Patrol", 1000, -1, 0, ResourceName.Empty));
		}

		config.m_iShippedRanks = rungs;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount Police(string uid, int rank, bool onDuty)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(rank);
		account.SetOnDuty(onDuty);
		return account;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceConfig.UseForTest(Ladder(5));

		LF_PlayerAccount patrol = Police("promote-rung-p1", 1, true);
		LF_PlayerAccount recruit = Police("promote-rung-r0", 0, true);
		ctx.False(LF_PoliceRanks.Promote("promote-rung-p1", "promote-rung-r0"), "rank 1 cannot promote rank 0 into rank 1");
		ctx.Equal(0, recruit.GetPoliceRank(), "a refused promotion leaves the target's rank");
		ctx.False(LF_PoliceRanks.CanPromote("promote-rung-p1"), "rank 1 is not offered the promote action on a five-rung ladder");

		LF_PlayerAccount sergeant = Police("promote-rung-s3", 3, true);
		LF_PlayerAccount officer = Police("promote-rung-o1", 1, true);
		ctx.True(LF_PoliceRanks.CanPromote("promote-rung-s3"), "rank 3 is offered the promote action");
		ctx.True(LF_PoliceRanks.Promote("promote-rung-s3", "promote-rung-o1"), "rank 3 promotes rank 1 into rank 2");
		ctx.Equal(2, officer.GetPoliceRank(), "the target moved up one rung");

		LF_PlayerAccount equal = Police("promote-rung-e2", 2, true);
		LF_PlayerAccount below = Police("promote-rung-b1", 1, true);
		ctx.False(LF_PoliceRanks.Promote("promote-rung-e2", "promote-rung-b1"), "rank 2 cannot promote rank 1 into rank 2 (equal, not above)");
		ctx.Equal(1, below.GetPoliceRank(), "an equal-rank refusal leaves the target");

		LF_PlayerAccount top = Police("promote-rung-t4", 4, true);
		LF_PlayerAccount nextToTop = Police("promote-rung-n3", 3, true);
		ctx.True(LF_PoliceRanks.Promote("promote-rung-t4", "promote-rung-n3"), "the top rank promotes into the top rung");
		ctx.Equal(4, nextToTop.GetPoliceRank(), "the target reached the top rung");

		LF_PlayerAccount offDuty = Police("promote-rung-off4", 4, false);
		LF_PlayerAccount waiting = Police("promote-rung-w0", 0, true);
		ctx.False(LF_PoliceRanks.Promote("promote-rung-off4", "promote-rung-w0"), "an off-duty top rank cannot promote");

		// A two-rung ladder keeps today's rule: the senior rung promotes the patrol rung.
		LF_PoliceConfig.UseForTest(Ladder(2));
		LF_PlayerAccount twoSenior = Police("promote-rung-2s", 1, true);
		LF_PlayerAccount twoPatrol = Police("promote-rung-2p", 0, true);
		ctx.True(LF_PoliceRanks.CanPromote("promote-rung-2s"), "a two-rung ladder: the senior is offered the promote action");
		ctx.False(LF_PoliceRanks.CanPromote("promote-rung-2p"), "a two-rung ladder: a patrol is not offered the promote action");
		ctx.True(LF_PoliceRanks.Promote("promote-rung-2s", "promote-rung-2p"), "a two-rung ladder: the senior promotes the patrol");
		ctx.Equal(1, twoPatrol.GetPoliceRank(), "a two-rung ladder: the patrol became senior");

		// A one-rung ladder has nowhere to promote to.
		LF_PoliceConfig.UseForTest(Ladder(1));
		LF_PlayerAccount lone = Police("promote-rung-1a", 0, true);
		LF_PlayerAccount loneTarget = Police("promote-rung-1b", 0, true);
		ctx.False(LF_PoliceRanks.CanPromote("promote-rung-1a"), "a one-rung ladder: nobody is offered the promote action");
		ctx.False(LF_PoliceRanks.Promote("promote-rung-1a", "promote-rung-1b"), "a one-rung ladder: nobody promotes");
		ctx.Equal(0, loneTarget.GetPoliceRank(), "a one-rung ladder: the target stays");

		LF_PoliceConfig.Reload();
	}
}
