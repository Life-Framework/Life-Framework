//------------------------------------------------------------------------------------------------
//! The two voiced lines a radio call is read with: what happened and where. Either may be null.
class LF_DispatchVoicePick : Managed
{
	LF_DialogueLine m_Type;
	LF_DialogueLine m_Place;
}

//------------------------------------------------------------------------------------------------
//! The dispatcher's voice on an officer's radio (#4422). When a call reaches an on-duty officer the
//! vanilla radio squelch plays from their body (LF_Dispatch, `radioSoundProject`), then the dispatcher
//! says the call type and the place: "Robbery in progress. At the general store."
//!
//! The recordings are a normal voiced dialogue, `dispatcher` (tools/assets/dialogue/dispatcher), that
//! the pack registers in the dialogue index. A line answers a key: the offence key of the call picks
//! the type line, the place the call carries (a sign title key) picks the place line. The core holds
//! no place names; a place with no line, or an offence with none, is logged and the call still lands
//! (the written line and the board are unchanged); the voice just says what it has.
class LF_DispatchVoice
{
	static const string FEATURE = "Police";
	static const string DIALOGUE_ID = "dispatcher";
	//! The squelch is short; the voice starts after it, then the place follows the type.
	static const int SQUELCH_LEAD_MS = 600; // lf-fixed: audio pacing after the radio squelch
	static const int GAP_MS = 250; // lf-fixed: pause between the type and place lines

	//------------------------------------------------------------------------------------------------
	//! The lines for one call. Never null; a missing line leaves its field null and warns.
	//! \param config The dispatcher dialogue (null logs and returns an empty pick).
	//! \param offence The call's offence key, `#LF-Offence_Robbery`.
	//! \param place The place the call carries: a sign title key, or "grid ..." text, which has no line.
	static LF_DispatchVoicePick Pick(LF_DialogueConfig config, string offence, string place)
	{
		LF_DispatchVoicePick pick = new LF_DispatchVoicePick();
		if (!config)
		{
			LF_Debug.Warn(FEATURE, "dispatch voice: no dispatcher dialogue loaded; the call lands without a voice");
			return pick;
		}

		pick.m_Type = VoicedFor(config, offence);
		if (!pick.m_Type)
			LF_Debug.Warn(FEATURE, string.Format("dispatch voice: no voiced type line for '%1'; the call lands without it", offence));

		pick.m_Place = VoicedFor(config, place);
		if (!pick.m_Place)
			LF_Debug.Warn(FEATURE, string.Format("dispatch voice: no voiced place line for '%1'; the call lands without it", place));

		LF_Debug.Info(FEATURE, LogText(pick));
		return pick;
	}

	//------------------------------------------------------------------------------------------------
	//! "dispatch voice <type line id> <place line id>"; a missing line reads "none".
	static string LogText(notnull LF_DispatchVoicePick pick)
	{
		string type = "none";
		string place = "none";
		if (pick.m_Type)
			type = pick.m_Type.m_sId;

		if (pick.m_Place)
			place = pick.m_Place.m_sId;

		return string.Format("dispatch voice %1 %2", type, place);
	}

	//------------------------------------------------------------------------------------------------
	//! The recorded line voiced for a key, or null (no key, no such line, or a text-only line).
	protected static LF_DialogueLine VoicedFor(notnull LF_DialogueConfig config, string key)
	{
		if (key.IsEmpty() || !config.m_aLines)
			return null;

		foreach (LF_DialogueLine line : config.m_aLines)
		{
			if (line && line.m_sForKey == key && !line.IsTextOnly())
				return line;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the officer hears the call. Loads the dispatcher dialogue, picks the lines and plays the
	//! type, then the place, from the officer's body after the radio squelch.
	static void Speak(string offence, string place)
	{
		LF_DialogueConfig config;
		ResourceName path = LF_DialogueRegistry.Resolve(DIALOGUE_ID);
		if (!path.IsEmpty())
			config = LF_DialogueConfig.Load(path);

		LF_DispatchVoicePick pick = Pick(config, offence, place);
		if (!config || config.m_sSoundProject.IsEmpty())
			return;

		int delay = SQUELCH_LEAD_MS;
		if (pick.m_Type)
		{
			GetGame().GetCallqueue().CallLater(PlayLine, delay, false, config.m_sSoundProject, pick.m_Type.m_sEvent);
			delay += Math.Floor(pick.m_Type.m_fSeconds * 1000) + GAP_MS;
		}

		if (pick.m_Place)
			GetGame().GetCallqueue().CallLater(PlayLine, delay, false, config.m_sSoundProject, pick.m_Place.m_sEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: play one event from the local officer's body (they may have died or left the body by now).
	protected static void PlayLine(string project, string soundEvent)
	{
		if (!GetGame() || !GetGame().GetPlayerController())
			return;

		IEntity body = GetGame().GetPlayerController().GetControlledEntity();
		if (!body)
			return;

		vector mat[4];
		body.GetWorldTransform(mat);
		mat[3] = body.GetOrigin() + "0 1.5 0";
		if (AudioSystem.PlayEvent(project, soundEvent, mat) == AudioHandle.Invalid)
			LF_Debug.Error(FEATURE, string.Format("dispatch voice event %1 did not start in %2", soundEvent, project));
	}
}
