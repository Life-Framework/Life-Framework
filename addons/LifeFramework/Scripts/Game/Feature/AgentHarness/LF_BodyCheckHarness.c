// Harness verb `bodycheck` (proof-preconditions-2, 2026-11-28): what a proof's start-of-run preconditions read and
// repair on THIS client's own character, the parts `state` (read on the server) cannot see:
//   bodycheck                 `camera=<class> stance=<STAND|CROUCH|PRONE> seat=<0|1> weight=<kg> maxload=<kg>`
//   bodycheck fix camera      hand the view back to the player camera (a dialogue, film or clean-shot camera left on)
//   bodycheck fix stand       stand a living, non-downed body up
//   bodycheck fix leave       get out of the vehicle the body sits in
// The proofs (agent-box/proofs/_preconditions.mjs) decide from the reply and call the fix; each fix answers `ok ...`
// or `error <why>`. Dev tooling only: it changes nothing a player can do, and it is a modded verb behind the same
// harness gate as every other verb.

//------------------------------------------------------------------------------------------------
class LF_BodyCheckHarness
{
	//------------------------------------------------------------------------------------------------
	//! The class name of the camera this client looks through, "none" without one.
	static string CameraName()
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras)
			return "none";

		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return "none";

		return camera.Type().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! The read line, or "no_character".
	static string Read()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "no_character";

		string stance = "none";
		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller)
			stance = typename.EnumToString(ECharacterStance, controller.GetStance());

		int seated = 0;
		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (access && access.GetVehicleIn(character))
			seated = 1;

		float weight = 0;
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			weight = inventory.GetTotalWeightOfAllStorages();

		float maxLoad = 0;
		SCR_CharacterInventoryStorageComponent storage = LF_Component<SCR_CharacterInventoryStorageComponent>.Find(character);
		if (storage)
			maxLoad = storage.GetMaxLoad();

		return string.Format("camera=%1 stance=%2 seat=%3 weight=%4 maxload=%5", CameraName(), stance, seated, Math.Round(weight * 10) / 10, Math.Round(maxLoad * 10) / 10);
	}

	//------------------------------------------------------------------------------------------------
	//! A reply `ok ...` or `error <why>`.
	static string Fix(string what)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "error no_character";

		if (what == "camera")
		{
			CameraManager cameras = GetGame().GetCameraManager();
			PlayerController player = GetGame().GetPlayerController();
			if (!cameras || !player)
				return "error no_camera_manager";

			CameraBase own = player.GetPlayerCamera();
			if (!own)
				return "error no_player_camera";

			CameraBase now = cameras.CurrentCamera();
			if (now == own)
				return "ok camera already " + CameraName();

			cameras.SetCamera(own);
			return "ok camera was " + now.Type().ToString() + " now " + CameraName();
		}

		if (what == "stand")
		{
			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller)
				return "error no_controller";

			if (controller.GetLifeState() != ECharacterLifeState.ALIVE || character.LF_IsDowned())
				return "error not_standing_material";

			controller.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);
			return "ok stance change to erect asked";
		}

		if (what == "leave")
		{
			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access || !access.GetVehicleIn(character))
				return "ok not seated";

			if (access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.CLOSE_DOOR, false))
				return "ok left the vehicle";

			return "error leave_refused";
		}

		return "error unknown_fix " + what;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "bodycheck")
			return super.Handle(cmd);

		string reply;
		if (cmd.Arg1 == "fix")
			reply = LF_BodyCheckHarness.Fix(cmd.Arg2);
		else
			reply = LF_BodyCheckHarness.Read();

		LF_Debug.Info("BodyCheck", string.Format("bodycheck %1 %2: %3", cmd.Arg1, cmd.Arg2, reply));
		if (reply.StartsWith("ok "))
			LF_AgentHarness.Answer(cmd.Id, "ok", reply.Substring(3, reply.Length() - 3)); // lf-bounded: a short reply
		else if (reply.StartsWith("error "))
			LF_AgentHarness.Answer(cmd.Id, "error", reply.Substring(6, reply.Length() - 6)); // lf-bounded: a short reply
		else if (reply == "no_character")
			LF_AgentHarness.Answer(cmd.Id, "error", reply);
		else
			LF_AgentHarness.Answer(cmd.Id, "ok", reply);
		return true;
	}
}
