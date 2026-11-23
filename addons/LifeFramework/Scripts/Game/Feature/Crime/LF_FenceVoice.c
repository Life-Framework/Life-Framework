// LF_FenceVoice.c - the fence speaks at his counter (#4620, slice of #4440).
// The fence shop turns a buyer away from a gated tool with one voiced line and sells the drill with
// another. The shop calls `OnBuy` on the server with what its buy path decided; the line is found in the
// fence NPC's own conversation (his role dialogue with the pack's patch over it), so a server with no
// recording for it simply stays quiet. The buyer's client plays the clip from the fence's head and shows
// the line as a toast caption, the same way a told rumor does (LF_RumorVoice).

//------------------------------------------------------------------------------------------------
class LF_FenceVoice
{
	static const string FEATURE = "Fence";
	//! The line that turns a buyer away from a gated tool.
	static const string LINE_REFUSE = "refuse_drill";
	//! The line that goes with a drill sale.
	static const string LINE_SELL = "sell_drill";
	static const ResourceName DRILL = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";
	static const string TITLE_KEY = "#LF-Shop_Name_Fence";
	//! Seconds the caption stays after the clip ends.
	static const float CAPTION_PAD_SECONDS = 1.5; // lf-fixed: reading time
	//! Caption time for a line with no recording.
	static const float CAPTION_SILENT_SECONDS = 6.0; // lf-fixed: reading time

	protected static ref LF_DialogueConfig s_TestConfig;
	protected static string s_sLastLine;
	protected static int s_iSpoken;

	//------------------------------------------------------------------------------------------------
	//! The line a shop decision speaks, or empty. A refusal for record turns the buyer away from any
	//! gated tool; only the drill has a sale line.
	//! \param reason What the shop's buy path returned.
	static string LineFor(LF_EShopRefusal reason, ResourceName prefab)
	{
		if (reason == LF_EShopRefusal.NOT_TRUSTED)
			return LINE_REFUSE;

		if (reason == LF_EShopRefusal.NONE && LF_Utils.SamePrefab(prefab, DRILL))
			return LINE_SELL;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds the caption toast holds: the clip's length and a short read, or a fixed read for a
	//! line with no clip.
	static float CaptionSeconds(LF_DialogueLine line)
	{
		if (!line || line.IsTextOnly())
			return CAPTION_SILENT_SECONDS;

		return line.m_fSeconds + CAPTION_PAD_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! The conversation the fence speaks from: an authored dialogue on him, else his role's.
	static LF_DialogueConfig ConfigOf(IEntity fence)
	{
		if (s_TestConfig)
			return s_TestConfig;

		return LF_DialogueComponent.ConfigOf(fence);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the shop's buy path decided `reason` for `buyer` at `fence`. Speaks the matching line on the
	//! buyer's machine; nothing for a decision with no line.
	//! \return True when a line was spoken.
	static bool OnBuy(IEntity fence, IEntity buyer, LF_EShopRefusal reason, ResourceName prefab)
	{
		string lineId = LineFor(reason, prefab);
		if (lineId.IsEmpty())
			return false;

		return Say(fence, buyer, lineId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `buyer` hears `lineId` from `fence`: the clip plays on their client and the caption shows as
	//! a toast. A line the fence's conversation does not hold is logged and dropped.
	//! \return True when the line was found and sent.
	static bool Say(IEntity fence, IEntity buyer, string lineId)
	{
		if (!fence || !buyer)
			return false;

		LF_DialogueLine line = LF_DialogueFlow.FindLine(ConfigOf(fence), lineId);
		if (!line)
		{
			LF_Debug.Info(FEATURE, string.Format("the fence has no recorded line '%1': he stays quiet", lineId));
			return false;
		}

		s_sLastLine = lineId;
		s_iSpoken++;
		LF_Debug.Info(FEATURE, string.Format("the fence says '%1' to %2", lineId, LF_Utils.GetPlayerUid(buyer)));

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return true;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(buyer);
		if (playerId <= 0)
			return true;

		SCR_PlayerController controller = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerId));
		RplId fenceId = LF_NetworkUtils.GetRplId(fence);
		if (controller && fenceId.IsValid())
			controller.LF_FenceSay(fenceId, lineId);

		LF_NotificationManagerComponent.NotifyPlayer(playerId, TITLE_KEY, line.m_sTextKey, CaptionSeconds(line), LF_ENotificationType.INFO);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: play `lineId` from the fence's head. Silent (logged) when the line has no recording or the
	//! fence is not on this machine.
	//! \return True when a clip started.
	static bool Speak(IEntity fence, string lineId)
	{
		if (!fence)
		{
			LF_Debug.Warn(FEATURE, string.Format("the fence is not on this machine: no clip for '%1'", lineId));
			return false;
		}

		LF_DialogueConfig config = ConfigOf(fence);
		LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
		if (!line || line.IsTextOnly())
		{
			LF_Debug.Warn(FEATURE, string.Format("the fence has no recorded line '%1': caption only", lineId));
			return false;
		}

		LF_DialogueVoice.Say(fence, config, lineId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The last line spoken to a buyer, and how many were. What the tests read.
	static string LastLine()
	{
		return s_sLastLine;
	}

	//------------------------------------------------------------------------------------------------
	static int SpokenCount()
	{
		return s_iSpoken;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: the conversation the fence speaks from, so a core test needs no pack recording.
	static void SetConfigForTests(LF_DialogueConfig config)
	{
		s_TestConfig = config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_TestConfig = null;
		s_sLastLine = "";
		s_iSpoken = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FenceVoice's statics (LF_StaticReset finds it by reflection).
class LF_FenceVoice_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FenceVoice";
	}

	override void Apply()
	{
		LF_FenceVoice.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The wire of a fence line (#4620). The server tells the buyer's own client which line to play and from
//! whom; the client finds the clip in its own copy of the conversation, so only an entity id and a line id
//! travel. Nothing goes from a client to the server.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: play fence line `lineId` from the entity at `fenceId` on this player's machine.
	void LF_FenceSay(RplId fenceId, string lineId)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_FenceSay(fenceId, lineId);
		else
			Rpc(RpcDo_LF_FenceSay, fenceId, lineId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_FenceSay(RplId fenceId, string lineId)
	{
		LF_FenceVoice.Speak(LF_NetworkUtils.FindEntityByRplId(fenceId), lineId);
	}
}
