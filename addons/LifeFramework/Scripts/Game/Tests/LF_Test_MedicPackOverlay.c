// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_MedicPackOverlay` (batched with its pack-leg sibling) printed `RED-OK medic/pack-overlay (fails with the hook on, 7 assertions)`; first failure `three rungs: the refused rows and the removed extra are gone (expected 3, got 0)`. The hook skips the pack source, so the config lists no rung. `--only` passes.
//! #3047: the medic config reads pack overlays like LF_PropertyConfig and LF_MarketConfig. The ladder is a
//! list keyed by rank id: a pack row adds a rung, a repeated id patches it (pay, promotion time, kit set
//! whole), `replace` rebuilds one, and a remove is refused on every rung but the last. A rung with no title
//! key, a pay below -1 or a patch to such a pay is refused and the rung keeps its values. Without a pack the
//! core file gives no rung and the built-in three answer. The test overlay
//! (Configs/Tests/LF_Test_MedicRankOverlay.conf) is reached through a mission header built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MedicPackOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{5E2A91C7B84D3F06}Configs/Tests/LF_Test_MedicRankOverlay.conf";

	override string GetName()
	{
		return "medic/pack-overlay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_MedicConfig.Reload();

		// No pack: the core file lists no rung, so the built-in three stand.
		LF_MedicConfig core = LF_MedicConfig.Get();
		ctx.True(core != null, "the core medic config loads");
		if (!core)
			return;

		ctx.Equal(0, core.m_aRanks.Count(), "without a pack the core file lists no rung");
		ctx.Equal(3, LF_MedicRanks.RankCount(), "without a pack the built-in three rungs stand");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_MedicConfig.Reload();
		LF_MedicConfig merged = LF_MedicConfig.Get();
		ctx.True(merged != null, "the merged medic config loads");
		if (!merged)
		{
			LF_ConfigService.Reset();
			LF_MedicConfig.Reload();
			return;
		}

		// add x3 (orderly, nurse, chief); badpay and notitle refused; orderly's remove refused (not last);
		// extra added then removed (last).
		ctx.Equal(3, merged.m_aRanks.Count(), "three rungs: the refused rows and the removed extra are gone");
		ctx.Equal(3, LF_MedicRanks.RankCount(), "the ladder is the pack's three rungs");
		if (merged.m_aRanks.Count() != 3)
		{
			LF_ConfigService.Reset();
			LF_MedicConfig.Reload();
			return;
		}

		ctx.EqualStr("orderly", merged.m_aRanks[0].m_sId, "add: orderly is rung 0");
		ctx.EqualStr("nurse", merged.m_aRanks[1].m_sId, "add: nurse is rung 1");
		ctx.EqualStr("chief", merged.m_aRanks[2].m_sId, "add: chief is rung 2");
		ctx.EqualStr("LF-MedicRank_Trainee", LF_MedicRanks.TitleKey(0), "add: the title key is the pack's");
		ctx.True(Math.AbsFloat(LF_MedicRanks.PayPerSecond(0) - 0.2) < 0.0001, "add: orderly pay is the pack's 0.2");
		ctx.Equal(100, LF_MedicRanks.DutySecondsForNext(0), "add: orderly promotes after the pack's 100 seconds");
		ctx.Equal(0, LF_MedicRanks.DutySecondsForNext(2), "the top rung never promotes");

		ctx.True(Math.AbsFloat(LF_MedicRanks.PayPerSecond(1) - 0.35) < 0.0001, "patch: nurse pay is the patched 0.35, the refused -9 changed nothing");
		ctx.Equal(300, LF_MedicRanks.DutySecondsForNext(1), "patch: nurse promotion seconds are the patched 300");
		ctx.EqualStr("LF-MedicRank_Paramedic", LF_MedicRanks.TitleKey(1), "patch: a field the pack leaves alone stays");
		ctx.Equal(2, merged.m_aRanks[1].m_aKit.Count(), "patch: the kit is replaced whole with the pack's two items");

		ctx.True(Math.AbsFloat(LF_MedicRanks.PayPerSecond(2) - 0.45) < 0.0001, "replace: chief pay is the replacing row's 0.45");
		ctx.Equal(0, merged.m_aRanks[2].m_aKit.Count(), "replace: chief is rebuilt from the replacing row alone, so its kit is empty");

		// The kit rule follows the pack ladder: a rank-0 orderly draws a bandage, not the nurse's saline.
		string refusal;
		ctx.True(LF_MedicRanks.CanDrawItem(0, LF_MedicRanks.BANDAGE, refusal), "an orderly draws the bandage");
		ctx.False(LF_MedicRanks.CanDrawItem(0, LF_MedicRanks.SALINE, refusal), "an orderly may not draw the nurse's saline");
		ctx.True(LF_MedicRanks.CanDrawItem(1, LF_MedicRanks.TOURNIQUET, refusal), "a nurse draws the patched-in tourniquet");
		ctx.False(LF_MedicRanks.CanDrawItem(2, LF_MedicRanks.MORPHINE, refusal), "the replaced chief no longer draws morphine");

		LF_ConfigService.Reset();
		LF_MedicConfig.Reload();
	}
}
