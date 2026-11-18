// LF_ResidentTalk.c - the greeting follows how a resident feels about you (#2908, epic #1012).
// Server side. LF_Dialogue.Start asks StartNode which node a conversation opens on. A resident whose
// dialogue has a `greet_<word>` node, `word` being the memory word (LF_ResidentMemory.Tier), opens on it:
// a regular is greeted warmly, a resident who saw a theft turns away. A pack tunes one resident with a
// node `<residentId>_greet_<word>` in its dialogue patch. The client is only ever told the node and
// line ids; the word and the score never leave the server (M6 decision 7).

class LF_ResidentTalk
{
	static const string FEATURE = "Residents";
	static const string GREET_PREFIX = "greet_";

	//------------------------------------------------------------------------------------------------
	//! The core greeting node id for a memory word.
	static string CoreNodeId(string word)
	{
		return GREET_PREFIX + word;
	}

	//------------------------------------------------------------------------------------------------
	//! The pack's greeting node id for one resident and word.
	static string ResidentNodeId(string residentId, string word)
	{
		return residentId + "_" + GREET_PREFIX + word;
	}

	//------------------------------------------------------------------------------------------------
	//! The node a talk opens on: the resident's own node for the word, else the core node for the word,
	//! else the dialogue's own start (a dialogue with no tier nodes greets as it always did).
	static string StartNodeFor(LF_DialogueConfig config, string residentId, string word)
	{
		if (!config)
			return string.Empty;

		if (!residentId.IsEmpty() && LF_DialogueFlow.FindNode(config, ResidentNodeId(residentId, word)))
			return ResidentNodeId(residentId, word);

		if (LF_DialogueFlow.FindNode(config, CoreNodeId(word)))
			return CoreNodeId(word);

		return config.m_sStart;
	}

	//------------------------------------------------------------------------------------------------
	//! The line id the resident says on opening, for tests and logs: the start node's line.
	static string LineFor(LF_DialogueConfig config, string residentId, string word)
	{
		LF_DialogueNode node = LF_DialogueFlow.FindNode(config, StartNodeFor(config, residentId, word));
		if (!node)
			return string.Empty;

		return node.m_sLine;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the node a player's talk with `npc` opens on. A character that is not a live resident
	//! gets the dialogue's own start.
	static string StartNode(IEntity player, IEntity npc, LF_DialogueConfig config)
	{
		if (!config)
			return string.Empty;

		string residentId = LF_ResidentSpawner.ResidentIdOf(npc);
		if (residentId.IsEmpty())
			return config.m_sStart;

		string uid = LF_Utils.GetPlayerUid(player);
		if (uid.IsEmpty())
			return config.m_sStart;

		string word = LF_ResidentMemory.Tier(residentId, uid);
		string node = StartNodeFor(config, residentId, word);
		LF_Debug.Info(FEATURE, string.Format("talk: resident '%1' opens on '%2' (%3)", residentId, node, word));
		return node;
	}
}
