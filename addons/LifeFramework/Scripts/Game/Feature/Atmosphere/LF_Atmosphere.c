//------------------------------------------------------------------------------------------------
//! The town's background sound on one machine: every LF_AmbientEmitterComponent alive here, and a
//! tick that starts and stops their voices from LF_AtmosphereMix. Nothing here is replicated; the
//! server only replicates state an emitter's condition reads (LF_AmbientSwitchComponent, a
//! station's percent). Design and the list of spots: docs/architecture/atmosphere.md.
class LF_Atmosphere
{
	static const string FEATURE = "Atmosphere";
	static const int TICK_MS = 500; // lf-fixed: how often this machine re-reads listener distances, not a gameplay knob

	//! Most ambient voices at once on this machine. The engine mixes far more; this keeps a busy
	//! street from turning into noise and bounds the cost.
	static int s_iMaxVoices = 6;

	protected static ref array<ref LF_AmbientCandidate> s_aCandidates;
	protected static ref array<int> s_aChosen;
	protected static bool s_bTicking;
	protected static int s_iStarts;
	protected static int s_iStops;
	protected static int s_iPeak;

	//------------------------------------------------------------------------------------------------
	//! Called by an emitter when it joins or leaves LF_AmbientEmitterComponent.GetAll(): the tick
	//! runs while there is at least one. The first emitter of a world always arms it, because a
	//! world change can drop the queue while the flag from the last world is still set.
	static void OnEmittersChanged(int count)
	{
		if (!s_aCandidates)
		{
			s_aCandidates = {};
			s_aChosen = {};
		}

		ScriptCallQueue queue;
		if (GetGame())
			queue = GetGame().GetCallqueue();

		if (!queue)
			return;

		if (count == 0)
		{
			s_bTicking = false;
			queue.Remove(Tick);
			return;
		}

		if (s_bTicking && count > 1)
			return;

		s_bTicking = true;
		queue.Remove(Tick);
		queue.CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static int EmitterCount()
	{
		return LF_AmbientEmitterComponent.GetAll().Count();
	}

	//------------------------------------------------------------------------------------------------
	static int PlayingCount()
	{
		int n;
		foreach (LF_AmbientEmitterComponent emitter : LF_AmbientEmitterComponent.GetAll())
		{
			if (emitter && emitter.IsPlaying())
				n++;
		}

		return n;
	}

	//------------------------------------------------------------------------------------------------
	static void CountStart()
	{
		s_iStarts++;
		int playing = PlayingCount() + 1;
		if (playing > s_iPeak)
			s_iPeak = playing;
	}

	//------------------------------------------------------------------------------------------------
	static void CountStop()
	{
		s_iStops++;
	}

	//------------------------------------------------------------------------------------------------
	//! One line for logs and proofs: how many emitters, how many sound, the most at once.
	static string Describe()
	{
		return string.Format("emitters=%1 playing=%2 peak=%3 cap=%4 starts=%5 stops=%6", EmitterCount(), PlayingCount(), s_iPeak, s_iMaxVoices, s_iStarts, s_iStops);
	}

	//------------------------------------------------------------------------------------------------
	static string LabelOf(IEntity entity)
	{
		if (!entity)
			return "(no entity)";

		string name = entity.GetName();
		if (!name.IsEmpty())
			return name;

		EntityPrefabData data = entity.GetPrefabData();
		if (data && !data.GetPrefabName().IsEmpty())
			return FilePath.StripPath(data.GetPrefabName());

		return entity.ClassName();
	}

	//------------------------------------------------------------------------------------------------
	//! Local solar hour of the day, 0 to 24 (noon is the sun at its highest; the engine clock runs on
	//! the world's time zone, as LF_FilmScene explains). 12 when the world has no clock.
	static float HourNow()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return 12;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		if (!clock)
			return 12;

		float hour = clock.GetTimeOfTheDay() + clock.GetTimeZoneOffset() + clock.GetCurrentLongitude() / 15;
		while (hour < 0)
			hour += 24;

		while (hour >= 24)
			hour -= 24;

		return hour;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		array<LF_AmbientEmitterComponent> emitters = LF_AmbientEmitterComponent.GetAll();
		if (emitters.IsEmpty() || !s_aCandidates)
			return;

		float hour = HourNow();
		s_aCandidates.Clear();
		for (int i = 0; i < emitters.Count(); i++)
		{
			LF_AmbientEmitterComponent source = emitters[i];
			if (!source || !source.GetOwner())
				continue;

			// The engine's own distance to the listener (the camera), so no player lookup here.
			float distance = AudioSystem.GetDistance(source.GetSoundPosition());
			s_aCandidates.Insert(source.Refresh(i, distance, hour));
		}

		LF_AtmosphereMix.Select(s_aCandidates, s_iMaxVoices, s_aChosen);

		int before = s_iStarts + s_iStops;
		foreach (LF_AmbientCandidate candidate : s_aCandidates)
		{
			LF_AmbientEmitterComponent emitter = emitters[candidate.m_iId];
			if (s_aChosen.Contains(candidate.m_iId))
			{
				if (!candidate.m_bPlaying)
					emitter.Play(candidate.m_fDistance);
				else
					emitter.CheckVoice();

				continue;
			}

			if (!candidate.m_bPlaying)
				continue;

			if (!candidate.m_bAllowed)
				emitter.Stop("condition or hours");
			else if (LF_AtmosphereMix.InRange(candidate))
				emitter.Stop("voice cap or group");
			else
				emitter.Stop(string.Format("out of range at %1 m", candidate.m_fDistance.ToString(-1, 1)));
		}

		if (s_iStarts + s_iStops != before)
			LF_Debug.Info(FEATURE, string.Format("mix %1 hour=%2", Describe(), hour.ToString(-1, 1)));
	}
}
