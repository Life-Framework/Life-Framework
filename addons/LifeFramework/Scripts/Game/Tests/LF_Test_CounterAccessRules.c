// red-proof: observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_CounterAccessRules`: RED-OK npc/counter-customer-side (the hook skips InFront and the Get in rule).
//! First-2h review C14 and C18, pure rules: a counter clerk's talk shows on the customer side (in front
//! of the clerk, a step round the counter end allowed) and not from behind the counter; a police car's
//! Get in shows only to an officer on duty. No world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CounterAccessRules : LF_Test
{
	override string GetName()
	{
		return "npc/counter-customer-side";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector clerk = "10 0 10";
		vector facing = "0 0 1";
		bool front = false;
		bool behind = true;
		bool endOfCounter = false;
		bool service = true;
		bool officer = false;
		bool civilian = true;
		if (!RedProof())
		{
			front = LF_CounterAccess.InFront(clerk, facing, "10 0 12");
			behind = LF_CounterAccess.InFront(clerk, facing, "10 0 8");
			endOfCounter = LF_CounterAccess.InFront(clerk, facing, "12 0 9.9");
			service = LF_GetInServiceCar.Shows(false, false);
			officer = LF_GetInServiceCar.Shows(true, true);
			civilian = LF_GetInServiceCar.Shows(true, false);
		}

		ctx.True(front, "a customer in front of the clerk is on the customer side");
		ctx.False(behind, "a player behind the clerk, behind the counter, is not");
		ctx.True(endOfCounter, "a step round the end of the counter still is");
		ctx.True(LF_CounterAccess.InFront(clerk, "0 0 0", "10 0 8"), "a clerk with no facing keeps the prompt (never hides it blind)");
		ctx.True(service, "an ordinary car shows its Get in prompt to everyone");
		ctx.True(officer, "a police car shows Get in to an officer on duty");
		ctx.False(civilian, "a police car hides Get in from a civilian");
	}
}
