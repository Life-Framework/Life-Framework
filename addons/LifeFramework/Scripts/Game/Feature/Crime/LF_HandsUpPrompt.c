//------------------------------------------------------------------------------------------------
//! The "Put your hands up" prompt on the target's own screen (#3396). Client only. The server offers
//! it (LF_HandsUp.Offer) when the player is held up or aimed at; it hides by itself after
//! LF_HandsUp.PROMPT_MS. The key (action LF_HandsUp) answers it; ignoring it changes nothing.
//------------------------------------------------------------------------------------------------
class LF_HandsUpPrompt
{
	static const ResourceName LAYOUT = "{7B4E0C91A2D63F58}UI/Layouts/HUD/Crime/LF_HandsUpPrompt.layout";

	protected static Widget s_wRoot;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Hide();
	}

	//------------------------------------------------------------------------------------------------
	//! True while the prompt is on screen: the key then raises the hands.
	static bool IsShown()
	{
		return s_wRoot != null;
	}

	//------------------------------------------------------------------------------------------------
	static void Show()
	{
		Hide();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			s_wRoot = workspace.CreateWidgets(LAYOUT);

		if (!s_wRoot)
		{
			LF_Debug.Error(LF_HandsUp.FEATURE, "the hands-up prompt did not load; the key still answers a held-up player");
			return;
		}

		GetGame().GetCallqueue().CallLater(Hide, LF_HandsUp.PROMPT_MS, false);
		LF_Debug.Info(LF_HandsUp.FEATURE, "hands-up prompt shown");
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(Hide);

		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness: what the prompt shows now.
	static string Describe()
	{
		if (s_wRoot)
			return "shown";

		return "hidden";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HandsUpPrompt's statics (LF_StaticReset finds it by reflection).
class LF_HandsUpPrompt_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HandsUpPrompt";
	}

	override void Apply()
	{
		LF_HandsUpPrompt.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The wire of hands-up on the player's own controller. Server -> owner: the offer. Owner -> server:
//! the answer (RpcAsk_), judged by LF_HandsUp.Answer against the player id of THIS controller.
modded class SCR_PlayerController
{
	protected bool m_bLF_HandsUpListening;

	//------------------------------------------------------------------------------------------------
	//! The listener lives on the process-global InputManager, so it leaves with this controller (#2545).
	void ~SCR_PlayerController()
	{
		if (!m_bLF_HandsUpListening || !GetGame())
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener("LF_HandsUp", EActionTrigger.DOWN, LF_OnHandsUpKey);

		m_bLF_HandsUpListening = false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		LF_RegisterHandsUpListener();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RegisterHandsUpListener()
	{
		if (m_bLF_HandsUpListening || GetGame().GetPlayerController() != this)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("LF_HandsUp", EActionTrigger.DOWN, LF_OnHandsUpKey);
		m_bLF_HandsUpListening = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The key: with the prompt up it raises the hands; with the hands up it lowers them.
	protected void LF_OnHandsUpKey()
	{
		if (LF_HandsUpPrompt.IsShown())
		{
			LF_HandsUpPrompt.Hide();
			LF_HandsUpAsk(LF_HandsUp.CHOICE_RAISE);
			return;
		}

		SCR_ChimeraCharacter me = SCR_ChimeraCharacter.Cast(GetControlledEntity());
		if (me && me.LF_IsHandsUp())
			LF_HandsUpAsk(LF_HandsUp.CHOICE_LOWER);
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry (also the harness): this player's own answer. A listen host decides in place.
	void LF_HandsUpAsk(int choice)
	{
		if (Replication.IsServer())
		{
			LF_HandsUp.Answer(GetPlayerId(), choice);
			return;
		}

		Rpc(RpcAsk_LF_HandsUp, choice);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HandsUp(int choice)
	{
		// Only a player with a body of their own answers; LF_HandsUp.Answer then needs a live offer for this controller's id.
		if (GetControlledEntity() == null)
			return;

		LF_HandsUp.Answer(GetPlayerId(), choice);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show this player the prompt.
	void LF_HandsUpOffer(int actorId)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_HandsUpOffer(actorId);
		else
			Rpc(RpcDo_LF_HandsUpOffer, actorId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_HandsUpOffer(int actorId)
	{
		LF_HandsUpPrompt.Show();
	}

	//------------------------------------------------------------------------------------------------
	//! Harness only: ask the server to offer the prompt to `targetId`, standing in for a held aim.
	void LF_HandsUpAskOffer(int targetId)
	{
		if (Replication.IsServer())
		{
			if (LF_DevGate.CheckHarnessRequest(GetControlledEntity(), "handsup offer"))
				LF_HandsUp.Offer(targetId, GetPlayerId());

			return;
		}

		Rpc(RpcAsk_LF_HandsUpOffer, targetId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HandsUpOffer(int targetId)
	{
		if (LF_DevGate.CheckHarnessRequest(GetControlledEntity(), "handsup offer"))
			LF_HandsUp.Offer(targetId, GetPlayerId());
	}
}
