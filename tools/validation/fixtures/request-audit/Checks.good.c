// The same request with the checks written. Each handler carries a different
// kind of check; validate-request-audit must pass all three.
class LF_FixtureChecksController
{
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FixtureGive(RplId target, int amount)
	{
		IEntity receiver = LF_NetworkUtils.FindEntityByRplId(target);
		IEntity sender = GetControlledEntity();
		if (amount <= 0 || !receiver || !sender)
			return;
		if (vector.Distance(sender.GetOrigin(), receiver.GetOrigin()) > 3)
			return;
		LF_FixtureChecks.Pay(sender, receiver, amount);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FixtureDebug(int value)
	{
		if (!LF_DevGate.CheckCharacterRequest(this, "Fixture", "debug"))
			return;
		LF_FixtureChecks.Apply(value);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FixtureDelegated(int value)
	{
		// The check lives one call down, as in most real handlers.
		LF_FixtureChecks.Serve(this, value);
	}
}

class LF_FixtureChecks
{
	static void Pay(IEntity sender, IEntity receiver, int amount) {}
	static void Apply(int value) {}

	static void Serve(IEntity user, int value)
	{
		if (!LF_PoliceUtils.IsOnDutyOfficer(user))
			return;
		Apply(value);
	}
}
