// red-proof: RED-OK police/cuffs-server-rules, observed 2026-11-17 in the same batched `--red` run: with the
// cuffed flag off, `a cuffed shooter's hit on someone else is refused` and `a cuffed character may not drive` fail.
// tier: LOGIC
//! police/cuffs-server-rules (#2417): the pure rules behind the server's cuff refusals. A cuffed
//! instigator's hit is refused unless it is self-damage or bleeding already running; a cuffed
//! character may sit in a cargo seat but not a driver's seat.
class LF_Test_CuffsServerRules : LF_Test
{
	override string GetName()
	{
		return "police/cuffs-server-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		bool cuffed = true;
		if (RedProof())
			cuffed = false;

		ctx.True(LF_CuffEnforcement.RefusesHit(cuffed, false, false), "a cuffed shooter's hit on someone else is refused");
		ctx.False(LF_CuffEnforcement.RefusesHit(cuffed, true, false), "a cuffed character's own fall still hurts");
		ctx.False(LF_CuffEnforcement.RefusesHit(cuffed, false, true), "bleeding from an earlier shot keeps running");
		ctx.False(LF_CuffEnforcement.RefusesHit(false, false, false), "a free shooter's hit is not refused");

		ctx.True(LF_CuffEnforcement.RefusesSeat(cuffed, true), "a cuffed character may not drive");
		ctx.False(LF_CuffEnforcement.RefusesSeat(cuffed, false), "a cuffed prisoner may ride in a cargo seat");
		ctx.False(LF_CuffEnforcement.RefusesSeat(false, true), "a free character may drive");
	}
}
