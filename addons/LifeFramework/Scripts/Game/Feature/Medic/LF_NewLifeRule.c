//------------------------------------------------------------------------------------------------
//! One wristband: where the wearer died and when the rule runs out. Session state; a restart
//! forgets it, like the pending hospital wake it hangs off.
class LF_NewLifeWristband : Managed
{
	string m_sUid;
	vector m_vScene;
	int m_iWokeAt;
	int m_iExpiresAt;
	//! The wearer has been seen outside the radius, so coming back in counts as a return. A wake
	//! in a bed that is itself inside the radius does not fire the reminder.
	bool m_bLeft;
	bool m_bReminded;
}

//------------------------------------------------------------------------------------------------
//! The New Life Rule (#264, Medic epic #146). A real death records the death scene; the hospital
//! wake then gives the wearer a wristband for LF_MedicConfig.m_iNewLifeMinutes. A revive never
//! dies, so it never reaches the wake and gives none. While a wristband lasts, a throttled server
//! check (every m_fNewLifeCheckSeconds, only while a wristband exists) watches the wearer's
//! distance to the scene; coming back inside m_fNewLifeRadius after having been outside gives one
//! quiet toast and one incident row (LF_OnNewLifeReturn, kind "nlr"). Nothing blocks the player.
//! PERSISTENCE: none. The wristband is session state; the window is minutes long.
class LF_NewLifeRule
{
	static const string FEATURE = "NewLife";

	protected static ref map<string, vector> s_mScenes;
	protected static ref array<ref LF_NewLifeWristband> s_aWristbands;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_NewLifeWristband> Wristbands()
	{
		if (!s_aWristbands)
			s_aWristbands = new array<ref LF_NewLifeWristband>();

		return s_aWristbands;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		if (s_bTicking)
			GetGame().GetCallqueue().Remove(TickNow);

		s_bTicking = false;
		s_mScenes = null;
		s_aWristbands = null;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Wristbands().Count();
	}

	//------------------------------------------------------------------------------------------------
	static LF_NewLifeWristband Find(string uid)
	{
		foreach (LF_NewLifeWristband band : Wristbands())
		{
			if (band.m_sUid == uid)
				return band;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player died for real at this position. The next wake turns it into a wristband.
	static void RecordDeath(string uid, vector position)
	{
		if (uid.IsEmpty())
			return;

		if (!s_mScenes)
			s_mScenes = new map<string, vector>();

		s_mScenes.Set(uid, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the hospital wake: put the wristband on a player whose death was recorded.
	//! \return The wristband, or null when the rule is off or there is no recorded death.
	static LF_NewLifeWristband Issue(string uid, IEntity body)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		if (!config.m_bNewLifeEnabled)
			return null;

		LF_NewLifeWristband band = Wear(uid, System.GetUnixTime(), config.m_iNewLifeMinutes);
		if (band && body)
			GetGame().GetCallqueue().CallLater(WakeToast, 1500, false, body, config.m_iNewLifeMinutes);

		return band;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast needs the body to be the player's controlled entity, which the handover sets late.
	protected static void WakeToast(IEntity body, int minutes)
	{
		if (body)
			LF_Utils.NotifyEntity(body, "#LF-NewLife_WakeToast", "#LF-Hospital_Title", 6.0, minutes.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the wristband itself, with the clock passed in so a test controls it.
	static LF_NewLifeWristband Wear(string uid, int now, int minutes)
	{
		if (uid.IsEmpty() || !s_mScenes || !s_mScenes.Contains(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("no wristband for %1: no recorded death", uid));
			return null;
		}

		vector scene = s_mScenes.Get(uid);
		s_mScenes.Remove(uid);

		LF_NewLifeWristband band = Find(uid);
		if (!band)
		{
			band = new LF_NewLifeWristband();
			Wristbands().Insert(band);
		}

		band.m_sUid = uid;
		band.m_vScene = scene;
		band.m_iWokeAt = now;
		band.m_iExpiresAt = now + minutes * 60;
		band.m_bLeft = false;
		band.m_bReminded = false;
		LF_Debug.Info(FEATURE, string.Format("wristband on %1 for %2 min, death scene %3", uid, minutes, scene));
		LF_EventBus.Publish(LF_OnNewLifeWristband.Create(uid, minutes, scene));
		StartTicking();
		return band;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !Replication.IsServer())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(TickNow, Math.Round(LF_MedicConfig.Get().m_fNewLifeCheckSeconds * 1000), true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickNow()
	{
		Tick(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is this position within the radius (flat, metres) of the scene?
	static bool IsNear(vector scene, vector position, float radius)
	{
		return vector.DistanceSqXZ(scene, position) <= radius * radius;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: one look at a wearer. Updates the wristband and returns true when this look is the
	//! reminder: back inside the radius after having been outside, inside the window, not yet
	//! reminded.
	static bool Observe(notnull LF_NewLifeWristband band, vector position, int now, float radius)
	{
		if (now >= band.m_iExpiresAt || band.m_bReminded)
			return false;

		if (!IsNear(band.m_vScene, position, radius))
		{
			band.m_bLeft = true;
			return false;
		}

		if (!band.m_bLeft)
			return false;

		band.m_bReminded = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one throttled pass over every wristband. Expired and reminded ones are dropped;
	//! the timer stops with the last one.
	static void Tick(int now)
	{
		float radius = LF_MedicConfig.Get().m_fNewLifeRadius;
		array<ref LF_NewLifeWristband> bands = Wristbands();
		for (int i = bands.Count() - 1; i >= 0; i--)
		{
			LF_NewLifeWristband band = bands[i];
			if (now >= band.m_iExpiresAt || band.m_bReminded)
			{
				LF_Debug.Info(FEATURE, string.Format("wristband of %1 is done (reminded %2)", band.m_sUid, band.m_bReminded));
				bands.Remove(i);
				continue;
			}

			IEntity body = FindBody(band.m_sUid);
			if (!body)
				continue;

			vector position = body.GetOrigin();
			if (!Observe(band, position, now, radius))
				continue;

			Remind(band, body, position, now);
		}

		if (bands.IsEmpty() && s_bTicking)
		{
			GetGame().GetCallqueue().Remove(TickNow);
			s_bTicking = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remind(LF_NewLifeWristband band, IEntity body, vector position, int now)
	{
		LF_Debug.Info(FEATURE, string.Format("%1 returned to their death scene %2 s after waking", band.m_sUid, now - band.m_iWokeAt));
		LF_Utils.NotifyEntity(body, "#LF-NewLife_ReturnToast", "", 5.0);
		LF_EventBus.Publish(LF_OnNewLifeReturn.Create(band.m_sUid, position, band.m_vScene, now - band.m_iWokeAt));
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindBody(string uid)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return null;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			if (LF_Utils.GetPlayerUID(id) == uid)
				return players.GetPlayerControlledEntity(id);
		}

		return null;
	}
}
