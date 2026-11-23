// Harness verb `rumortell [rumorId]` (#4569), for the session picture of a told rumor. It runs on the client
// the verb is sent to: picks a gossip rumor (or the named row) from this machine's rumor config, plays its
// recorded clip through `LF_RumorVoice.Speak` (the call the server's RPC ends in) from the nearest other
// character within 10 m, or from the player's own head when nobody else is near (a client cannot spawn a
// replicated NPC), and shows the caption toast. The server, the map mark and the RPC are covered by the
// tests and by the real bench; this is what the player sees and hears on their own machine. Local to the
// client, no state. `ok told=<id> place=<place> clip=<1|0> speaker=<own|other>` or an error.

//------------------------------------------------------------------------------------------------
//! Collects the characters near a point other than one body.
class LF_RumorHarnessQuery
{
	IEntity m_Skip;
	IEntity m_Nearest;
	vector m_vFrom;
	float m_fBest = 100; // 10 m squared

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (!entity || entity == m_Skip)
			return true;

		if (!SCR_ChimeraCharacter.Cast(entity))
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
class LF_RumorHarness
{
	//------------------------------------------------------------------------------------------------
	//! The named row, or the first row a gossip tells.
	protected static LF_RumorDef Choose(string rumorId)
	{
		if (!rumorId.IsEmpty())
			return LF_RumorPool.Find(rumorId);

		LF_RumorsConfig config = LF_RumorsConfig.Get();
		if (!config || !config.m_aRumors)
			return null;

		foreach (LF_RumorDef def : config.m_aRumors)
		{
			if (def && !def.m_bCrime && def.m_aGivers && def.m_aGivers.Find("gossip") != -1)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "ok ..." or "error ..." for the harness answer.
	static string Tell(string rumorId)
	{
		IEntity body = LF_Utils.GetLocalCharacter();
		if (!body)
			return "error no_character";

		LF_RumorDef def = Choose(rumorId);
		if (!def)
			return "error no_rumor_to_tell";

		LF_RumorHarnessQuery query = new LF_RumorHarnessQuery();
		query.m_Skip = body;
		query.m_vFrom = body.GetOrigin();
		GetGame().GetWorld().QueryEntitiesBySphere(query.m_vFrom, 10, query.Collect, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		IEntity speaker = query.m_Nearest;
		string who = "other";
		if (!speaker)
		{
			speaker = body;
			who = "own";
		}

		int clip;
		if (LF_RumorVoice.Speak(speaker, def))
			clip = 1;

		LF_DialogueConfig voice;
		LF_DialogueLine line = LF_RumorVoice.FindLine(def, voice);
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (manager)
			manager.ShowNotificationToast("#LF-Gossip_Title", def.m_sLineKey, LF_RumorVoice.CaptionSeconds(line), LF_ENotificationType.INFO);

		return string.Format("ok told=%1 place=%2 clip=%3 speaker=%4", def.m_sId, def.m_sPlaceId, clip, who);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "rumortell")
			return super.Handle(cmd);

		string result = LF_RumorHarness.Tell(cmd.Arg1);
		if (result.IndexOf("ok") == 0)
			LF_AgentHarness.Answer(cmd.Id, "ok", result.Substring(3, result.Length() - 3));
		else
			LF_AgentHarness.Answer(cmd.Id, "error", result.Substring(6, result.Length() - 6));

		return true;
	}
}
