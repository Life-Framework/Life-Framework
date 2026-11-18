//------------------------------------------------------------------------------------------------
//! RPC string probe (#2674): how long a single string RPC argument can be before it is cut or lost.
//! Harness `rpcstring <chars> down|up` on a client:
//!   down: the client asks the server (an int), the server builds <chars> characters and sends them
//!         back in one owner RPC string; the client answers `down sent=N got=M hash=ok|bad`.
//!   up:   the client builds <chars> characters and sends them in one server RPC string; the server
//!         answers with what arrived and the client answers `up sent=N got=M hash=ok|bad`.
//! A string that never arrives leaves the harness command to time out: the proof reports it as lost.
//! Dev gate only (harness builds and development worlds, the same gate as /licence).
//! The measurement is in docs/architecture/enforce-string-limits.md; agent-box/proofs/rpc-string-proof.mjs drives it.
class LF_RpcStringProbe
{
	static const string FEATURE = "RpcProbe";
	//! Upper bound on one probe, so the verb cannot build an unbounded string.
	static const int MAX_CHARS = 262144; // lf-fixed: a dev probe bound, not a tunable
	protected static const string PATTERN = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";

	//! The harness command waiting for a reply on this client; 0 when none.
	protected static int s_iPendingId;
	protected static string s_sPendingDirection;
	protected static int s_iPendingChars;

	//------------------------------------------------------------------------------------------------
	//! A string of exactly chars characters. Built from pieces of 64 with `+=`, which never cuts
	//! (enforce-string-limits.md); the doubling keeps a 256 KB build to a dozen joins.
	static string Build(int chars)
	{
		string result;
		string piece = PATTERN;
		int left = chars;
		while (left >= PATTERN.Length())
		{
			if (piece.Length() > left)
				piece = PATTERN;

			result += piece;
			left -= piece.Length();
			if (piece.Length() * 2 <= left)
				piece += piece;
		}

		if (left > 0)
			result += PATTERN.Substring(0, left); // lf-bounded: Substring of the 64-char PATTERN, at most 63 chars; the long result grows by +=, which never cuts

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! One result line: what was sent against what arrived.
	static string Report(string direction, int sent, int got, bool hashOk)
	{
		string hash = "bad";
		if (hashOk)
			hash = "ok";

		return string.Format("%1 sent=%2 got=%3 hash=%4", direction, sent, got, hash);
	}

	//------------------------------------------------------------------------------------------------
	static void SetPending(int id, string direction, int chars)
	{
		s_iPendingId = id;
		s_sPendingDirection = direction;
		s_iPendingChars = chars;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the reply arrived; answers the waiting harness command. A late reply to an earlier,
	//! timed-out probe only logs, so it never answers the next probe's command.
	static void Finish(string direction, int sent, int got, bool hashOk)
	{
		string line = Report(direction, sent, got, hashOk);
		LF_Debug.Info(FEATURE, "client " + line);
		if (s_iPendingId <= 0 || direction != s_sPendingDirection || sent != s_iPendingChars)
			return;

		int id = s_iPendingId;
		s_iPendingId = 0;
		LF_AgentHarness.Answer(id, "ok", line);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iPendingId = 0;
		s_sPendingDirection = string.Empty;
		s_iPendingChars = 0;
	}
};

//------------------------------------------------------------------------------------------------
class LF_RpcStringProbe_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RpcStringProbe";
	}

	override void Apply()
	{
		LF_RpcStringProbe.ResetStatics();
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server for chars characters in one owner RPC.
	void LF_AskRpcProbeDown(int chars)
	{
		if (Replication.IsServer())
			RpcAsk_LF_RpcProbeDown(chars);
		else
			Rpc(RpcAsk_LF_RpcProbeDown, chars);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: sends chars characters to the server in one RPC.
	void LF_SendRpcProbeUp(int chars)
	{
		string payload = LF_RpcStringProbe.Build(chars);
		LF_Debug.Info(LF_RpcStringProbe.FEATURE, string.Format("client sends up %1 chars", payload.Length()));
		if (Replication.IsServer())
			RpcAsk_LF_RpcProbeUp(payload, payload.Length(), payload.Hash());
		else
			Rpc(RpcAsk_LF_RpcProbeUp, payload, payload.Length(), payload.Hash());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_RpcProbeDown(int chars)
	{
		if (!LF_DevGate.HarnessBuild() && !LF_DevGate.CheckPlayerRequest(GetPlayerId(), LF_RpcStringProbe.FEATURE, "rpcstring"))
			return;

		if (chars < 1 || chars > LF_RpcStringProbe.MAX_CHARS)
		{
			LF_Debug.Warn(LF_RpcStringProbe.FEATURE, string.Format("refused: %1 chars is outside 1 to %2", chars, LF_RpcStringProbe.MAX_CHARS));
			return;
		}

		string payload = LF_RpcStringProbe.Build(chars);
		LF_Debug.Info(LF_RpcStringProbe.FEATURE, string.Format("server sends down %1 chars to player %2", payload.Length(), GetPlayerId()));
		if (Replication.IsServer() && GetGame().GetPlayerController() == this)
			RpcDo_LF_RpcProbeDown(payload, payload.Length(), payload.Hash());
		else
			Rpc(RpcDo_LF_RpcProbeDown, payload, payload.Length(), payload.Hash());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RpcProbeDown(string payload, int sentChars, int sentHash)
	{
		LF_RpcStringProbe.Finish("down", sentChars, payload.Length(), payload.Hash() == sentHash);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_RpcProbeUp(string payload, int sentChars, int sentHash)
	{
		if (!LF_DevGate.HarnessBuild() && !LF_DevGate.CheckPlayerRequest(GetPlayerId(), LF_RpcStringProbe.FEATURE, "rpcstring"))
			return;

		bool hashOk = payload.Hash() == sentHash;
		LF_Debug.Info(LF_RpcStringProbe.FEATURE, "server got " + LF_RpcStringProbe.Report("up", sentChars, payload.Length(), hashOk));
		if (Replication.IsServer() && GetGame().GetPlayerController() == this)
			RpcDo_LF_RpcProbeUpResult(sentChars, payload.Length(), hashOk);
		else
			Rpc(RpcDo_LF_RpcProbeUpResult, sentChars, payload.Length(), hashOk);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RpcProbeUpResult(int sentChars, int gotChars, bool hashOk)
	{
		LF_RpcStringProbe.Finish("up", sentChars, gotChars, hashOk);
	}
};

//------------------------------------------------------------------------------------------------
//! Harness `rpcstring <chars> down|up` (#2674). The answer comes when the reply RPC arrives.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "rpcstring")
			return super.Handle(cmd);

		int chars = cmd.Arg1.ToInt();
		if (chars < 1 || chars > LF_RpcStringProbe.MAX_CHARS)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "chars_1_to_262144");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		if (cmd.Arg2 == "up")
		{
			LF_RpcStringProbe.SetPending(cmd.Id, "up", chars);
			controller.LF_SendRpcProbeUp(chars);
		}
		else if (cmd.Arg2 == "down" || cmd.Arg2.IsEmpty())
		{
			LF_RpcStringProbe.SetPending(cmd.Id, "down", chars);
			controller.LF_AskRpcProbeDown(chars);
		}
		else
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "direction_down_or_up");
		}

		return true;
	}
};
