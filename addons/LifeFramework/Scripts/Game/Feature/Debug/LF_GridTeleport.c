//------------------------------------------------------------------------------------------------
//! Grid teleport (#1813, epic #1802): `/grid <slug>` moves the caller to a grid cell's spawn point,
//! and the harness verb `teleport grid:<slug>` sends the same request. A cell spawn is any entity
//! carrying an LF_PlaceComponent of kind GridCell whose id is the cell's slug (`tools\cli grid place`
//! writes one at each cell's gap, `node tools/grid/cell-spawn.mjs --backfill` adds it to old cells,
//! and the frame has one at the gate). The teleport finds the spawn through LF_Places, never by a
//! coordinate or an entity name, so it works on whatever map places the components and does nothing
//! in a world with no grid cells.
//!
//! The client sends only the slug. The server checks the dev gate (development worlds and harness
//! builds, or an administrator), finds the place and moves the body; an unknown slug, a world with
//! no grid cells or a caller without a body logs LF_Debug.Warn and moves nothing.
//------------------------------------------------------------------------------------------------
enum LF_EGridTeleport
{
	OK,
	NOT_ALLOWED,
	NO_BODY,
	NO_GRID,
	UNKNOWN_CELL
}

//------------------------------------------------------------------------------------------------
class LF_GridTeleport
{
	static const string FEATURE = "GridTeleport";
	//! The LF_PlaceComponent kind of a cell spawn; the id is the cell's slug.
	static const string KIND = "GridCell";
	//! A body is set this far above the ground so it does not start inside it.
	static const float LIFT = 0.2; // lf-fixed: ground clearance for a teleported body, not an owner knob

	//! How many refusals this process has logged. A test reads the difference around a request.
	protected static int s_iWarnings;

	//------------------------------------------------------------------------------------------------
	static int WarningCount()
	{
		return s_iWarnings;
	}

	//------------------------------------------------------------------------------------------------
	//! The slug as typed: trimmed and lower-case (slugs are lower-case letters, digits and dashes).
	static string Clean(string slug)
	{
		string clean = slug.Trim();
		clean.ToLower();
		return clean;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when this world places at least one grid cell spawn.
	static bool WorldHasCells()
	{
		array<IEntity> cells = {};
		return LF_Places.FindAll(KIND, cells) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where a body stands for this cell: the spawn's X and Z on the ground there. False
	//! when no spawn is placed for the slug.
	static bool FindDestination(string slug, out vector destination)
	{
		vector origin;
		if (!LF_Places.FindOrigin(KIND, slug, origin))
			return false;

		float groundY = 0;
		if (GetGame() && GetGame().GetWorld())
			groundY = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);

		destination = Vector(origin[0], groundY + LIFT, origin[2]);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Validates and performs one teleport for a body.
	//! \param allowed The dev gate's answer (a parameter so a test drives both sides).
	//! \return The check result; the body moved only for OK.
	static LF_EGridTeleport Serve(IEntity body, string slug, bool allowed)
	{
		string clean = Clean(slug);
		if (!allowed)
			return Refuse(LF_EGridTeleport.NOT_ALLOWED, string.Format("grid '%1' refused: dev tools are off for this player", clean));

		if (!body)
			return Refuse(LF_EGridTeleport.NO_BODY, string.Format("grid '%1' refused: the caller has no body", clean));

		if (!WorldHasCells())
			return Refuse(LF_EGridTeleport.NO_GRID, string.Format("grid '%1' refused: this world has no grid cells", clean));

		vector destination;
		if (!FindDestination(clean, destination))
			return Refuse(LF_EGridTeleport.UNKNOWN_CELL, string.Format("grid '%1' refused: no such grid cell (a cell is its slug, tools\\cli grid list)", clean));

		LF_Jail.MoveBody(body, destination);
		LF_Debug.Info(FEATURE, string.Format("moved a player to grid cell '%1' at %2", clean, destination));
		return LF_EGridTeleport.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one request from a player.
	//! \param allowed The dev gate's answer for that player (the RPC checks it).
	static LF_EGridTeleport ServerRun(int playerId, string slug, bool allowed)
	{
		if (!Replication.IsServer())
			return LF_EGridTeleport.NOT_ALLOWED;

		IEntity body = null;
		if (GetGame() && GetGame().GetPlayerManager())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		return Serve(body, slug, allowed);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EGridTeleport Refuse(LF_EGridTeleport result, string message)
	{
		s_iWarnings++;
		LF_Debug.Warn(FEATURE, message);
		return result;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("grid");
		if (invoker)
			invoker.Insert(LF_OnGridCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnGridCommand(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_GridTeleport.FEATURE, "/grid not sent: no local player controller");
			return;
		}

		controller.LF_AskGridTeleport(data);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to move this player to a grid cell's spawn. The player owns their controller,
	//! so the RPC is allowed; the server checks the gate and the slug.
	void LF_AskGridTeleport(string slug)
	{
		if (Replication.IsServer())
			RpcAsk_LF_GridTeleport(slug);
		else
			Rpc(RpcAsk_LF_GridTeleport, slug);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_GridTeleport(string slug)
	{
		// Development worlds and harness builds, or an administrator (the same gate as /licence).
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(GetPlayerId(), LF_GridTeleport.FEATURE, "/grid");
		LF_GridTeleport.ServerRun(GetPlayerId(), slug, allowed);
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `teleport grid:<slug>`: this client sends what /grid sends. The server's dev gate and
//! the LF_Places lookup decide; the verdict is the `[LFDebug:GridTeleport]` line and the player's
//! position in `state`.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "teleport")
			return super.Handle(cmd);

		string prefix = "grid:";
		if (!cmd.Arg1.StartsWith(prefix))
			return super.Handle(cmd);

		string slug = cmd.Arg1.Substring(prefix.Length(), cmd.Arg1.Length() - prefix.Length());
		if (slug.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "need_grid_slug");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		controller.LF_AskGridTeleport(slug);
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked grid:" + slug);
		return true;
	}
}
