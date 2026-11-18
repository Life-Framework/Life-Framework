// red-proof: 2026-11-18 `test --red LF_Test_HarnessUseFallback`: RED-OK, "a named use that lands on another
// named prop fails" (expected 'wrong_target:Fit_gun_counter', got '') with the hook skipping the refusal.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HarnessUseFallback : LF_Test
{
	override string GetName()
	{
		return "agent-harness/use-fallback-strict";
	}

	override void Run(LF_TestContext ctx)
	{
		// #2935: `use LF_ShopAction <missing store>` opened the gun counter next door and said ok.
		string refusal = "";
		if (!RedProof())
			refusal = LF_AgentHarness.FallbackRefusal("Fit_store_till", "Fit_gun_counter");
		ctx.EqualStr("wrong_target:Fit_gun_counter", refusal, "a named use that lands on another named prop fails");

		// A replicated NPC has no layer name on a client: the unnamed host stays allowed.
		ctx.EqualStr("", LF_AgentHarness.FallbackRefusal("Fit_bank_teller", ""), "an unnamed host (replicated NPC) is allowed");
		ctx.EqualStr("", LF_AgentHarness.FallbackRefusal("Fit_board", "Fit_board"), "the named host itself is allowed");
		ctx.EqualStr("", LF_AgentHarness.FallbackRefusal("", "Fit_gun_counter"), "an unnamed use takes the nearest host");
	}
}
