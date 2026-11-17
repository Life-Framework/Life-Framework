// red-proof: the bug-fix run. On the unfixed LF_Breach (Arm filed the footprint card with the report) this test failed `- placing the charge leaves no evidence (expected 0, got 1)`; with the fix it passes.
// tier: WORLD
//! Forced entry evidence: the footprint card lies when the charge DETONATES, not when it is placed.
//! Placing a charge on a running fuse files the report and sounds the alarm but leaves nothing to
//! bag; the fuse running out leaves exactly one card, filed against that report; a charge that is
//! repaired (defused) before it goes off never leaves one.
class LF_Test_BreachEvidence : LF_Test
{
	override string GetName()
	{
		return "crime/breach-evidence";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "breach-evidence-robber";
		string policeUid = "breach-evidence-police";
		string armedTarget = "breach-evidence-door";
		string defusedTarget = "breach-evidence-defused";

		LF_Evidence.ResetStatics();
		LF_Breach.Configure(armedTarget, 5, "");
		LF_Breach.Configure(defusedTarget, 5, "");
		LF_PlayerAccountManager.GetOrCreate(policeUid).SetFaction(LF_Faction.POLICE);

		IEntity robber = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "85 0 85", true);
		if (!robber)
			return;

		ctx.Equal(2, LF_InventoryUtils.AddAmount(robber, LF_Breach.FENCE_CHARGE_PREFAB, 2), "two charges land in the robber's inventory");
		if (ctx.FailureCount() > 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(robber);
			return;
		}

		// Placing: the report is filed, nothing lies on the ground yet.
		ctx.True(LF_Breach.Arm(armedTarget, uid, robber), "the charge is placed");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(armedTarget), "placing files the report at once");
		int report = LF_WantedReports.CountFor(uid);
		ctx.Equal(1, report, "one crime report stands for the charge");
		ctx.Equal(0, LF_Evidence.EvidenceCount(), "placing the charge leaves no evidence");

		// The fuse runs: still nothing.
		ctx.False(LF_Breach.Advance(armedTarget, 4), "the fuse is still running");
		ctx.Equal(0, LF_Evidence.EvidenceCount(), "a running fuse leaves no evidence");

		// Detonation: exactly one card, against the report the placement filed.
		ctx.True(LF_Breach.Advance(armedTarget, 1), "the charge detonates");
		ctx.Equal(1, LF_Evidence.EvidenceCount(), "the detonation leaves exactly one piece of evidence");
		int sequence = LF_WantedReports.GetFor(uid)[0].m_iSequence;
		ctx.Equal(1, LF_Evidence.CountFor(sequence, LF_Evidence.KIND_FOOTPRINT), "it is the footprint card of that report");

		// Later steps add nothing: the plan is spent.
		LF_Breach.Advance(armedTarget, 10);
		ctx.Equal(1, LF_Evidence.EvidenceCount(), "an open door leaves nothing more");

		// Defused before it goes off: the report stands, no card ever lies.
		int before = LF_Evidence.EvidenceCount();
		ctx.True(LF_Breach.Arm(defusedTarget, uid, robber), "a second charge is placed");
		ctx.Equal(before, LF_Evidence.EvidenceCount(), "placing the second charge leaves no evidence");
		ctx.True(LF_Breach.Repair(defusedTarget, policeUid), "the charge is defused");
		LF_Breach.Advance(defusedTarget, 10);
		ctx.True(LF_Breach.IsLocked(defusedTarget), "the defused door stays shut");
		ctx.Equal(before, LF_Evidence.EvidenceCount(), "a defused charge leaves no evidence");

		LF_Evidence.ClearAll();
		SCR_EntityHelper.DeleteEntityAndChildren(robber);
	}
}
