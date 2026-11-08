//------------------------------------------------------------------------------------------------
//! Cooldown math shared by the vehicle and weapon robbery actions.
//! World time (BaseWorld.GetWorldTime) is in milliseconds; the action attributes are in seconds.
class LF_RobCooldown
{
	//! Sentinel for "never robbed": no cooldown until the first robbery.
	static const float NEVER = -1;

	//------------------------------------------------------------------------------------------------
	//! \param nowMs current world time in milliseconds
	//! \param lastMs world time of the last robbery in milliseconds, or NEVER
	//! \param cooldownSec cooldown length in seconds
	static bool IsActive(float nowMs, float lastMs, float cooldownSec)
	{
		if (lastMs < 0)
			return false;

		return (nowMs - lastMs) < (cooldownSec * 1000.0);
	}
}
