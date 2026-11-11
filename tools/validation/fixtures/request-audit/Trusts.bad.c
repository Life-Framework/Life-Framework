// A handler that spends a client-sent amount and acts on a client-sent entity
// with no server-side check. validate-request-audit must report it.
class LF_FixtureTrustsController
{
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FixtureGive(RplId target, int amount)
	{
		IEntity receiver = LF_NetworkUtils.FindEntityByRplId(target);
		LF_FixtureTrusts.Pay(receiver, amount);
	}
}

class LF_FixtureTrusts
{
	static void Pay(IEntity receiver, int amount)
	{
		LF_MoneyUtils.Add(receiver, amount);
	}
}
