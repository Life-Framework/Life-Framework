// red-proof: 2026-11-17 `--red` (RedProof() skips the three bare Rpc() calls): RED-OK core/rpc-routing-owned; red: "a Server Rpc on a server-owned entity runs here inside the call (expected 1, got 0)" and the Owner twin.
// tier: WORLD
//! #2641 case (a): the server calls a bare Rpc() on an entity it owns, with a valid RplId (a listen host's own
//! body). Settles which receivers need a host branch. Findings: docs/architecture/rpc-routing.md.
class LF_Test_RpcRoutingOwned : LF_Test
{
	static const ResourceName PROBE_PREFAB = "{69B26F072FCDC869}Prefabs/Debug/LF_TestRpcProbe.et";

	override string GetName()
	{
		return "core/rpc-routing-owned";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(PROBE_PREFAB, "0 0 0");
		if (!entity)
		{
			ctx.Fail("the probe prefab did not spawn: " + PROBE_PREFAB);
			return;
		}

		LF_TestRpcProbeComponent probe = LF_TestRpcProbeComponent.Cast(entity.FindComponent(LF_TestRpcProbeComponent));
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!probe || !rpl)
		{
			ctx.Fail("the probe has no LF_TestRpcProbeComponent or no RplComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}
		probe.SetLabel("owned");

		ctx.True(Replication.IsServer(), "the test boot is the server");
		ctx.True(rpl.Id().IsValid(), "the probe is registered for replication (valid RplId)");
		ctx.True(rpl.IsOwner(), "the server owns the probe");
		ctx.False(rpl.IsProxy(), "the server holds the authority, not a proxy");

		if (!RedProof())
		{
			probe.SendServer();
			probe.SendOwner();
			probe.SendBroadcast();
		}

		ctx.Equal(1, probe.ServerRuns(), "a Server Rpc on a server-owned entity runs here inside the call");
		ctx.Equal(1, probe.OwnerRuns(), "an Owner Rpc on a server-owned entity runs here inside the call");
		ctx.Equal(0, probe.BroadcastRuns(), "a Broadcast Rpc does not run on the server (it goes to proxies only)");
		ctx.Equal(0, probe.LateRuns(), "no handler ran after its Rpc() call returned");

		probe.ReportLaterAndDelete(100);
	}
}
