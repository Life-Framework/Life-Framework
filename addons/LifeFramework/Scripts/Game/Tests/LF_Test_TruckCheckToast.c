// red-proof: RED-OK licence/truck-check-toast, observed 2026-11-24: `tools/cli test --no-build --red LF_Test_TruckCheckToast` RED PROOF OK (the toast keys came back empty and the gate list held 0 gates); `--only` passed.
//! licence/truck-check-toast (WK-W21, #4697): the check says each answer in words. A gate cleared in order
//! names its number (1 to 4), the fourth is the pass, a gate out of order is the failure with the retake
//! wait, and no open run says nothing. The harness verb `truckcheck` reads its gate list the same way: only
//! gates 0 to 3, each a bare digit.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TruckCheckToast : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licence/truck-check-toast";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string key = "";
		string arg = "";
		string pass = "";
		string failed = "";
		string none = "";
		if (!RedProof())
		{
			key = LF_TruckCheckGateComponent.ToastKey(LF_EDrivingCheck.GATE_OK);
			arg = LF_TruckCheckGateComponent.ToastArg(LF_EDrivingCheck.GATE_OK, 2);
			pass = LF_TruckCheckGateComponent.ToastKey(LF_EDrivingCheck.PASSED);
			failed = LF_TruckCheckGateComponent.ToastKey(LF_EDrivingCheck.FAILED_COURSE);
			none = LF_TruckCheckGateComponent.ToastKey(LF_EDrivingCheck.NO_RUN);
		}

		ctx.EqualStr("#LF-TruckCheck_Gate", key, "a gate cleared in order has its toast");
		ctx.EqualStr("3", arg, "gate order 2 is called gate 3");
		ctx.EqualStr("#LF-TruckCheck_Passed", pass, "the last gate is the pass");
		ctx.EqualStr("#LF-TruckCheck_FailedCourse", failed, "a gate out of order is the failure");
		ctx.EqualStr("", none, "no open run says nothing");
		ctx.EqualStr(LF_DrivingCheck.RETAKE_MINUTES.ToString(), LF_TruckCheckGateComponent.ToastArg(LF_EDrivingCheck.FAILED_COURSE, 1), "the failure names the retake wait");

		array<int> gates = {};
		array<int> bad = {};
		array<int> outOfRange = {};
		if (!RedProof())
		{
			gates = LF_TruckCheckHarness.ParseGates("0,1,2,3");
			bad = LF_TruckCheckHarness.ParseGates("0,x");
			outOfRange = LF_TruckCheckHarness.ParseGates("0,4");
		}

		ctx.Equal(4, gates.Count(), "four gates in a clean run");
		if (gates.Count() == 4)
			ctx.Equal(3, gates[3], "the list keeps its order");

		ctx.True(bad.IsEmpty(), "a word that is not a gate refuses the whole list");
		ctx.True(outOfRange.IsEmpty(), "gate 4 is not on the course");
	}
}
