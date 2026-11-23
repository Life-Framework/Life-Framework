// LF_OutpostVoice.c - the outpost trader says no out loud (#4640, follow-up of #4459).
// The quartermaster and the contraband buyer have a voiced refusal (`for_key` LF_Outpost.REFUSE_KEY in their own
// conversation). When LF_Outpost.NotifyRefusal turns an on-duty officer away, the server finds the speaker
// standing at the trader's spot and tells the refused officer's own client to play that clip from his head;
// nobody else hears it from this call. A trader with no such line (the shop clerk, a plank trader) stays
// silent and the toast carries the refusal as before. Same wire as LF_FenceVoice: only an entity id and a
// line id travel, from the server to the one client.

//------------------------------------------------------------------------------------------------
//! Collects the nearest character within range that holds a refusal line.
class LF_OutpostSpeakerQuery
{
	IEntity m_Skip;
	IEntity m_Nearest;
	vector m_vFrom;
	float m_fBest;

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (!entity || entity == m_Skip)
			return true;

		if (!SCR_ChimeraCharacter.Cast(entity))
			return true;

		if (!LF_OutpostVoice.RefusalLineOf(entity))
			return true;

		float distance = vector.DistanceSq(entity.GetOrigin(), m_vFrom);
		if (distance < m_fBest)
		{
			m_fBest = distance;
			m_Nearest = entity;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OutpostVoice
{
	static const string FEATURE = "Outposts";
	//! How far from the trader's spot the speaker may stand (the crate sits beside the quartermaster).
	static const float SPEAKER_RANGE_M = 6.0; // lf-fixed: a counter's width

	protected static ref LF_DialogueConfig s_TestConfig;
	protected static string s_sLastLine;
	protected static string s_sLastListener;
	protected static int s_iSpoken;

	//------------------------------------------------------------------------------------------------
	//! The conversation a trader speaks from: his dialogue component's, else his role's.
	protected static LF_DialogueConfig ConfigOf(IEntity trader)
	{
		if (s_TestConfig)
			return s_TestConfig;

		return LF_DialogueComponent.ConfigOf(trader);
	}

	//------------------------------------------------------------------------------------------------
	//! The voiced refusal line of a trader, or null: no conversation, no line for the refusal key, or a
	//! text-only line (nothing to play).
	static LF_DialogueLine RefusalLineOf(IEntity trader)
	{
		LF_DialogueConfig config = ConfigOf(trader);
		if (!config || !config.m_aLines)
			return null;

		foreach (LF_DialogueLine line : config.m_aLines)
		{
			if (line && line.m_sForKey == LF_Outpost.REFUSE_KEY && !line.IsTextOnly())
				return line;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest character within SPEAKER_RANGE_M of `traderPos` that has a voiced refusal, or null.
	//! \param[in] customer The refused player, never the speaker.
	static IEntity FindSpeaker(IEntity customer, vector traderPos)
	{
		LF_OutpostSpeakerQuery query = new LF_OutpostSpeakerQuery();
		query.m_Skip = customer;
		query.m_vFrom = traderPos;
		query.m_fBest = SPEAKER_RANGE_M * SPEAKER_RANGE_M;
		GetGame().GetWorld().QueryEntitiesBySphere(traderPos, SPEAKER_RANGE_M, query.Collect, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		return query.m_Nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the trader at `traderPos` turned `customer` away. Speaks the refusal line to that player
	//! only; a spot with no voiced speaker stays quiet.
	//! \return True when a line was sent.
	static bool OnRefusal(IEntity customer, vector traderPos)
	{
		if (!customer)
			return false;

		IEntity speaker = FindSpeaker(customer, traderPos);
		if (!speaker)
		{
			LF_Debug.Info(FEATURE, string.Format("no voiced trader stands at %1: the refusal is a toast only", traderPos));
			return false;
		}

		return Say(speaker, customer);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `customer` hears the refusal line from `trader`: the clip plays on their client only.
	//! \return True when the line was found and sent (or counted, with no player controller behind it).
	static bool Say(IEntity trader, IEntity customer)
	{
		if (!trader || !customer)
			return false;

		if (Replication.IsRunning() && !Replication.IsServer())
			return false;

		LF_DialogueLine line = RefusalLineOf(trader);
		if (!line)
		{
			LF_Debug.Info(FEATURE, "the trader has no voiced refusal line: he stays quiet");
			return false;
		}

		s_sLastLine = line.m_sId;
		s_sLastListener = LF_Utils.GetPlayerUid(customer);
		s_iSpoken++;
		LF_Debug.Info(FEATURE, string.Format("the trader says '%1' to %2", line.m_sId, s_sLastListener));

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return true;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(customer);
		if (playerId <= 0)
			return true;

		SCR_PlayerController controller = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
		RplId traderId = LF_NetworkUtils.GetRplId(trader);
		if (controller && traderId.IsValid())
			controller.LF_OutpostSay(traderId, line.m_sId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: play `lineId` from the trader's head. Silent (logged) when the line has no recording, the
	//! trader is not on this machine, or the id is not a refusal line he holds (a client's word is never
	//! taken for a line).
	//! \return True when a clip started.
	static bool Speak(IEntity trader, string lineId)
	{
		if (!trader)
		{
			LF_Debug.Warn(FEATURE, string.Format("the trader is not on this machine: no clip for '%1'", lineId));
			return false;
		}

		LF_DialogueConfig config = ConfigOf(trader);
		LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
		if (!line || line.m_sForKey != LF_Outpost.REFUSE_KEY || line.IsTextOnly())
		{
			LF_Debug.Warn(FEATURE, string.Format("the trader has no recorded refusal line '%1': caption only", lineId));
			return false;
		}

		LF_DialogueVoice.Say(trader, config, lineId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The last refusal line spoken, who heard it (their uid), and how many were. What the tests read.
	static string LastLine()
	{
		return s_sLastLine;
	}

	//------------------------------------------------------------------------------------------------
	static string LastListener()
	{
		return s_sLastListener;
	}

	//------------------------------------------------------------------------------------------------
	static int SpokenCount()
	{
		return s_iSpoken;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: the conversation every trader speaks from, so a core test needs no pack recording.
	static void SetConfigForTests(LF_DialogueConfig config)
	{
		s_TestConfig = config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_TestConfig = null;
		s_sLastLine = "";
		s_sLastListener = "";
		s_iSpoken = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OutpostVoice's statics (LF_StaticReset finds it by reflection).
class LF_OutpostVoice_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OutpostVoice";
	}

	override void Apply()
	{
		LF_OutpostVoice.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The wire of an outpost refusal (#4640). The server tells the refused officer's own client which line to
//! play and from whom; the client finds the clip in its own copy of the conversation, so only an entity id
//! and a line id travel, and nothing goes from a client to the server.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: play refusal line `lineId` from the entity at `traderId` on this player's machine.
	void LF_OutpostSay(RplId traderId, string lineId)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_OutpostSay(traderId, lineId);
		else
			Rpc(RpcDo_LF_OutpostSay, traderId, lineId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OutpostSay(RplId traderId, string lineId)
	{
		LF_OutpostVoice.Speak(LF_NetworkUtils.FindEntityByRplId(traderId), lineId);
	}
}
