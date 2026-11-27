// LF_VehicleLotHarness.c -- harness verb `lotsheet`: the car lot and truck yard sheets for a proof or a picture.
//
//   lotsheet open    opens the sheet of the lot nearest the character as the lot's own action does: the client
//                    asks the server, the server runs LF_VehicleLot.Show (after the same counter-reach check
//                    as LF_VehicleBuyAction), and the sheet arrives over the same RPC. Answers when the sheet is
//                    up: `ok lotsheet title=<title> rows=<n> ## Car0=<label> | <status> | <button> ## ...`
//   lotsheet rows    reads the open sheet back the same way (`error not_open` with no sheet).
//   lotsheet close   closes it (`ok closed`).
//
// A truck row reads "<name> (needs truck license)" for a character without it. HARNESS ONLY: the server refuses
// in a build without -scrDefine LF_HARNESS (an administrator may still ask, as for /grid).

//------------------------------------------------------------------------------------------------
//! One `lotsheet open` waiting on the sheet to arrive (client side).
class LF_VehicleLotWait : Managed
{
	int m_iCommandId;
	int m_iTicks;

	//------------------------------------------------------------------------------------------------
	void Poll()
	{
		m_iTicks++;
		string seen = LF_VehicleLotSheet.ReadBack();
		if (!seen.IsEmpty())
		{
			LF_AgentHarness.Answer(m_iCommandId, "ok", "lotsheet " + seen);
			LF_VehicleLotHarness.Forget(this);
			return;
		}

		if (m_iTicks >= LF_VehicleLotHarness.MAX_TICKS)
		{
			LF_AgentHarness.Answer(m_iCommandId, "error", "not_opened (no lot within reach, or the server refused: see [LFDebug:Vehicle])");
			LF_VehicleLotHarness.Forget(this);
		}
	}
}

//------------------------------------------------------------------------------------------------
class LF_VehicleLotHarness
{
	static const string VERB = "lotsheet";
	static const int TICK_MS = 200;	// lf-fixed: harness poll step
	static const int MAX_TICKS = 25;	// lf-fixed: harness wait budget (5 s)

	protected static ref array<ref LF_VehicleLotWait> s_aWaits = {};

	//------------------------------------------------------------------------------------------------
	//! Pure: the sub-command word of a `lotsheet <word>` call, "" when it is not open, rows or close.
	static string ParseWord(string arg)
	{
		if (arg == "open" || arg == "rows" || arg == "close")
			return arg;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: opens the nearest lot's sheet for the player, through the action's own path.
	//! \return "" when it was sent, else the refusal (also logged).
	static string ServerOpen(IEntity character)
	{
		if (!Replication.IsServer() || !character)
			return "no character";

		LF_VehicleLotComponent lot = LF_VehicleLotComponent.Nearest(character.GetOrigin());
		if (!lot)
		{
			LF_Debug.Warn("Vehicle", "lotsheet harness: the world has no lot");
			return "no lot";
		}

		IEntity lotEntity = lot.GetOwner();
		if (!LF_Garage.WithinReach(character, lotEntity))
		{
			LF_Debug.Warn("Vehicle", string.Format("lotsheet harness: the nearest lot (%1) is %2 m away, out of the counter's reach", lot.GetShopId(), Math.Round(vector.Distance(character.GetOrigin(), lotEntity.GetOrigin()))));
			return "out of reach";
		}

		if (!LF_VehicleLot.Show(character, lotEntity))
			return "not sent";

		LF_Debug.Info("Vehicle", string.Format("lotsheet harness: opened the %1 lot sheet", lot.GetShopId()));
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the answer to one `lotsheet` command.
	static void Run(LF_AgentCommand cmd)
	{
		string word = ParseWord(cmd.Arg1);
		if (word.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage_lotsheet_open|rows|close");
			return;
		}

		if (word == "close")
		{
			LF_VehicleLotSheet.Close();
			LF_AgentHarness.Answer(cmd.Id, "ok", "closed");
			return;
		}

		if (word == "rows")
		{
			string seen = LF_VehicleLotSheet.ReadBack();
			if (seen.IsEmpty())
				LF_AgentHarness.Answer(cmd.Id, "error", "not_open");
			else
				LF_AgentHarness.Answer(cmd.Id, "ok", "lotsheet " + seen);

			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return;
		}

		// A sheet already up is closed first, so the answer reads the one this ask opens.
		LF_VehicleLotSheet.Close();
		LF_VehicleLotWait wait = new LF_VehicleLotWait();
		wait.m_iCommandId = cmd.Id;
		s_aWaits.Insert(wait);
		GetGame().GetCallqueue().CallLater(wait.Poll, TICK_MS, true);
		controller.LF_AskLotSheetHarness();
	}

	//------------------------------------------------------------------------------------------------
	static void Forget(LF_VehicleLotWait wait)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(wait.Poll);

		int at = s_aWaits.Find(wait);
		if (at >= 0)
			s_aWaits.Remove(at);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		foreach (LF_VehicleLotWait wait : s_aWaits)
		{
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(wait.Poll);
		}

		s_aWaits.Clear();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the waits and the sheet's read-back (LF_StaticReset finds it by reflection).
class LF_VehicleLotHarness_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleLotHarness";
	}

	override void Apply()
	{
		LF_VehicleLotHarness.ResetStatics();
		LF_VehicleLotSheet.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskLotSheetHarness()
	{
		if (Replication.IsServer())
			RpcAsk_LF_LotSheetHarness();
		else
			Rpc(RpcAsk_LF_LotSheetHarness);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LotSheetHarness()
	{
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(GetPlayerId(), "Vehicle", "lotsheet harness");
		if (!allowed)
		{
			LF_Debug.Warn("Vehicle", "lotsheet harness refused: not a LF_HARNESS build");
			return;
		}

		LF_VehicleLotHarness.ServerOpen(GetControlledEntity());
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != LF_VehicleLotHarness.VERB)
			return super.Handle(cmd);

		LF_VehicleLotHarness.Run(cmd);
		return true;
	}
}
