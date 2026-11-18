//------------------------------------------------------------------------------------------------
//! Harness stand-in for pictures of a character in motion (the parcel hold proof):
//!   walk <seconds> [speed]   this client's character walks straight ahead (held forward key, like W;
//!                            the speed word is accepted and ignored) for that many seconds, then stops
//! It runs on the client that owns the character; the harness poll itself is behind the dev gate.
//! The movement input is given at the end of each frame's own input (OnPrepareControls below), so
//! the player's absent keys do not cancel it. A hold that only looks right standing still is a bad
//! hold; the pictures during a walk show it.
//------------------------------------------------------------------------------------------------
class LF_WalkHarness
{
	static const string FEATURE = "AgentHarness";
	static const int MAX_SECONDS = 30; // lf-fixed: harness guard

	protected static float s_fEndMs; // lf-float: world time in ms for a harness walk, not a saved value
	protected static float s_fSpeed;

	//------------------------------------------------------------------------------------------------
	static string Start(float seconds, float speed)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "error no_character";

		if (seconds <= 0 || seconds > MAX_SECONDS)
			return "error seconds_out_of_range";

		if (speed <= 0)
			speed = 1;

		s_fEndMs = GetGame().GetWorld().GetWorldTime() + seconds * 1000;
		s_fSpeed = speed;
		LF_Debug.Info(FEATURE, string.Format("walk %1 s at speed %2", seconds, speed));
		return string.Format("ok walking %1 s", seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! The speed to give the movement input this frame, 0 when no walk is running.
	static float Speed()
	{
		if (s_fEndMs <= 0)
			return 0;

		if (GetGame().GetWorld().GetWorldTime() >= s_fEndMs)
		{
			s_fEndMs = 0;
			return 0;
		}

		return s_fSpeed;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterControllerComponent
{
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		// Held forward like the W key (vanilla's own SCR_CharacterControllerComponent_Test does the same).
		if (player && am && LF_WalkHarness.Speed() > 0)
			am.SetActionValue("CharacterForward", 1);

		super.OnPrepareControls(owner, am, dt, player);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "walk")
			return super.Handle(cmd);

		float speed = 1;
		if (!cmd.Arg2.IsEmpty())
			speed = cmd.Arg2.ToFloat();

		string result = LF_WalkHarness.Start(cmd.Arg1.ToFloat(), speed);
		if (result.StartsWith("ok "))
			LF_AgentHarness.Answer(cmd.Id, "ok", result.Substring(3, result.Length() - 3));
		else
			LF_AgentHarness.Answer(cmd.Id, "error", result.Substring(6, result.Length() - 6));

		return true;
	}
}
