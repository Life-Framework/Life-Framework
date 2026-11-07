// red-proof: PENDING for this revision - not yet observed (the server install was being restored).
// Planned perturbation: build the identity from "LocalPlayer" alone (drop the playerId) and run
// `tools\cli test --no-build --tier fast`; the distinct-players assertion goes red. The old test
// only checked non-empty and same-input-same-output.

// tier: LOGIC
class EL_Test_LocalIdentity : EL_Test
{
	override string GetName()
	{
		return "spawn/local-identity";
	}

	override void Run(EL_TestContext ctx)
	{
		// The fallback identity keys accounts, bank balances and saves on a non-dedicated
		// session. Two players must never share one, and one player must keep theirs.
		string first = EL_Utils.BuildLocalIdentity(1);
		string again = EL_Utils.BuildLocalIdentity(1);
		string other = EL_Utils.BuildLocalIdentity(2);

		ctx.EqualStr(first, again, "local identity is stable for the same player");
		ctx.True(first != other, "two players get different local identities");
		ctx.True(first.IndexOf("00bbbddd-") == 0, "local identity carries the local-session prefix");
	}
}
