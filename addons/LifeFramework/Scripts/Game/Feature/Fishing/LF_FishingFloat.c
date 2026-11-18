//------------------------------------------------------------------------------------------------
//! The float (#2995, docs/architecture/fishing.md section 3): a small red-and-white bobber on the water at
//! the cast point, drawn only on the owner's machine. It is a local entity with no RplComponent, so nothing
//! per cast is broadcast; the server never knows it exists. `LF_FishingRodComponent.OnServerEvent` drives it
//! from the events the session already sends: CAST_OK shows it, BITE makes it dip and plays the cue, any
//! other event (caught, missed, ended) takes it away. The events only ever reach the owner's machine, so a
//! dedicated server never shows one (and plays no cue).
//!
//! The prefab is the Blender float (Assets/Items/Fishing/FishingFloat) at three times its modelled size, so a
//! 10 cm bobber still reads as a red-and-white mark 3 to 15 m out on a flat screen.
//!
//! The cue is the base game's UI click (a reel click) until the sound pipeline supplies a splash.
//------------------------------------------------------------------------------------------------
class LF_FishingFloat
{
	static const ResourceName PREFAB = "{E512C323AE69A7EF}Prefabs/Systems/Fishing/LF_FishingFloat.et";
	static const float REST_ABOVE_WATER_METRES = 0.05;	// lf-fixed: the model's origin is its middle (x3 scale), the white and red meet at the waterline
	static const float DIP_DEPTH_METRES = 0.15;			// lf-fixed: how far the bobber is pulled under, large enough to see from the bank
	static const int BOB_INTERVAL_MS = 130;				// lf-fixed: one step of the dip animation

	protected static IEntity s_Float;
	protected static float s_fRestY;
	protected static bool s_bDipping;
	protected static int s_iBobStep;

	//------------------------------------------------------------------------------------------------
	//! Client: puts the float on the water at the flat position `target`, replacing any float already out.
	//! \return True when a float is out after the call.
	static bool Show(vector target)
	{
		Hide();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		s_fRestY = SurfaceY(world, target);
		vector at = target;
		at[1] = s_fRestY + REST_ABOVE_WATER_METRES;
		s_Float = LF_Utils.SpawnEntityPrefab(PREFAB, at, "0 0 0", false);
		if (!s_Float)
		{
			LF_Debug.Error(LF_FishingConfig.FEATURE, "the float prefab did not spawn: the cast has no float to show");
			return false;
		}

		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("client: float out at the water (surface %1)", s_fRestY));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The water surface height at a flat position: the water query's, else the ocean height, else the ground.
	protected static float SurfaceY(BaseWorld world, vector target)
	{
		vector probe = target;
		probe[1] = world.GetSurfaceY(target[0], target[2]);
		vector surface;
		vector transformWS[4];
		vector extents;
		EWaterSurfaceType kind;
		if (ChimeraWorldUtils.TryGetWaterSurface(world, probe, surface, kind, transformWS, extents))
			return surface[1];

		return probe[1];
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the bite. The float is pulled under and bobs until it is taken away; the cue plays once.
	static void Dip()
	{
		if (!s_Float || s_bDipping)
			return;

		s_bDipping = true;
		s_iBobStep = 0;
		if (RplSession.Mode() != RplMode.Dedicated)
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);

		GetGame().GetCallqueue().CallLater(BobStep, BOB_INTERVAL_MS, true);
		BobStep();
		LF_Debug.Info(LF_FishingConfig.FEATURE, "client: the float dips");
	}

	//------------------------------------------------------------------------------------------------
	//! One step of the dip: under, nearly up, under again, up; then round again until Hide.
	static void BobStep()
	{
		if (!s_Float)
		{
			StopBobbing();
			return;
		}

		float depth = DepthAt(s_iBobStep);
		s_iBobStep++;
		vector at = s_Float.GetOrigin();
		at[1] = s_fRestY + REST_ABOVE_WATER_METRES - depth;
		s_Float.SetOrigin(at);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How far under its resting height the float is at step `step` of the dip (metres).
	static float DepthAt(int step)
	{
		int phase = step % 4;
		if (phase == 0)
			return DIP_DEPTH_METRES;

		if (phase == 1)
			return DIP_DEPTH_METRES * 0.15;

		if (phase == 2)
			return DIP_DEPTH_METRES * 0.8;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopBobbing()
	{
		if (s_bDipping && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(BobStep);

		s_bDipping = false;
		s_iBobStep = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: takes the float away (caught, missed, ended, rod put away, world end).
	static void Hide()
	{
		StopBobbing();
		if (s_Float)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(s_Float);
			s_Float = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOut()
	{
		return s_Float != null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDipping()
	{
		return s_bDipping;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Hide();
		s_fRestY = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FishingFloat's statics (LF_StaticReset finds it by reflection).
class LF_FishingFloat_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FishingFloat";
	}

	override void Apply()
	{
		LF_FishingFloat.ResetStatics();
	}
}
