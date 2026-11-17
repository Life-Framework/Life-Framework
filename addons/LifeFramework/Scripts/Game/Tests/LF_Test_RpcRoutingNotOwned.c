// red-proof: 2026-11-17 `--red` (RedProof() runs the handlers directly, as a host branch would): RED-OK core/rpc-routing-not-owned; the Broadcast-on-the-server assertion went red (expected 0, got 1).
// tier: WORLD
//! #2641 case (b): the server calls a bare Rpc() on a replicated entity it does not own. The suite boot has
//! no client, so ownership goes to RplIdentity.Invalid(), an owner with no connection. Observed on build
//! 26266485: Server runs here, Owner ALSO runs here (an owner with no connection does not take the call
//! away), Broadcast does not. IsOwner() after that Give is not stable (false on one run, true on the next),
//! so the test logs it instead of asserting it. A real remote owner (a joined client's body)
//! is the engine table's "routed to the owner client" case and needs a two-process session to observe.
//! Findings: docs/architecture/rpc-routing.md.
class LF_Test_RpcRoutingNotOwned : LF_Test
{
	override string GetName()
	{
		return "core/rpc-routing-not-owned";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(LF_Test_RpcRoutingOwned.PROBE_PREFAB, "0 0 0");
		if (!entity)
		{
			ctx.Fail("the probe prefab did not spawn: " + LF_Test_RpcRoutingOwned.PROBE_PREFAB);
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
		probe.SetLabel("not-owned");

		rpl.Give(RplIdentity.Invalid());
		ctx.True(Replication.IsServer(), "the test boot is the server");
		ctx.True(rpl.Id().IsValid(), "the probe is registered for replication (valid RplId)");
		// Not asserted: IsOwner() after Give(Invalid) read false on one run and true on the next (build 26266485).
		LF_Debug.Info(LF_TestRpcProbeComponent.FEATURE, string.Format("not-owned: after Give(Invalid) IsOwner=%1 FindOwner valid=%2",
			rpl.IsOwner(), Replication.FindOwner(rpl.Id()).IsValid()));
		ctx.False(rpl.IsProxy(), "the server still holds the authority");

		if (RedProof())
			probe.RunAllHandlersDirectly();
		else
		{
			probe.SendServer();
			probe.SendOwner();
			probe.SendBroadcast();
		}

		ctx.Equal(1, probe.ServerRuns(), "a Server Rpc runs here even when the server does not own the entity");
		ctx.Equal(1, probe.OwnerRuns(), "an Owner Rpc runs here when the owner has no connection (RplIdentity.Invalid)");
		ctx.Equal(0, probe.BroadcastRuns(), "a Broadcast Rpc does not run on the server (not-owned)");
		ctx.Equal(0, probe.LateRuns(), "no handler ran after its Rpc() call returned");

		probe.ReportLaterAndDelete(100);
	}
}
