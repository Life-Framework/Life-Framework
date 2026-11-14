// red-proof: 2026-11-14, with both evidence lockers on the prefab's default m_sTargetId (layer overrides reverted),
//   cli test --no-build --only LF_Test_PlacesEvidenceLocker:
//   [LFTEST]   - both evidence lockers are found under their own breach target id
//   (console: [LFDebug:Places] register refused: BreachTarget 'LF_EvidenceLocker' is already placed)
// Restored; green.
// tier: WORLD
//! places/evidence-locker: DebugWorld places an evidence locker at the test annex and one at the
//! village police station (#1004). Both are breach targets, so each carries its own id; sharing the
//! prefab default made the second register refuse with "BreachTarget 'LF_EvidenceLocker' is already
//! placed" and left that locker unbreachable.
class LF_Test_PlacesEvidenceLocker : LF_Test
{
	protected static const string LOCKER_ID = "EvidenceLocker";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/evidence-locker";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity annex = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.POLICE_EVIDENCE_LOCKER);
		ctx.NotNull(annex, "the annex evidence locker is placed");

		array<IEntity> targets = {};
		LF_Places.FindAll(LF_Breach.PLACE_KIND, targets);
		int lockers;
		foreach (IEntity target : targets)
		{
			LF_BreachTargetComponent breach = LF_Component<LF_BreachTargetComponent>.Find(target);
			if (breach && breach.GetTargetId().Contains(LOCKER_ID) && LF_Places.Find(LF_Breach.PLACE_KIND, breach.GetTargetId()) == target)
				lockers++;
		}

		ctx.True(lockers >= 2, "both evidence lockers are found under their own breach target id");
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, LF_TestAnnex.POLICE_EVIDENCE_LOCKER) == annex, "the annex locker holds its own breach target id");
	}
}
