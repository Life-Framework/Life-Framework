//------------------------------------------------------------------------------------------------
//! Conversations with NPCs: the server side. The server owns where each player is in a
//! conversation. A client asks to start one with an NPC and then names a reply by its index; the
//! server checks the player still stands at the NPC and that it offered that reply, runs the
//! reply's action, and answers with ids only: the node, what the NPC says, and which replies are
//! offered. Camera, captions and voice are the client's (LF_DialogueScene, LF_DialogueVoice).
//! Sessions are not saved: a restart or a disconnect ends the conversation.
//------------------------------------------------------------------------------------------------
class LF_DialogueSession : Managed
{
	//! Server-local id of the NPC; never sent.
	EntityID m_NpcId;
	string m_sNode;
	//! Replies offered at m_sNode, as sent to the client.
	int m_iMask;
}

class LF_Dialogue
{
	static const string FEATURE = "Dialogue";
	//! The talk action shows within 3 m; the server allows a little more for lag.
	static const float REACH = 4.0; // lf-fixed: follows the talk action's own 3 m, not a balance value

	protected static ref map<int, ref LF_DialogueSession> s_mSessions;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSessions = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<int, ref LF_DialogueSession> Sessions()
	{
		if (!s_mSessions)
			s_mSessions = new map<int, ref LF_DialogueSession>();

		return s_mSessions;
	}

	//------------------------------------------------------------------------------------------------
	//! The session of a player, or null.
	static LF_DialogueSession SessionOf(int playerId)
	{
		return Sessions().Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	static bool InReach(IEntity player, IEntity npc)
	{
		return player && npc && vector.Distance(player.GetOrigin(), npc.GetOrigin()) <= REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player asks to talk to an NPC. \return True when the conversation opened.
	static bool Start(SCR_PlayerController controller, IEntity npc)
	{
		if (!Replication.IsServer() || !controller)
			return false;

		int playerId = controller.GetPlayerId();
		IEntity player = controller.GetControlledEntity();
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		if (!config || !InReach(player, npc))
		{
			LF_Debug.Warn(FEATURE, string.Format("start refused for player %1: no playable dialogue on that NPC, or not within %2 m", playerId, REACH));
			return false;
		}

		LF_DialogueSession session = new LF_DialogueSession();
		session.m_NpcId = npc.GetID();
		Sessions().Set(playerId, session);
		LF_Debug.Info(FEATURE, string.Format("player %1 talks to '%2'", playerId, config.m_sId));
		Enter(controller, session, npc, config, config.m_sStart, null);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player picks reply `index` of the node it is on. \return True when it was taken.
	static bool Choose(SCR_PlayerController controller, int index)
	{
		if (!Replication.IsServer() || !controller)
			return false;

		int playerId = controller.GetPlayerId();
		LF_DialogueSession session = SessionOf(playerId);
		if (!session)
		{
			LF_Debug.Warn(FEATURE, string.Format("reply %1 refused for player %2: not in a conversation", index, playerId));
			return false;
		}

		IEntity player = controller.GetControlledEntity();
		IEntity npc = GetGame().GetWorld().FindEntityByID(session.m_NpcId);
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		if (!config || !InReach(player, npc))
		{
			LF_Debug.Warn(FEATURE, string.Format("reply %1 refused for player %2: the NPC is gone or out of reach; conversation ended", index, playerId));
			End(controller, true);
			return false;
		}

		LF_DialogueNode node = LF_DialogueFlow.FindNode(config, session.m_sNode);
		LF_DialogueOption option = LF_DialogueFlow.Pick(node, session.m_iMask, index);
		// The condition is asked again: the world may have changed since the replies were offered.
		if (!option || !LF_DialogueHook.Offers(option, player, npc))
		{
			LF_Debug.Warn(FEATURE, string.Format("reply %1 refused for player %2 at '%3/%4': not offered", index, playerId, config.m_sId, session.m_sNode));
			return false;
		}

		array<string> spoken = {};
		if (!option.m_sAction.IsEmpty())
		{
			LF_DialogueHook hook = LF_DialogueHook.Find(option.m_sAction);
			if (hook)
				hook.Run(player, npc, option.m_sParam, spoken);
		}

		LF_Debug.Info(FEATURE, string.Format("player %1 at '%2/%3' picked reply %4 (action '%5', said %6 key(s))", playerId, config.m_sId, session.m_sNode, index, option.m_sAction, spoken.Count()));
		if (option.m_sNext.IsEmpty())
		{
			// A reply that ends the talk can still have had an answer; show it, then close.
			Sessions().Remove(playerId);
			string last = LF_DialogueFlow.SpokenList(config, null, spoken);
			controller.LF_DialogueShow(LF_NetworkUtils.GetRplId(npc), "", last, 0);
			Overhear(playerId, npc, config, last);
			return true;
		}

		Enter(controller, session, npc, config, option.m_sNext, spoken);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move a session to a node and tell the client what the NPC says and what is offered.
	protected static void Enter(SCR_PlayerController controller, LF_DialogueSession session, IEntity npc, LF_DialogueConfig config, string nodeId, array<string> spoken)
	{
		LF_DialogueNode node = LF_DialogueFlow.FindNode(config, nodeId);
		IEntity player = controller.GetControlledEntity();
		array<bool> allowed = {};
		if (node && node.m_aOptions)
		{
			foreach (LF_DialogueOption option : node.m_aOptions)
				allowed.Insert(LF_DialogueHook.Offers(option, player, npc));
		}

		session.m_sNode = nodeId;
		session.m_iMask = LF_DialogueFlow.OptionMask(node, allowed);
		string list = LF_DialogueFlow.SpokenList(config, node, spoken);
		if (session.m_iMask == 0)
			Sessions().Remove(controller.GetPlayerId());

		controller.LF_DialogueShow(LF_NetworkUtils.GetRplId(npc), nodeId, list, session.m_iMask);
		Overhear(controller.GetPlayerId(), npc, config, list);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the talk is over (the player left, walked off, or the NPC is gone).
	//! \param tellClient Close the scene on the client too (it did not ask for the end itself).
	static void End(SCR_PlayerController controller, bool tellClient)
	{
		if (!Replication.IsServer() || !controller)
			return;

		int playerId = controller.GetPlayerId();
		if (Sessions().Contains(playerId))
		{
			Sessions().Remove(playerId);
			LF_Debug.Info(FEATURE, string.Format("player %1 left the conversation", playerId));
		}

		if (tellClient)
			controller.LF_DialogueClose();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the NPC says the line voiced for `key` (its `for_key`) to everyone within its hear
	//! range, outside any conversation: another feature's moment, such as a robbery's begging
	//! (LF_RobHold). A key with no voiced line is logged and not sent.
	static void Announce(IEntity npc, string key)
	{
		if (!Replication.IsServer())
			return;

		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		if (!config)
		{
			LF_Debug.Warn(FEATURE, string.Format("'%1' not said: the NPC has no playable dialogue", key));
			return;
		}

		string line = LF_DialogueFlow.SpokenFor(config, key);
		if (line == key)
		{
			LF_Debug.Warn(FEATURE, string.Format("'%1' not said: '%2' has no line voiced for that key", key, config.m_sId));
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("'%1' says '%2' to whoever is near", config.m_sId, line));
		Overhear(0, npc, config, line);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: players standing near the NPC hear what it says to the talker.
	protected static void Overhear(int talkerId, IEntity npc, LF_DialogueConfig config, string list)
	{
		if (config.m_fHearRange <= 0 || list.IsEmpty())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		RplId npcId = LF_NetworkUtils.GetRplId(npc);
		int heard = 0;
		foreach (int id : ids)
		{
			if (id == talkerId)
				continue;

			IEntity body = players.GetPlayerControlledEntity(id);
			if (!body || vector.Distance(body.GetOrigin(), npc.GetOrigin()) > config.m_fHearRange)
				continue;

			SCR_PlayerController other = SCR_PlayerController.Cast(players.GetPlayerController(id));
			if (!other)
				continue;

			other.LF_DialogueHear(npcId, list);
			heard++;
		}

		if (heard > 0)
			LF_Debug.Info(FEATURE, string.Format("%1 bystander(s) hear '%2' say: %3", heard, config.m_sId, list));
	}
}
