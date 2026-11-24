//------------------------------------------------------------------------------------------------
//! Harness stand-in for driving one car up behind another (#4425 proof: run a plate on a car ahead).
//! The harness cannot drive, so a proof that needs a stolen car in front of a police car sets it there:
//!   parkat <EntityName> [metres]   (server, behind the harness gate) sets the sending player's newest
//!                                  car in the world that many metres (default 8) ahead of the placed
//!                                  entity, facing the way it faces; replies `ok parkat plate=<plate>`
//! `park` does the same beside the nearest garage. HARNESS ONLY: Run refuses in any build without
//! -scrDefine LF_HARNESS, on top of the gate the RPC already passed (LF_DevGate.CheckHarnessRequest).
//------------------------------------------------------------------------------------------------
class LF_ParkAtHarness
{
	static const string FEATURE = "AgentHarness";
	static const string VERB = "parkat";
	static const string STOLEN_VERB = "markstolen";
	static const float DEFAULT_METRES = 8;	// lf-fixed: inside the run-plate reach (30 m), clear of a car length

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a harness tool string is this verb.
	static bool IsVerb(string tool)
	{
		return tool.StartsWith(VERB + " ") || tool == STOLEN_VERB || tool.StartsWith(STOLEN_VERB + " ");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `markstolen` marks the sending player's newest car in the world stolen (what a hotwire does, #412),
	//! so a proof tests the STOLEN read of the report and Run plate on every run instead of on a hotwire roll.
	//! \return "ok markstolen plate=<plate>" or "error <reason>"
	static string RunMarkStolen(IEntity character)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "markstolen refused: not a LF_HARNESS build");
			return "error harness_only";
		}

		string vehicleId = LF_VehicleOwnership.LatestInWorld(LF_Utils.GetPlayerUid(character));
		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!vehicle)
			return "error no_vehicle";

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return "error no_vehicle_lock";

		vehicleLock.SetStolen(true);
		string plate = LF_VehicleOwnership.GetPlate(vehicleId);
		LF_Debug.Info(FEATURE, string.Format("markstolen: %1 marked stolen (stand-in for a hotwire)", plate));
		return "ok markstolen plate=" + plate;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the sending player's newest car ahead of the named entity.
	//! \return "ok ..." or "error <reason>"
	static string Run(IEntity character, string tool)
	{
		if (tool == STOLEN_VERB || tool.StartsWith(STOLEN_VERB + " "))
			return RunMarkStolen(character);

		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "parkat refused: not a LF_HARNESS build");
			return "error harness_only";
		}

		array<string> words = {};
		tool.Split(" ", words, true);
		if (words.Count() < 2)
			return "error usage_parkat_entity_metres";

		IEntity anchor = GetGame().GetWorld().FindEntityByName(words[1]);
		if (!anchor)
			return "error no_entity " + words[1];

		float metres = DEFAULT_METRES;
		if (words.Count() > 2)
			metres = words[2].ToFloat();

		string vehicleId = LF_VehicleOwnership.LatestInWorld(LF_Utils.GetPlayerUid(character));
		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!vehicle)
			return "error no_vehicle";

		vector previous = vehicle.GetOrigin();
		vector transform[4];
		anchor.GetWorldTransform(transform);
		vector spot = transform[3] + transform[2] * metres;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 0.5;
		transform[3] = spot;
		vehicle.SetWorldTransform(transform);
		vehicle.Update();
		// A replicated entity moved far on the server stays where it was for the clients unless its node is told.
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			rpl.ForceNodeMovement(previous);

		Physics physics = vehicle.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
			physics.SetActive(ActiveState.ACTIVE);
		}

		string plate = LF_VehicleOwnership.GetPlate(vehicleId);
		LF_Debug.Info(FEATURE, string.Format("parkat: %1 set %2 m ahead of %3", plate, metres, words[1]));
		return "ok parkat plate=" + plate;
	}
}

//------------------------------------------------------------------------------------------------
//! Client side of `parkat`: hands the words to the sending character's server request.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != LF_ParkAtHarness.VERB && cmd.Verb != LF_ParkAtHarness.STOLEN_VERB)
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		if (cmd.Verb == LF_ParkAtHarness.STOLEN_VERB)
		{
			character.LF_AskHarnessScanner(cmd.Id, LF_ParkAtHarness.STOLEN_VERB);
			return true;
		}

		string tool = LF_ParkAtHarness.VERB + " " + cmd.Arg1;
		if (!cmd.Arg2.IsEmpty())
			tool = tool + " " + cmd.Arg2;

		character.LF_AskHarnessScanner(cmd.Id, tool);
		return true;
	}
}
