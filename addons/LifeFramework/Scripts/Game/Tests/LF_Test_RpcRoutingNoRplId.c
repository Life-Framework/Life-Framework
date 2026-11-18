// red-proof: 2026-11-17 `--red` (RedProof() runs the handlers directly, as a host branch would): RED-OK core/rpc-routing-no-rplid; red: the three "... with no RplId: dropped" lines (expected 0, got 1) and "Broadcast Rpc, local-only spawn".
//! #2641 case (c): the server calls a bare Rpc() on an entity with no RplId. Observed on build 26266485:
//! - an entity with NO RplComponent: every Rpc (Server, Owner, Broadcast) is dropped, with
//!   `RPL (E): Attempting to send an RPC through unregistred item`; nothing runs, here or anywhere.
//! - the replicated probe prefab spawned local-only (SpawnEntityPrefabLocal) on the server still gets a
//!   valid RplId and routes exactly like case (a): on the server, "local-only" is not "unregistered".
//! Findings: docs/architecture/rpc-routing.md.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RpcRoutingNoRplId : LF_Test
{
	static const ResourceName NO_RPL_PREFAB = "{EB67B516E1C0A23D}Prefabs/Debug/LF_TestRpcProbe_NoRpl.et";

	override string GetName()
	{
		return "core/rpc-routing-no-rplid";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(Replication.IsServer(), "the test boot is the server");

		// c1: no RplComponent on the entity at all.
		IEntity bare = LF_Utils.SpawnEntityPrefab(NO_RPL_PREFAB, "0 0 0");
		LF_TestRpcProbeComponent bareProbe;
		if (bare)
			bareProbe = LF_TestRpcProbeComponent.Cast(bare.FindComponent(LF_TestRpcProbeComponent));
		if (!bareProbe)
			ctx.Fail("the no-RplComponent probe did not spawn with its probe component: " + NO_RPL_PREFAB);
		else
		{
			bareProbe.SetLabel("no-rplcomponent");
			ctx.True(bare.FindComponent(RplComponent) == null, "the bare probe has no RplComponent");
			ctx.False(Replication.FindItemId(bareProbe).IsValid(), "the bare probe component has no RplId");
			Exercise(bareProbe);
			ctx.Equal(0, bareProbe.ServerRuns(), "Server Rpc with no RplId: dropped, does not run here");
			ctx.Equal(0, bareProbe.OwnerRuns(), "Owner Rpc with no RplId: dropped, does not run here");
			ctx.Equal(0, bareProbe.BroadcastRuns(), "Broadcast Rpc with no RplId: dropped, does not run here");
			bareProbe.ReportLaterAndDelete(100);
		}

		// c2: RplComponent present, spawned local-only on the server.
		IEntity local = LF_Utils.SpawnEntityPrefab(LF_Test_RpcRoutingOwned.PROBE_PREFAB, "0 0 0", "0 0 0", false);
		LF_TestRpcProbeComponent localProbe;
		if (local)
			localProbe = LF_TestRpcProbeComponent.Cast(local.FindComponent(LF_TestRpcProbeComponent));
		if (!localProbe)
			ctx.Fail("the local-only probe did not spawn with its probe component");
		else
		{
			localProbe.SetLabel("local-only");
			RplComponent rpl = RplComponent.Cast(local.FindComponent(RplComponent));
			ctx.True(rpl && rpl.Id().IsValid(), "a local-only spawn on the server still gets an RplId");
			Exercise(localProbe);
			ctx.Equal(1, localProbe.ServerRuns(), "Server Rpc, local-only spawn: runs here inside the call");
			ctx.Equal(1, localProbe.OwnerRuns(), "Owner Rpc, local-only spawn: runs here inside the call");
			ctx.Equal(0, localProbe.BroadcastRuns(), "Broadcast Rpc, local-only spawn: does not run here");
			ctx.Equal(0, localProbe.LateRuns(), "no handler ran after its Rpc() call returned (local-only)");
			localProbe.ReportLaterAndDelete(100);
		}
	}

	protected void Exercise(LF_TestRpcProbeComponent probe)
	{
		if (RedProof())
		{
			probe.RunAllHandlersDirectly();
			return;
		}
		probe.SendServer();
		probe.SendOwner();
		probe.SendBroadcast();
	}
}
