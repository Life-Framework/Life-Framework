// LF_AssetAnim.c — the rules of an asset's own animation (#340): how far a spinning part has turned,
// how far a hinge or a slide has moved. Pure maths, called every frame by LF_AssetAnimComponent on each
// machine that renders; the server only replicates "active".

//------------------------------------------------------------------------------------------------
//! How a part moves. SPIN turns for as long as the asset is active; HINGE and SLIDE move to their end
//! while it is active and back when it is not.
enum LF_EAssetAnimMotion
{
	SPIN,
	HINGE,
	SLIDE
}

//------------------------------------------------------------------------------------------------
class LF_AssetAnim
{
	static const string FEATURE = "AssetAnim";

	protected static int s_iStarts;
	protected static int s_iStops;

	//------------------------------------------------------------------------------------------------
	//! Angles of a spinning part after `seconds`, each wrapped into 0..360 so a long spin loses no precision.
	static vector SpinAngles(vector degreesPerSecond, float seconds)
	{
		vector angles;
		for (int i = 0; i < 3; i++)
		{
			float turned = degreesPerSecond[i] * seconds;
			angles[i] = turned - Math.Floor(turned / 360) * 360;
		}

		return angles;
	}

	//------------------------------------------------------------------------------------------------
	//! Progress 0..1 of a hinge or slide one frame on: towards 1 while active, towards 0 when not.
	static float Progress(float progress, bool active, float timeSlice, float seconds)
	{
		if (seconds <= 0)
		{
			if (active)
				return 1;

			return 0;
		}

		float step = timeSlice / seconds;
		if (active)
			return Math.Min(progress + step, 1);

		return Math.Max(progress - step, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Eased progress, so a door starts and stops softly.
	static float Ease(float progress)
	{
		float t = Math.Clamp(progress, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Has a hinge or slide reached the end it is moving to?
	static bool Settled(float progress, bool active)
	{
		if (active)
			return progress >= 1;

		return progress <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Counted by the component when the server switches an asset on or off (tests and logs).
	static void CountSwitch(bool active)
	{
		if (active)
			s_iStarts = s_iStarts + 1;
		else
			s_iStops = s_iStops + 1;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStarts()
	{
		return s_iStarts;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStops()
	{
		return s_iStops;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_iStarts = 0;
		s_iStops = 0;
	}
}
