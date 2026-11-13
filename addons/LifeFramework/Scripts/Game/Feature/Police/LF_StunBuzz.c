//------------------------------------------------------------------------------------------------
//! The electric buzz around a stunned player (#341). The server replicates the stun flag on the
//! character (SCR_ChimeraCharacter.m_bLF_Stunned in LF_Downed.c); every client that has the
//! character streamed in starts a looped sound on it when the flag sets and ends it when the flag
//! clears, so the sound follows the server's stun and nearby players hear it from the body.
//!
//! Client side only: a dedicated server has no listener. The sound and its project come from the
//! asset pipeline's sound stage (tools/assets/taser/sounds.json).
//------------------------------------------------------------------------------------------------
class LF_StunBuzz
{
	static const string FEATURE = "Police";
	static const ResourceName PROJECT = "{3BB5DCC4ECCFE291}Sounds/Weapons/Handguns/Taser/Weapons_Handguns_Taser_Stun.acp";
	static const string EVENT = "SOUND_LF_STUN_LP";

	//! A loop never outlives this, whatever happens to the flag or the character.
	// lf-fixed: a safety cap on a leaked loop, far above any stun; the stun time is the weapon's m_fStunSeconds
	static const int MAX_MS = 30000;

	protected static ref map<EntityID, ref SCR_AudioSource> s_mPlaying = new map<EntityID, ref SCR_AudioSource>();
	protected static int s_iStarted;
	protected static int s_iStopped;

	//------------------------------------------------------------------------------------------------
	//! Pure rule: what to do for a character given its stun flag and whether its buzz plays here.
	//! \return 1 start, -1 stop, 0 nothing.
	static int Decide(bool stunned, bool playing)
	{
		if (stunned && !playing)
			return 1;

		if (!stunned && playing)
			return -1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Bring the buzz on this machine in line with the character's stun flag. Called from the
	//! replication callback on clients and directly where the server changes the flag (a host).
	static void Sync(notnull SCR_ChimeraCharacter character)
	{
		if (System.IsConsoleApp() || RplSession.Mode() == RplMode.Dedicated)
			return;

		EntityID id = character.GetID();
		int decision = Decide(character.LF_HasStunFlag(), s_mPlaying.Contains(id));
		if (decision > 0)
			Start(character, id);
		else if (decision < 0)
			Stop(id, "the stun ended");
	}

	//------------------------------------------------------------------------------------------------
	protected static void Start(notnull SCR_ChimeraCharacter character, EntityID id)
	{
		SCR_SoundManagerModule sound = SCR_SoundManagerModule.GetInstance(character.GetWorld());
		if (!sound)
		{
			LF_Debug.Warn(FEATURE, "stun buzz: no sound manager in this world; the stun is silent");
			return;
		}

		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = PROJECT;
		config.m_sSoundEventName = EVENT;
		// Not static: a stunned body can be carried.
		config.m_eFlags = 0;

		// Null when the listener is out of the sound's range, or when the project did not load.
		SCR_AudioSource audio = sound.CreateAudioSource(character, config);
		if (!audio)
		{
			LF_Debug.Info(FEATURE, string.Format("stun buzz on %1 not started: out of earshot here (or %2 failed to load)", LF_Utils.GetPlayerUID(character), PROJECT));
			return;
		}

		sound.PlayAudioSource(audio);
		s_mPlaying.Set(id, audio);
		s_iStarted++;
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Expire, MAX_MS, false, audio);

		LF_Debug.Info(FEATURE, string.Format("stun buzz started on %1 (%2)", LF_Utils.GetPlayerUID(character), EVENT));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop(EntityID id, string why)
	{
		SCR_AudioSource audio = s_mPlaying.Get(id);
		if (!audio)
			return;

		audio.Terminate();
		s_mPlaying.Remove(id);
		s_iStopped++;
		LF_Debug.Info(FEATURE, string.Format("stun buzz stopped: %1", why));
	}

	//------------------------------------------------------------------------------------------------
	//! The cap: ends this loop if it is still the one playing for that character.
	//! (The call queue cannot carry an EntityID, so the loop itself is the key.)
	protected static void Expire(SCR_AudioSource audio)
	{
		if (!audio)
			return;

		foreach (EntityID id, SCR_AudioSource playing : s_mPlaying)
		{
			if (playing != audio)
				continue;

			Stop(id, "it reached the time cap");
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	static int GetPlayingCount()
	{
		return s_mPlaying.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int GetStartedCount()
	{
		return s_iStarted;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStoppedCount()
	{
		return s_iStopped;
	}
}
