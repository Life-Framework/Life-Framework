// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: building the fallback name from "LocalPlayer"
// alone (no playerId) in LF_Utils.BuildLocalIdentity failed it with
// `[LFTEST] FAIL spawn/local-identity`, `- two players get different local identities`. Restored; green.

// tier: LOGIC
class LF_Test_LocalIdentity : LF_Test
{
	override string GetName()
	{
		return "spawn/local-identity";
	}

	override void Run(LF_TestContext ctx)
	{
		// The fallback identity keys accounts, bank balances and saves on a non-dedicated
		// session. Two players must never share one, and one player must keep theirs.
		string first = LF_Utils.BuildLocalIdentity(1);
		string again = LF_Utils.BuildLocalIdentity(1);
		string other = LF_Utils.BuildLocalIdentity(2);

		ctx.EqualStr(first, again, "local identity is stable for the same player");
		ctx.True(first != other, "two players get different local identities");
		ctx.True(first.IndexOf("00bbbddd-") == 0, "local identity carries the local-session prefix");
	}
}
