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

	// `walk to x,z`: a walk to a spot.
	static const float TO_MAX_SECONDS = 300; // lf-fixed: harness guard
	static const float TO_TOL_M = 6; // lf-fixed: harness geometry
	static const float TO_STILL_MS = 1500; // lf-fixed: harness tuning
	static const float TO_STILL_M = 0.6; // lf-fixed: harness geometry
	static const float TO_SIDE_MS = 2500; // lf-fixed: harness tuning
	protected static string s_sTo = "idle";
	protected static vector s_vTo;
	protected static float s_fToTol;
	protected static float s_fToSideDeg; // the turn off the straight line while a tree or rock is rounded
	protected static float s_fToSideUntilMs; // lf-float: world time in ms for a harness walk, not a saved value
	protected static float s_fToCheckMs; // lf-float: world time in ms for a harness walk, not a saved value
	protected static vector s_vToCheck;
	protected static int s_iToSides;
	protected static float s_fToEndMs; // lf-float: world time in ms for a harness walk, not a saved value

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

	//------------------------------------------------------------------------------------------------
	//! `walk to x,z [seconds] [tol]`: this client's character walks to that spot as a player does: turns to it, holds
	//! forward (sprint while far), stops within `tol` metres (default 6) or when the seconds (default 120, at most
	//! TO_MAX_SECONDS) run out. A drive that ended short of its goal finishes on foot with it (the tour's off-road tail).
	static string StartTo(string spot, float seconds, float tol)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "error no_character";

		array<string> parts = {};
		spot.Split(",", parts, true);
		if (parts.Count() != 2)
			return "error spot_is_x,z";

		if (seconds <= 0)
			seconds = 120;

		seconds = Math.Min(seconds, TO_MAX_SECONDS); // lf-float: a walk budget in seconds, never large
		if (tol <= 0)
			tol = TO_TOL_M;

		s_vTo = Vector(parts[0].ToFloat(), 0, parts[1].ToFloat());
		s_fToTol = tol;
		s_fToEndMs = GetGame().GetWorld().GetWorldTime() + seconds * 1000;
		s_sTo = "walking";
		s_fToSideDeg = 0;
		s_fToSideUntilMs = 0;
		s_fToCheckMs = 0;
		s_iToSides = 0;
		LF_Debug.Info(FEATURE, string.Format("walk to %1,%2 within %3 m, %4 s: from %5, %6 m away", s_vTo[0], s_vTo[2], tol, seconds, character.GetOrigin(), Math.Round(vector.DistanceXZ(character.GetOrigin(), s_vTo))));
		return string.Format("ok walking to %1,%2 left=%3", s_vTo[0], s_vTo[2], Math.Round(vector.DistanceXZ(character.GetOrigin(), s_vTo)));
	}

	//------------------------------------------------------------------------------------------------
	//! `walk state`: idle, walking, arrived or timeout, and the metres left.
	static string StateLine()
	{
		string line = "ok walk=" + s_sTo;
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (character && s_sTo != "idle")
			line += string.Format(" left=%1 at=%2,%3", Math.Round(vector.DistanceXZ(character.GetOrigin(), s_vTo)), Math.Round(character.GetOrigin()[0]), Math.Round(character.GetOrigin()[2]));

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! One frame of the walk to a spot: the aiming yaw to it and the movement input. True while it walks.
	static bool ToTick(IEntity owner, CharacterInputContext context, ActionManager am)
	{
		if (s_sTo != "walking" || !owner || !context)
			return false;

		vector at = owner.GetOrigin();
		float left = vector.DistanceXZ(at, s_vTo);
		if (left <= s_fToTol)
		{
			s_sTo = "arrived";
			LF_Debug.Info(FEATURE, string.Format("walk to %1,%2 arrived: %3 m left", s_vTo[0], s_vTo[2], Math.Round(left)));
			return false;
		}

		if (GetGame().GetWorld().GetWorldTime() >= s_fToEndMs)
		{
			s_sTo = "timeout";
			LF_Debug.Warn(FEATURE, string.Format("walk to %1,%2 timed out %3 m short at %4", s_vTo[0], s_vTo[2], Math.Round(left), at));
			return false;
		}

		// Not moving for TO_STILL_MS (a trunk, a boulder, a fence): turn off the line for TO_SIDE_MS, the other side next time.
		float now = GetGame().GetWorld().GetWorldTime();
		if (s_fToCheckMs <= 0 || now - s_fToCheckMs > TO_STILL_MS)
		{
			if (s_fToCheckMs > 0 && now > s_fToSideUntilMs && vector.DistanceXZ(at, s_vToCheck) < TO_STILL_M)
			{
				s_iToSides++;
				int widen = s_iToSides % 3;
				int flip = s_iToSides % 2;
				s_fToSideDeg = 70 + 20 * widen;
				if (flip == 0)
					s_fToSideDeg = -s_fToSideDeg;

				s_fToSideUntilMs = now + TO_SIDE_MS;
				LF_Debug.Info(FEATURE, string.Format("walk to %1,%2 held at %3: turning %4 degrees for a while", s_vTo[0], s_vTo[2], at, s_fToSideDeg));
			}

			s_fToCheckMs = now;
			s_vToCheck = at;
		}

		float yaw = Math.Atan2(s_vTo[0] - at[0], s_vTo[2] - at[2]);
		if (now < s_fToSideUntilMs)
			yaw += s_fToSideDeg * Math.DEG2RAD;

		while (yaw < 0)
			yaw += Math.PI2;

		while (yaw >= Math.PI2)
			yaw -= Math.PI2;

		context.SetHeadingAngle(yaw);
		vector aim = context.GetAimingAngles();
		aim[0] = 0;
		aim[1] = 0;
		context.SetAimingAngles(aim);
		am.SetActionValue("CharacterForward", 1);
		if (left > 15)
			am.SetActionValue("CharacterSprint", 1);

		return true;
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

		if (player && am)
			LF_WalkHarness.ToTick(owner, GetInputContext(), am);

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

		string result;
		if (cmd.Arg1 == "to")
		{
			result = LF_WalkHarness.StartTo(cmd.Arg2, cmd.Arg3.ToFloat(), cmd.Arg4.ToFloat());
		}
		else if (cmd.Arg1 == "state")
		{
			result = LF_WalkHarness.StateLine();
		}
		else
		{
			float speed = 1;
			if (!cmd.Arg2.IsEmpty())
				speed = cmd.Arg2.ToFloat();

			result = LF_WalkHarness.Start(cmd.Arg1.ToFloat(), speed);
		}

		if (result.StartsWith("ok "))
			LF_AgentHarness.Answer(cmd.Id, "ok", result.Substring(3, result.Length() - 3));
		else
			LF_AgentHarness.Answer(cmd.Id, "error", result.Substring(6, result.Length() - 6));

		return true;
	}
}
