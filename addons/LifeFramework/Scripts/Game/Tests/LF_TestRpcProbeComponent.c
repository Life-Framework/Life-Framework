//! #2641: a test probe that settles where a bare Rpc() runs when the server calls it. One Rpc per
//! RplRcver kind, each handler counting its runs and whether it ran inside the Rpc() call (a direct
//! call) or later (a routed call that came back). Only the test prefabs Prefabs/Debug/LF_TestRpcProbe*.et
//! carry it; the LF_Test_RpcRouting* tests drive it (docs/architecture/rpc-routing.md).
[ComponentEditorProps(category: "LifeFramework/Tests", description: "Test probe (#2641): one Rpc per RplRcver kind; counts where and when each handler runs. Only test prefabs carry it.")]
class LF_TestRpcProbeComponentClass : ScriptComponentClass
{
}

class LF_TestRpcProbeComponent : ScriptComponent
{
	static const string FEATURE = "RpcProbe";

	protected string m_sLabel = "unlabelled";
	protected int m_iServer;
	protected int m_iOwner;
	protected int m_iBroadcast;
	protected int m_iLate;
	protected bool m_bInCall;

	//------------------------------------------------------------------------------------------------
	void SetLabel(string label)
	{
		m_sLabel = label;
	}

	//------------------------------------------------------------------------------------------------
	int ServerRuns()
	{
		return m_iServer;
	}

	//------------------------------------------------------------------------------------------------
	int OwnerRuns()
	{
		return m_iOwner;
	}

	//------------------------------------------------------------------------------------------------
	int BroadcastRuns()
	{
		return m_iBroadcast;
	}

	//------------------------------------------------------------------------------------------------
	//! Handler runs that happened after the Rpc() call had returned.
	int LateRuns()
	{
		return m_iLate;
	}

	//------------------------------------------------------------------------------------------------
	//! A bare Rpc with RplRcver.Server, no host branch (on purpose: this is what the probe measures).
	void SendServer()
	{
		m_bInCall = true;
		Rpc(RpcAsk_ProbeServer, 1);
		m_bInCall = false;
	}

	//------------------------------------------------------------------------------------------------
	void SendOwner()
	{
		m_bInCall = true;
		Rpc(RpcDo_ProbeOwner, 2);
		m_bInCall = false;
	}

	//------------------------------------------------------------------------------------------------
	void SendBroadcast()
	{
		m_bInCall = true;
		Rpc(RpcDo_ProbeBroadcast, 3);
		m_bInCall = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The red-proof stand-in for a host branch: runs every handler directly, the way a host path does.
	void RunAllHandlersDirectly()
	{
		m_bInCall = true;
		RpcAsk_ProbeServer(1);
		RpcDo_ProbeOwner(2);
		RpcDo_ProbeBroadcast(3);
		m_bInCall = false;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_ProbeServer(int tag)
	{
		m_iServer++;
		Note("Server", tag);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_ProbeOwner(int tag)
	{
		m_iOwner++;
		Note("Owner", tag);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ProbeBroadcast(int tag)
	{
		m_iBroadcast++;
		Note("Broadcast", tag);
	}

	//------------------------------------------------------------------------------------------------
	protected void Note(string kind, int tag)
	{
		string when = "inside the Rpc() call";
		if (!m_bInCall)
		{
			m_iLate++;
			when = "LATE, after the Rpc() call returned";
		}
		LF_Debug.Info(FEATURE, string.Format("%1: %2 handler ran (tag %3) %4", m_sLabel, kind, tag, when));
	}

	//------------------------------------------------------------------------------------------------
	//! Logs the counts after delayMs and then deletes the probe, so a routed call that comes back on a
	//! later frame shows in console.log (`tools\cli logs RpcProbe`) even though the test has finished.
	void ReportLaterAndDelete(int delayMs)
	{
		GetGame().GetCallqueue().CallLater(ReportAndDelete, delayMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReportAndDelete()
	{
		LF_Debug.Info(FEATURE, string.Format("%1: after the delay server=%2 owner=%3 broadcast=%4 late=%5",
			m_sLabel, m_iServer, m_iOwner, m_iBroadcast, m_iLate));
		SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(ReportAndDelete);
		super.OnDelete(owner);
	}
}
