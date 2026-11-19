// red-proof: Observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_HandsUpRules`: with the RedProof hook skipping the target's own accepted answer the hands never went up (`the hands belong to the target (expected 3, got -1)`); RED PROOF OK. Hook off; green.
//! Hands-up rules (#3394, LF_HandsUpRules): a target's own accepted answer starts it, another
//! player's request alone does not, and moving 2 m, drawing a weapon or the timeout ends it with
//! the reason.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HandsUpRules : LF_Test
{
	override string GetName()
	{
		return "crime/hands-up-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		const int TARGET = 3;
		const int OTHER = 7;
		vector spot = Vector(100, 0, 200);

		// Only the target's own accepted answer starts it.
		ctx.True(LF_HandsUpRules.AnswerStarts(TARGET, TARGET, true), "the target's own yes starts it");
		ctx.False(LF_HandsUpRules.AnswerStarts(TARGET, OTHER, true), "another player's answer alone does not");
		ctx.False(LF_HandsUpRules.AnswerStarts(TARGET, TARGET, false), "the target's refusal does not");
		ctx.False(LF_HandsUpRules.AnswerStarts(-1, -1, true), "no valid player starts nothing");

		LF_HandsUpRules foreign = new LF_HandsUpRules();
		ctx.False(foreign.Answer(TARGET, OTHER, true, spot, 0), "a foreign request does not raise the hands");
		ctx.False(foreign.IsActive(), "still down after a foreign request");

		LF_HandsUpRules refused = new LF_HandsUpRules();
		ctx.False(refused.Answer(TARGET, TARGET, false, spot, 0), "a refusal does not raise the hands");
		ctx.False(refused.IsActive(), "still down after a refusal");

		// A choice starts it, and a still target stays up.
		LF_HandsUpRules rules = new LF_HandsUpRules();
		bool started = false;
		if (!RedProof())
			started = rules.Answer(TARGET, TARGET, true, spot, 1000);
		ctx.True(started, "the target's own yes raises the hands");
		ctx.True(rules.IsActive(), "hands are up");
		ctx.Equal(TARGET, rules.GetTargetId(), "the hands belong to the target");
		ctx.Equal(LF_HandsUpEndReason.NONE, rules.Tick(spot, false, 2000), "standing still keeps them up");
		ctx.Equal(LF_HandsUpEndReason.NONE, rules.Tick(Vector(100.5, 0, 200), false, 3000), "half a metre of shuffling keeps them up");
		ctx.True(rules.IsActive(), "still up after a shuffle");
		ctx.False(rules.Answer(TARGET, TARGET, true, spot, 9000), "repeating the yes does not restart it");

		// Moving 2 m ends it with MOVED.
		ctx.Equal(LF_HandsUpEndReason.MOVED, rules.Tick(Vector(102, 0, 200), false, 4000), "2 m away ends it as moved");
		ctx.False(rules.IsActive(), "down after moving");
		ctx.Equal(LF_HandsUpEndReason.MOVED, rules.GetEndReason(), "the reason is kept");
		ctx.Equal(-1, rules.GetTargetId(), "no target once down");

		// Drawing a weapon ends it with ARMED, even standing still.
		LF_HandsUpRules armed = new LF_HandsUpRules();
		armed.Answer(TARGET, TARGET, true, spot, 0);
		ctx.Equal(LF_HandsUpEndReason.ARMED, armed.Tick(spot, true, 500), "drawing a weapon ends it as armed");
		ctx.Equal(LF_HandsUpEndReason.ARMED, armed.GetEndReason(), "armed is kept");

		// The timeout ends it with TIMEOUT, not a moment before.
		LF_HandsUpRules timed = new LF_HandsUpRules();
		timed.Answer(TARGET, TARGET, true, spot, 5000);
		int justBefore = 5000 + LF_HandsUpRules.TIMEOUT_SECONDS * 1000 - 1;
		int atTimeout = 5000 + LF_HandsUpRules.TIMEOUT_SECONDS * 1000;
		ctx.Equal(LF_HandsUpEndReason.NONE, timed.Tick(spot, false, justBefore), "still up just before the timeout");
		ctx.Equal(LF_HandsUpEndReason.TIMEOUT, timed.Tick(spot, false, atTimeout), "the timeout ends it");
		ctx.Equal(LF_HandsUpEndReason.TIMEOUT, timed.GetEndReason(), "timeout is kept");

		// A tick while down reports nothing and keeps the old reason; a fresh answer starts again.
		ctx.Equal(LF_HandsUpEndReason.NONE, timed.Tick(spot, true, atTimeout + 1), "a tick while down ends nothing");
		ctx.Equal(LF_HandsUpEndReason.TIMEOUT, timed.GetEndReason(), "the old reason stands");
		ctx.True(timed.Answer(TARGET, TARGET, true, spot, 99000), "a new yes raises them again");
		ctx.Equal(LF_HandsUpEndReason.NONE, timed.GetEndReason(), "the reason clears on a new start");

		// The target can lower them; nobody else can.
		ctx.False(timed.Lower(OTHER), "another player cannot lower them");
		ctx.True(timed.IsActive(), "still up");
		ctx.True(timed.Lower(TARGET), "the target lowers them");
		ctx.Equal(LF_HandsUpEndReason.LOWERED, timed.GetEndReason(), "lowered is the reason");
	}
}
