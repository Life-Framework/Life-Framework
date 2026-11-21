//------------------------------------------------------------------------------------------------
//! Guides a new player through the first errands: while a quest step names a target
//! (LF_QuestDefinition.m_sTargetKind), the character's current step puts that place on their screen (the
//! one waypoint, LF_Waypoint: a HUD marker with the distance) and on their own map (a PLAYER share,
//! LF_MapShares). When the step is turned in the marks move to the next step's target; when no step is
//! left they clear. A target is found by what it is: `giver` is a placed quest giver (LF_QuestManager),
//! anything else is an LF_Places kind, one id or the nearest. The core holds no coordinate and no place
//! name; the pack's strings say where things are.
//!
//! Server only; each mark goes to the one player (an owner RPC on their controller and a PLAYER share),
//! so nobody else sees it. Nothing is saved: the current step follows from the saved turn-ins, and every
//! spawn (join, rejoin mid-quest, respawn) works it out again (LF_QuestTrackerComponent.RefreshGuide).
class LF_ArrivalGuide
{
	static const string FEATURE = "ArrivalGuide";
	static const string KIND_GIVER = "giver";
	static const string WAYPOINT_ID = "arrival";
	static const string KEY_PREFIX = "arrival:";
	// A vanilla marker config index and colour, like the job stops (LF_JobMapMarks).
	static const int ICON = 0; // lf-fixed: vanilla config index, not a server tunable
	static const int COLOUR = 5; // lf-fixed: vanilla config index
	static const float EXPIRY_SECONDS = 7200; // lf-fixed: safety net only; finishing the step moves or removes the mark

	protected static ref LF_JobWaypointSink s_Sink;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Sink = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: replace where the waypoint goes. Null restores the game's.
	static void SetSink(LF_JobWaypointSink sink)
	{
		s_Sink = sink;
	}

	//------------------------------------------------------------------------------------------------
	static string KeyOf(string uid)
	{
		return KEY_PREFIX + uid;
	}

	//------------------------------------------------------------------------------------------------
	//! The step a character is on: a guided quest (it names a target) that is not turned in and whose
	//! previous quest is. Null when every guided step is done (or none exists).
	static LF_QuestDefinition CurrentStep(notnull array<string> turnedIn)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!manager)
			return null;

		foreach (string questId, LF_QuestDefinition def : manager.GetDefinitions())
		{
			if (!def || def.m_sTargetKind.IsEmpty() || turnedIn.Find(questId) != -1)
				continue;

			if (def.m_sRequiredPreviousQuestId.IsEmpty() || turnedIn.Find(def.m_sRequiredPreviousQuestId) != -1)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a step's target is, from `from` (the player, for "the nearest"). \return false when the
	//! world does not place it.
	static bool ResolveTarget(notnull LF_QuestDefinition step, vector from, out vector position)
	{
		if (step.m_sTargetKind == KIND_GIVER)
		{
			LF_QuestManager manager = LF_QuestManager.GetInstance();
			if (!manager)
				return false;

			LF_QuestGiverComponent giver = manager.GetGiver(step.m_sTargetId);
			if (!giver || !giver.GetOwner())
				return false;

			position = giver.GetOwner().GetOrigin();
			return true;
		}

		if (!step.m_sTargetId.IsEmpty())
			return LF_Places.FindOrigin(step.m_sTargetKind, step.m_sTargetId, position);

		array<IEntity> places = {};
		LF_Places.FindAll(step.m_sTargetKind, places);
		float best = -1;
		foreach (IEntity place : places)
		{
			if (!place)
				continue;

			float distance = vector.DistanceSqXZ(place.GetOrigin(), from);
			if (best < 0 || distance < best)
			{
				best = distance;
				position = place.GetOrigin();
			}
		}

		return best >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The name a step's mark carries.
	static string LabelOf(notnull LF_QuestDefinition step)
	{
		if (!step.m_sTargetLabelKey.IsEmpty())
			return step.m_sTargetLabelKey;

		return step.m_sTitleKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: works out this character's step and puts its target on their map and screen, or clears the
	//! marks when no guided step is left or the world does not place the target.
	//! \return The quest id guided to, or "" when no mark stands.
	static string Refresh(string uid, vector from, notnull array<string> turnedIn)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "no guide mark: the body has no player behind it");
			return string.Empty;
		}

		LF_QuestDefinition step = CurrentStep(turnedIn);
		if (!step)
		{
			Done(uid);
			return string.Empty;
		}

		vector target;
		if (!ResolveTarget(step, from, target))
		{
			LF_Debug.Warn(FEATURE, string.Format("no guide mark for %1 at step %2: no %3 '%4' is placed in the world", uid, step.m_sQuestId, step.m_sTargetKind, step.m_sTargetId));
			Done(uid);
			return string.Empty;
		}

		int x = Math.Round(target[0]);
		int z = Math.Round(target[2]);
		string label = LabelOf(step);
		LF_MapShareRow row = LF_MapShareRow.Make(KeyOf(uid), LF_EMapShareAudience.PLAYER, uid, x, z, ICON, COLOUR, label, LF_MapShares.Now() + EXPIRY_SECONDS);
		if (!LF_MapShares.Put(row))
			LF_Debug.Warn(FEATURE, string.Format("the map share for %1 was refused; the waypoint still goes", uid));

		Sink().Mark(uid, WAYPOINT_ID, label, x, z);
		LF_Debug.Info(FEATURE, string.Format("%1 is guided to step %2: map mark %3 and waypoint %4 at %5 %6", uid, step.m_sQuestId, row.m_sKey, WAYPOINT_ID, x, z));
		return step.m_sQuestId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: no step is left (or none can be shown): both marks go.
	static void Done(string uid)
	{
		if (uid.IsEmpty())
			return;

		bool removed = LF_MapShares.Remove(KeyOf(uid));
		Sink().Clear(uid, WAYPOINT_ID);
		if (removed)
			LF_Debug.Info(FEATURE, string.Format("%1 has no guided step left: the map mark and waypoint are cleared", uid));
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobWaypointSink Sink()
	{
		if (!s_Sink)
			s_Sink = new LF_JobWaypointRplSink();

		return s_Sink;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ArrivalGuide's statics (LF_StaticReset finds it by reflection).
class LF_ArrivalGuide_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ArrivalGuide";
	}

	override void Apply()
	{
		LF_ArrivalGuide.ResetStatics();
	}
}
