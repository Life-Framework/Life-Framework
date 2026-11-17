// red-proof: new rule, written first. Run against the stub that returned the skin height (before
// LF_UISheetFit existed as a real rule), the grow, cap and list-squeeze assertions all failed.

// tier: LOGIC
//! #2462: a kit sheet grows to fit its content up to the screen's safe height. LF_UISheetFit.Height keeps
//! the skin height when the content fits, grows to the content when it does not, stops at the cap, and
//! never shrinks a sheet under its skin. ListHeight squeezes a list to scroll once the sheet is capped.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_UISheetFit : LF_Test
{
	override string GetName()
	{
		return "ui/sheet-fit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualFloat(900, LF_UISheetFit.Height(700, 900, 920), 0.01, "content that fits keeps the skin height");
		ctx.EqualFloat(900, LF_UISheetFit.Height(900, 900, 920), 0.01, "content exactly the skin height keeps it");
		ctx.EqualFloat(1000, LF_UISheetFit.Height(1000, 900, 1100), 0.01, "taller content grows the sheet to the content");
		ctx.EqualFloat(920, LF_UISheetFit.Height(1300, 900, 920), 0.01, "past the cap the sheet stops at the cap");
		ctx.EqualFloat(1300, LF_UISheetFit.Height(1300, 900, 0), 0.01, "no cap: the sheet grows to the content");
		ctx.EqualFloat(920, LF_UISheetFit.Height(1300, 920, 600), 0.01, "a cap under the skin height never shrinks the sheet");
		ctx.EqualFloat(900, LF_UISheetFit.Height(0, 900, 920), 0.01, "an unmeasured (zero) content keeps the skin height");

		ctx.EqualFloat(300, LF_UISheetFit.ListHeight(300, 0), 0.01, "no overflow leaves the list alone");
		ctx.EqualFloat(200, LF_UISheetFit.ListHeight(300, 100), 0.01, "the list gives up the overflow and scrolls");
		ctx.EqualFloat(LF_UISheetFit.MIN_LIST_HEIGHT, LF_UISheetFit.ListHeight(300, 900), 0.01, "a list never shrinks below its floor");
	}
}
