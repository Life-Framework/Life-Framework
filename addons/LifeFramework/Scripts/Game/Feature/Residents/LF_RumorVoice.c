// LF_RumorVoice.c - a rumor that is told is heard (#4569, slice of #4170, plan W5.4).
// A giver (the gossip bench, the bar, the informant) tells one row with `Deliver`: the place goes on
// the listener's own map, the row's recorded clip plays from the speaker's head on the listener's
// machine, and the caption shows as a toast for as long as the clip runs. The clip and the caption
// come from the row's `m_sLineKey`, found in one of the recorded conversations the rumor config lists
// (`LF_RumorsConfig.m_aVoices`), so a pack adds rows and voices with no core edit. A row with no
// recording still shows its caption and marks the place: nothing is silently dropped.

//------------------------------------------------------------------------------------------------
class LF_RumorVoice
{
	static const string FEATURE = "Residents";
	//! Seconds the caption stays after the clip ends.
	static const float CAPTION_PAD_SECONDS = 1.5; // lf-fixed: reading time
	//! Caption time for a row with no recording.
	static const float CAPTION_SILENT_SECONDS = 6.0; // lf-fixed: reading time

	protected static ref map<string, ref LF_DialogueConfig> s_mVoices;

	//------------------------------------------------------------------------------------------------
	//! The recorded line a row speaks, and the conversation it is in. Null when no listed conversation
	//! holds the row's line key.
	static LF_DialogueLine FindLine(notnull LF_RumorDef def, out LF_DialogueConfig voice)
	{
		voice = null;
		LF_RumorsConfig config = LF_RumorsConfig.Get();
		if (!config || !config.m_aVoices || def.m_sLineKey.IsEmpty())
			return null;

		if (!s_mVoices)
			s_mVoices = new map<string, ref LF_DialogueConfig>();

		foreach (ResourceName path : config.m_aVoices)
		{
			LF_DialogueConfig conversation;
			if (!s_mVoices.Find(path, conversation))
			{
				conversation = LF_DialogueConfig.Layered(path);
				s_mVoices.Set(path, conversation);
			}

			if (!conversation || !conversation.m_aLines)
				continue;

			foreach (LF_DialogueLine line : conversation.m_aLines)
			{
				if (line && line.m_sTextKey == def.m_sLineKey)
				{
					voice = conversation;
					return line;
				}
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds the caption toast holds: the clip's length and a short read, or a fixed read for a row
	//! with no clip.
	static float CaptionSeconds(LF_DialogueLine line)
	{
		if (!line || line.IsTextOnly())
			return CAPTION_SILENT_SECONDS;

		return line.m_fSeconds + CAPTION_PAD_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: play the row's clip from the speaker's head. Silent (logged) when the row has no
	//! recording or the speaker is not here.
	//! \return True when a clip started.
	static bool Speak(IEntity speaker, notnull LF_RumorDef def)
	{
		LF_DialogueConfig voice;
		LF_DialogueLine line = FindLine(def, voice);
		if (!line || !voice || line.IsTextOnly())
		{
			LF_Debug.Warn(FEATURE, string.Format("rumor '%1' has no recorded line for %2: caption only", def.m_sId, def.m_sLineKey));
			return false;
		}

		if (!speaker)
		{
			LF_Debug.Warn(FEATURE, string.Format("rumor '%1': the speaker is not on this machine: no clip", def.m_sId));
			return false;
		}

		LF_DialogueVoice.Say(speaker, voice, line.m_sId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `playerId` hears `def` from `speaker` (the gossip, the barkeeper, the informant; may be
	//! null for a caption only). The place is heard of on their own map, the clip plays on their machine
	//! and the caption shows as a toast under `titleKey`; `fallbackKey` is the toast text for a row with
	//! no caption row.
	static void Deliver(int playerId, IEntity speaker, notnull LF_RumorDef def, string titleKey, string fallbackKey)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller)
		{
			controller.LF_SendMapKnown(def.m_sPlaceId, LF_MapKnownRules.SavedState(LF_EMapPoiKnown.HEARD));
			RplId speakerId = LF_NetworkUtils.GetRplId(speaker);
			if (speaker && speakerId.IsValid())
				controller.LF_RumorSay(speakerId, def.m_sId);
		}

		LF_DialogueConfig voice;
		LF_DialogueLine line = FindLine(def, voice);
		string body = fallbackKey;
		if (line && !line.m_sTextKey.IsEmpty())
			body = line.m_sTextKey;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, titleKey, body, CaptionSeconds(line), LF_ENotificationType.INFO);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mVoices = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RumorVoice's statics (LF_StaticReset finds it by reflection).
class LF_RumorVoice_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RumorVoice";
	}

	override void Apply()
	{
		LF_RumorVoice.ResetStatics();
	}
}
