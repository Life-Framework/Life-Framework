// LF_ResidentActivity.c - a resident does what their timetable line says (#2907, epic #1012). The
// spawner (#2858) used to stand a resident on their anchor and only log `m_sActivity`; now it calls
// Apply, which sits them on a bench, faces them out at a counter, or leans them in a doorway:
//   sit     a free LF_SitAction seat near the anchor takes the body (LF_NpcLoiter.Sit, the same seat and
//           pose a player's Sit uses, so a late joiner sees it through the base game loiter replication).
//   serve   the resident faces the anchor's facing and stands behind the counter.
//   loiter  the LF_NpcLoiter stand set (lean or smoke), picked by the resident's id.
//   idle    (and empty) the resident faces the anchor's facing and stands.
// An activity that is none of these falls back to idle with one LF_Debug.Warn per name. A sit with no
// free seat near the anchor falls back to idle too, warned once per name. So does a sit or loiter for a
// resident whose prefab is a static NPC body (no character controller): the resident prefab must be a
// character (SCR_ChimeraCharacter) to sit or lean. Server only; nothing persists
// (the resident is respawned from the timetable). Entries hold nothing long-lived.

//------------------------------------------------------------------------------------------------
class LF_ResidentActivity
{
	static const string FEATURE = "Residents";
	static const string SIT = "sit";
	static const string SERVE = "serve";
	static const string LOITER = "loiter";
	static const string IDLE = "idle";
	//! How far from the anchor a seat may be for `sit` (the bench the anchor stands beside), in metres.
	static const float SEAT_RANGE = 2.5; // lf-fixed: the reach of one bench, not a server rule

	protected static ref set<string> s_mWarned;
	protected static int s_iWarnCount;

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: the activity a line stands for. Case and spaces are ignored; empty and
	//! unknown names are idle. \param[out] known false when the name was neither empty nor an activity.
	static string Normalise(string activity, out bool known)
	{
		string name = activity;
		name.TrimInPlace();
		name.ToLower();
		known = true;
		if (name.IsEmpty() || name == IDLE)
			return IDLE;

		if (name == SIT || name == SERVE || name == LOITER)
			return name;

		known = false;
		return IDLE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts the resident to their activity at `spot`. The body is first turned to the spot's
	//! facing. \return The activity the resident ended up doing (sit, serve, loiter or idle).
	static string Apply(IEntity entity, string activity, LF_ResidentSpot spot)
	{
		if (!entity || !Replication.IsServer())
			return IDLE;

		bool known;
		string what = Normalise(activity, known);
		if (!known)
			WarnOnce(activity, string.Format("resident activity '%1' is unknown; the resident idles", activity));

		if (spot)
			Face(entity, spot.m_vAngles);

		// A static body (an LF_NpcBaseEntity mannequin) has no character controller: it cannot sit or lean.
		if ((what == SIT || what == LOITER) && !SCR_ChimeraCharacter.Cast(entity))
		{
			WarnOnce("static", string.Format("resident is a static body, not a character; cannot %1, the resident stands", what));
			return IDLE;
		}

		if (what == SIT)
		{
			LF_SitAction seat = LF_NpcLoiter.FindFreeSeat(entity, SEAT_RANGE);
			if (seat && LF_NpcLoiter.Sit(entity, seat))
				return SIT;

			WarnOnce(SIT, "resident should sit but no free seat is near the anchor; the resident idles");
			return IDLE;
		}

		if (what == LOITER)
		{
			LF_ENpcLoiter kind = LF_NpcLoiter.Choose(false, LF_Roles.SeedHash(entity.GetID().ToString()));
			if (LF_NpcLoiter.Stand(entity, kind))
				return LOITER;

			WarnOnce(LOITER, "resident could not start the loiter idle; the resident idles");
			return IDLE;
		}

		return what;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the resident holds a seat.
	static bool IsSeated(IEntity entity)
	{
		return LF_NpcLoiter.IsSeated(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the resident's seat or stand-loiter up (a despawn does this through LF_NpcLoiter's watch;
	//! tests call it).
	static void Release(IEntity entity)
	{
		LF_NpcLoiter.Release(entity, "resident activity released");
	}

	//------------------------------------------------------------------------------------------------
	//! Turns the body to `angles` (yaw, pitch, roll in degrees) where it stands, level.
	static void Face(notnull IEntity entity, vector angles)
	{
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), mat);
		mat[3] = entity.GetOrigin();
		entity.SetWorldTransform(mat);
		entity.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Yaw of the body in degrees, 0 to 360 (tests read it).
	static float YawOf(notnull IEntity entity)
	{
		vector mat[4];
		entity.GetWorldTransform(mat);
		vector angles = Math3D.MatrixToAngles(mat);
		float yaw = angles[0];
		while (yaw < 0)
			yaw += 360;

		while (yaw >= 360)
			yaw -= 360;

		return yaw;
	}

	//------------------------------------------------------------------------------------------------
	//! How many activity warnings were logged since world start (tests read it).
	static int WarnCount()
	{
		return s_iWarnCount;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WarnOnce(string key, string message)
	{
		if (!s_mWarned)
			s_mWarned = new set<string>();

		if (s_mWarned.Contains(key))
			return;

		s_mWarned.Insert(key);
		s_iWarnCount++;
		LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): forgets which warnings were logged.
	static void ResetStatics()
	{
		s_mWarned = null;
		s_iWarnCount = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ResidentActivity's statics (LF_StaticReset finds it by reflection).
class LF_ResidentActivity_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentActivity";
	}

	override void Apply()
	{
		LF_ResidentActivity.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Residents", description: "A placed character does a resident activity (sit, serve, loiter, idle) a few seconds after the world loads: a map maker's resident who needs no timetable, and the grid proof's cast.")]
class LF_ResidentActivityComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ResidentActivityComponent : ScriptComponent
{
	[Attribute("idle", UIWidgets.EditBox, desc: "sit, serve, loiter or idle (the same words as a timetable line)")]
	protected string m_sActivity;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		// The world has loaded and the seat props have their actions by then.
		GetGame().GetCallqueue().CallLater(Begin, 3000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Begin()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		AIControlComponent control = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
		if (control && control.IsAIActivated())
			control.DeactivateAI();

		LF_ResidentSpot spot = new LF_ResidentSpot();
		vector mat[4];
		owner.GetWorldTransform(mat);
		spot.m_vOrigin = mat[3];
		spot.m_vAngles = Math3D.MatrixToAngles(mat);
		LF_ResidentActivity.Apply(owner, m_sActivity, spot);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Begin);

		super.OnDelete(owner);
	}
}
