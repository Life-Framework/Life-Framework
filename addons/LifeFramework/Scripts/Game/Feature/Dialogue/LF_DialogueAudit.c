// LF_DialogueAudit.c - counts the conversations a player would find missing, in a running mission.
//
// Two kinds (the "so much missing dialogue" report of 2026-11-19):
//   text  a speaker name, caption or reply whose key has no row in the loaded string tables (it shows as
//         the raw `#LF-...` id), or an empty caption key;
//   talk  an NPC near the player whose talk opens nothing (no authored dialogue and no role
//         conversation), or opens a sheet with one reply or none after the greeting (a placeholder).
// The offline twin is tools/validation/validate-dialogue-refs.mjs; this one reads what the mission
// really loaded (pack overlays, patches, the role table) on a client.

//! One sphere query's characters; lives for the query only.
class LF_DialogueAuditQuery
{
	ref array<IEntity> m_aFound = {};

	bool Collect(IEntity entity)
	{
		if (ChimeraCharacter.Cast(entity))
			m_aFound.Insert(entity);

		return true;
	}
}

class LF_DialogueAudit
{
	static const string FEATURE = "Dialogue";
	//! A greeting with this many replies or fewer is a placeholder sheet (only "Goodbye").
	static const int THIN_REPLIES = 1;

	//------------------------------------------------------------------------------------------------
	//! True when the key shows as itself: empty, or no row in the loaded tables.
	static bool IsUntranslated(string key)
	{
		if (key.IsEmpty())
			return true;

		string text = WidgetManager.Translate(key);
		return text.IsEmpty() || text == key;
	}

	//------------------------------------------------------------------------------------------------
	//! Every key of a config that shows raw, one line each ("<dialogue>: <where> <key>").
	static void TextGaps(LF_DialogueConfig config, notnull array<string> gaps)
	{
		if (!config)
			return;

		if (IsUntranslated(config.m_sSpeakerKey))
			gaps.Insert(string.Format("%1: speaker name '%2'", config.m_sId, config.m_sSpeakerKey));

		if (config.m_aLines)
		{
			foreach (LF_DialogueLine line : config.m_aLines)
			{
				if (line && IsUntranslated(line.m_sTextKey))
					gaps.Insert(string.Format("%1: line %2 caption '%3'", config.m_sId, line.m_sId, line.m_sTextKey));
			}
		}

		if (!config.m_aNodes)
			return;

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node)
				continue;

			if (LF_DialogueFlow.IsRaw(node.m_sLine) && IsUntranslated(node.m_sLine))
				gaps.Insert(string.Format("%1: node %2 caption '%3'", config.m_sId, node.m_sId, node.m_sLine));

			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				if (option && IsUntranslated(option.m_sTextKey))
					gaps.Insert(string.Format("%1: node %2 reply '%3'", config.m_sId, node.m_sId, option.m_sTextKey));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replies on the greeting a talk opens with.
	static int GreetingReplies(LF_DialogueConfig config)
	{
		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		if (!start || !start.m_aOptions)
			return 0;

		return start.m_aOptions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every dialogue the mission registers, layered with its pack patches, plus every role conversation.
	static void MissionConfigs(notnull array<ref LF_DialogueConfig> configs)
	{
		LF_DialogueIndexConfig index = LF_DialogueRegistry.Load();
		if (index && index.m_aEntries)
		{
			foreach (LF_DialogueIndexEntry entry : index.m_aEntries)
			{
				if (!entry || entry.m_Conf.IsEmpty())
					continue;

				LF_DialogueConfig config = LF_DialogueConfig.Layered(entry.m_Conf);
				if (config)
					configs.Insert(config);
				else
					LF_Debug.Warn(FEATURE, string.Format("audit: dialogue '%1' (%2) does not load", entry.m_sId, entry.m_Conf));
			}
		}

		foreach (LF_RoleDef role : LF_Roles.All())
		{
			// A role that plays a registered dialogue is already in the index above.
			if (!role.m_sDialogueId.IsEmpty())
				continue;

			LF_DialogueConfig roleConfig = LF_Roles.DialogueFor(role.m_sId);
			if (roleConfig)
				configs.Insert(roleConfig);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! NPC characters within `radius` m of `center` (players excluded).
	static void NpcsNear(vector center, float radius, notnull array<IEntity> npcs)
	{
		LF_DialogueAuditQuery query = new LF_DialogueAuditQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, query.Collect, null, EQueryEntitiesFlags.ALL);
		PlayerManager players = GetGame().GetPlayerManager();
		foreach (IEntity entity : query.m_aFound)
		{
			if (players && players.GetPlayerIdFromControlledEntity(entity) > 0)
				continue;

			npcs.Insert(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string Describe(IEntity npc)
	{
		string name = npc.GetName();
		string prefab = LF_Utils.GetPrefabName(npc);
		int slash = prefab.LastIndexOf("/");
		if (slash >= 0)
			prefab = prefab.Substring(slash + 1, prefab.Length() - slash - 1); // lf-bounded: a prefab path
		if (name.IsEmpty())
			return prefab;

		return name + " (" + prefab + ")";
	}

	//------------------------------------------------------------------------------------------------
	//! Runs both audits; logs one line per finding and returns the summary
	//! "text=<n> notalk=<n> thin=<n> npcs=<n> dialogues=<n>".
	static string Run(vector center, float radius)
	{
		array<ref LF_DialogueConfig> configs = {};
		MissionConfigs(configs);
		array<string> gaps = {};
		foreach (LF_DialogueConfig config : configs)
			TextGaps(config, gaps);

		foreach (string gap : gaps)
			LF_Debug.Warn(FEATURE, "audit text: " + gap);

		array<IEntity> npcs = {};
		NpcsNear(center, radius, npcs);
		int noTalk;
		int thin;
		foreach (IEntity npc : npcs)
		{
			LF_DialogueConfig talk = LF_DialogueComponent.ConfigOf(npc);
			if (!talk)
			{
				noTalk++;
				LF_Debug.Warn(FEATURE, string.Format("audit talk: %1 at %2 has no conversation", Describe(npc), npc.GetOrigin()));
				continue;
			}

			int replies = GreetingReplies(talk);
			if (replies <= THIN_REPLIES)
			{
				thin++;
				LF_Debug.Warn(FEATURE, string.Format("audit talk: %1 opens '%2' with %3 replies", Describe(npc), talk.m_sId, replies));
			}
			else
			{
				LF_Debug.Info(FEATURE, string.Format("audit talk: %1 opens '%2' with %3 replies", Describe(npc), talk.m_sId, replies));
			}
		}

		string summary = string.Format("text=%1 notalk=%2 thin=%3 npcs=%4 dialogues=%5", gaps.Count(), noTalk, thin, npcs.Count(), configs.Count());
		LF_Debug.Info(FEATURE, "audit: " + summary);
		return summary;
	}
}

//------------------------------------------------------------------------------------------------
//! `talkaudit [radius]`: LF_DialogueAudit.Run around the local player (default 600 m).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "talkaudit")
			return super.Handle(cmd);

		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_player");
			return true;
		}

		float radius = cmd.Arg1.ToFloat();
		if (radius <= 0)
			radius = 600;

		LF_AgentHarness.Answer(cmd.Id, "ok", LF_DialogueAudit.Run(player.GetOrigin(), radius));
		return true;
	}
}
