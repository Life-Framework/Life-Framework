//------------------------------------------------------------------------------------------------
//! One robber currently pushing a drill into a vault wall. Server only.
class LF_DrillSceneSession : Managed
{
	EntityID m_Worker;
	EntityID m_Drill;
	vector m_vRestPosition;
	vector m_vRestAngles;
}

//------------------------------------------------------------------------------------------------
//! The drill while the vault is being drilled (#340, #273). LF_Bank lays the drill beside the robber
//! (LeaveTool) and starts the push animation (LF_WorkAnim, DRILL); this holds the drill in the air in
//! front of the robber where the animation's hands are, switches it on (LF_AssetAnimComponent: the
//! chuck spins, sparks fly from the bit, the motor loops) and, when the work ends for any reason,
//! switches it off and lays it back where LeaveTool put it. Server only; the component replicates.
class LF_DrillScene
{
	static const string FEATURE = "Bank";

	//! Where the drill's origin sits in the robber's own space while they push (right, up, forward, m).
	//! From tools/assets/drill/clips/push_loop.json: the right hand is at 0.10 1.20 0.42 and the model's
	//! pistol grip is at -0.035 -0.03 -0.21, so the origin is the hand minus the grip.
	static const vector HOLD_OFFSET = "0.135 1.23 0.63";

	protected static ref array<ref LF_DrillSceneSession> s_aSessions = {};
	protected static int s_iStarts;
	protected static int s_iStops;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aSessions.Clear();
		s_iStarts = 0;
		s_iStops = 0;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStartCount()
	{
		return s_iStarts;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStopCount()
	{
		return s_iStops;
	}

	//------------------------------------------------------------------------------------------------
	static int GetActiveCount()
	{
		return s_aSessions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the drill's world position for a robber standing at `origin` and facing along `forward`.
	//! Only the heading counts: the drill is level whatever the ground or the character's lean.
	static vector HoldPosition(vector origin, vector forward)
	{
		vector ahead = Vector(forward[0], 0, forward[2]);
		if (ahead.LengthSq() < 0.0001)
			ahead = "0 0 1";

		ahead.Normalize();
		vector right = Vector(ahead[2], 0, -ahead[0]);
		return origin + right * HOLD_OFFSET[0] + Vector(0, HOLD_OFFSET[1], 0) + ahead * HOLD_OFFSET[2];
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the robber starts drilling with this drill (already lying where it will be left).
	//! \return true when the drill is held and running.
	static bool Begin(IEntity robber, IEntity drill)
	{
		if (!Replication.IsServer() || !robber || !drill)
			return false;

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(drill);
		if (!anim)
		{
			LF_Debug.Error(FEATURE, string.Format("drill scene not started: %1 has no LF_AssetAnimComponent", LF_Utils.GetPrefabName(drill)));
			return false;
		}

		End(robber.GetID(), "a new drill scene began");

		LF_DrillSceneSession session = new LF_DrillSceneSession();
		session.m_Worker = robber.GetID();
		session.m_Drill = drill.GetID();
		session.m_vRestPosition = drill.GetOrigin();
		session.m_vRestAngles = drill.GetAngles();
		s_aSessions.Insert(session);
		s_iStarts++;

		vector transform[4];
		robber.GetWorldTransform(transform);
		anim.Pose(HoldPosition(transform[3], transform[2]), Vector(0, robber.GetAngles()[1], 0));
		anim.SetActive(true);
		LF_Debug.Info(FEATURE, string.Format("drill scene started for %1", LF_Utils.GetPlayerUid(robber)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the drilling ended (done, cancelled, the robber went down or left). Harmless when no
	//! scene runs for that worker.
	static void End(EntityID worker, string reason)
	{
		for (int i = s_aSessions.Count() - 1; i >= 0; i--)
		{
			LF_DrillSceneSession session = s_aSessions[i];
			if (session.m_Worker != worker)
				continue;

			s_aSessions.Remove(i);
			s_iStops++;
			IEntity drill = GetGame().GetWorld().FindEntityByID(session.m_Drill);
			LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(drill);
			if (!anim)
			{
				LF_Debug.Warn(FEATURE, "drill scene ended (" + reason + "): the drill is gone");
				continue;
			}

			anim.SetActive(false);
			anim.Release(session.m_vRestPosition, session.m_vRestAngles);
			LF_Debug.Info(FEATURE, "drill scene ended: " + reason);
		}
	}
}
