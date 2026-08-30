// red-proof: break the fallback name guard (e.g. always use "LocalPlayer" + playerId and
// ignore the real player name) or perturb the hash math, then run `tools\cli test --tier fast`;
// the determinism/content assertions go red.

// tier: LOGIC
class EL_Test_LocalIdentity : EL_Test
{
	override string GetName()
	{
		return "spawn/local-identity";
	}

	override void Run(EL_TestContext ctx)
	{
		string first = EL_Utils.BuildLocalIdentity(1);
		string second = EL_Utils.BuildLocalIdentity(1);

		ctx.True(!first.IsEmpty(), "local identity is never empty");
		ctx.EqualStr(first, second, "local identity is deterministic for the same player");
	}
}