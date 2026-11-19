//------------------------------------------------------------------------------------------------
//! What the server does with a player's answer to the hands-up prompt (LF_HandsUp.Decide).
enum LF_EHandsUpAnswer
{
	IGNORED,
	RAISE,
	REFUSED,
	LOWER
}

//------------------------------------------------------------------------------------------------
//! Hands up, the server half (#3396, slice H2 of #1024; the rules are LF_HandsUpRules, #3394).
//! A hold-up or a held aim (LF_OnHoldUp, LF_OnAimedAt) offers the TARGET a short prompt on their own
//! screen. The target chooses: the key sends `RpcAsk_LF_HandsUp` on their own controller, the server
//! checks that an offer is live for that player id (taken from the controller, never the payload),
//! and only then raises the hands. Ignoring the prompt forces nothing. The state is a replicated
//! bool on the character (`SCR_ChimeraCharacter.LF_IsHandsUp`), so every client and a late joiner
//! read it; the owner's machine plays the base-game halt gesture until it ends.
//! It ends when the target walks off, draws a weapon, lowers them or the timeout runs out
//! (LF_HandsUpRules.Tick, polled on a server timer).
//------------------------------------------------------------------------------------------------
class LF_HandsUp
{
	static const string FEATURE = "HandsUp";
	//! Base-game halt gesture (Configs/Commanding/Gestures): a raised open hand, held until stopped.
	static const int GESTURE_ID = 2; // lf-fixed: vanilla ECharacterGestures.COMMAND_STOP, no two-hand pose exists in the base game
	//! How long an offer stays live, in ms (the prompt hides at the same time on the client).
	static const int PROMPT_MS = 8000; // lf-fixed: prompt lifetime, UI pacing
	//! After an offer, the same player is not offered again for this long (an ignored prompt is not nagged).
	static const int REOFFER_MS = 20000; // lf-fixed: anti-nag pacing, UI
	static const int TICK_MS = 500; // lf-fixed: server poll of the rules
	static const int CHOICE_REFUSE = 0;
	static const int CHOICE_RAISE = 1;
	static const int CHOICE_LOWER = 2;

	protected static ref map<int, ref LF_HandsUpRules> s_mRules;
	protected static ref map<int, int> s_mOfferUntil;
	protected static ref map<int, int> s_mOfferNext;
	protected static ref LF_HandsUpHoldUpSubscriber s_HoldUp;
	protected static ref LF_HandsUpAimSubscriber s_Aim;
	protected static bool s_bSubscribed;
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning && GetGame())
			GetGame().GetCallqueue().Remove(Tick);

		s_mRules = null;
		s_mOfferUntil = null;
		s_mOfferNext = null;
		s_HoldUp = null;
		s_Aim = null;
		s_bSubscribed = false;
		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen for hold-ups and held aims. Safe to call twice; the bus table is cleared at
	//! world end, so the game mode calls it again each world.
	static void EnsureSubscribed()
	{
		if (!Replication.IsServer() || s_bSubscribed)
			return;

		s_HoldUp = new LF_HandsUpHoldUpSubscriber();
		s_Aim = new LF_HandsUpAimSubscriber();
		LF_EventBus.Subscribe(LF_OnHoldUp, s_HoldUp);
		LF_EventBus.Subscribe(LF_OnAimedAt, s_Aim);
		s_bSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is an offer made until untilMs still live at nowMs? 0 means none.
	static bool OfferLive(int untilMs, int nowMs)
	{
		return untilMs != 0 && nowMs - untilMs < 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: what a player's choice does. A raise needs a live offer and hands that are down; a lower
	//! needs hands that are up; a refusal only closes the offer. Anything else changes nothing.
	static LF_EHandsUpAnswer Decide(int choice, bool offerLive, bool raised)
	{
		if (choice == CHOICE_RAISE)
		{
			if (offerLive && !raised)
				return LF_EHandsUpAnswer.RAISE;

			return LF_EHandsUpAnswer.IGNORED;
		}

		if (choice == CHOICE_LOWER)
		{
			if (raised)
				return LF_EHandsUpAnswer.LOWER;

			return LF_EHandsUpAnswer.IGNORED;
		}

		if (choice == CHOICE_REFUSE && offerLive)
			return LF_EHandsUpAnswer.REFUSED;

		return LF_EHandsUpAnswer.IGNORED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: is this player's hands-up state up?
	static bool IsRaised(int playerId)
	{
		return s_mRules && s_mRules.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a hold-up or held aim by actorUid on targetUid. Offers the target the prompt.
	static void OfferByUids(string actorUid, string targetUid)
	{
		int targetId = PlayerIdOfUid(targetUid);
		if (targetId <= 0)
			return;

		Offer(targetId, PlayerIdOfUid(actorUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show targetId the prompt unless it is already up, already shown, or shown lately.
	//! \return True when an offer went out.
	static bool Offer(int targetId, int actorId)
	{
		if (!Replication.IsServer() || targetId <= 0)
			return false;

		if (!s_mOfferUntil)
			s_mOfferUntil = new map<int, int>();

		if (!s_mOfferNext)
			s_mOfferNext = new map<int, int>();

		int now = System.GetTickCount();
		if (IsRaised(targetId) || OfferLive(s_mOfferUntil.Get(targetId), now) || OfferLive(s_mOfferNext.Get(targetId), now))
			return false;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(targetId));
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, string.Format("offer to player %1 dropped: no controller", targetId));
			return false;
		}

		s_mOfferUntil.Set(targetId, now + PROMPT_MS);
		s_mOfferNext.Set(targetId, now + REOFFER_MS);
		controller.LF_HandsUpOffer(actorId);
		LF_Debug.Info(FEATURE, string.Format("offered hands-up to player %1 (aimed by %2)", targetId, actorId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's own answer, from their own controller. \return what it did.
	static LF_EHandsUpAnswer Answer(int playerId, int choice)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return LF_EHandsUpAnswer.IGNORED;

		int now = System.GetTickCount();
		bool live = false;
		if (s_mOfferUntil)
			live = OfferLive(s_mOfferUntil.Get(playerId), now);

		LF_EHandsUpAnswer what = Decide(choice, live, IsRaised(playerId));
		if (what == LF_EHandsUpAnswer.IGNORED)
		{
			LF_Debug.Info(FEATURE, string.Format("player %1 answer %2 ignored (offer live %3)", playerId, choice, live));
			return what;
		}

		if (what == LF_EHandsUpAnswer.REFUSED)
		{
			s_mOfferUntil.Set(playerId, 0);
			LF_Debug.Info(FEATURE, string.Format("player %1 refused: nothing changes", playerId));
			return what;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return LF_EHandsUpAnswer.IGNORED;

		if (what == LF_EHandsUpAnswer.LOWER)
		{
			LF_HandsUpRules existing = s_mRules.Get(playerId);
			if (existing && existing.Lower(playerId))
				Finish(playerId, chimera, existing.GetEndReason());

			return what;
		}

		LF_HandsUpRules rules = new LF_HandsUpRules();
		if (!rules.Answer(playerId, playerId, true, character.GetOrigin(), now))
			return LF_EHandsUpAnswer.IGNORED;

		if (!s_mRules)
			s_mRules = new map<int, ref LF_HandsUpRules>();

		s_mRules.Set(playerId, rules);
		s_mOfferUntil.Set(playerId, 0);
		chimera.LF_SetHandsUp(true);
		LF_Debug.Info(FEATURE, string.Format("player %1 raised their hands", playerId));
		Start();
		return what;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Start()
	{
		if (s_bRunning)
			return;

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_mRules || s_mRules.Count() == 0)
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bRunning = false;
			return;
		}

		array<int> ids = {};
		foreach (int heldId, LF_HandsUpRules held : s_mRules)
			ids.Insert(heldId);

		int now = System.GetTickCount();
		foreach (int playerId : ids)
		{
			LF_HandsUpRules rules = s_mRules.Get(playerId);
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
			if (!chimera || !rules)
			{
				s_mRules.Remove(playerId);
				continue;
			}

			LF_HandsUpEndReason reason = rules.Tick(character.GetOrigin(), LF_HoldUp.IsArmed(character), now);
			if (reason != LF_HandsUpEndReason.NONE)
				Finish(playerId, chimera, reason);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(int playerId, SCR_ChimeraCharacter chimera, LF_HandsUpEndReason reason)
	{
		if (s_mRules)
			s_mRules.Remove(playerId);

		chimera.LF_SetHandsUp(false);
		LF_Debug.Info(FEATURE, string.Format("player %1 hands down: %2", playerId, typename.EnumToString(LF_HandsUpEndReason, reason)));
	}

	//------------------------------------------------------------------------------------------------
	//! The player id behind an account uid, or 0.
	static int PlayerIdOfUid(string uid)
	{
		if (uid.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			if (LF_Utils.GetPlayerUID(id) == uid)
				return id;
		}

		return 0;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HandsUpHoldUpSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnHoldUp holdUp = LF_OnHoldUp.Cast(busEvent);
		if (!holdUp)
			return false;

		LF_HandsUp.OfferByUids(holdUp.m_sActorUid, holdUp.m_sTargetUid);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HandsUpAimSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnAimedAt aimed = LF_OnAimedAt.Cast(busEvent);
		if (!aimed)
			return false;

		LF_HandsUp.OfferByUids(aimed.m_sActorUid, aimed.m_sTargetUid);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The replicated state: a bool on the character, written on the server only.
modded class SCR_ChimeraCharacter
{
	[RplProp(onRplName: "LF_OnHandsUpReplicated")]
	protected bool m_bLF_HandsUp;

	//------------------------------------------------------------------------------------------------
	void LF_SetHandsUp(bool up)
	{
		if (!Replication.IsServer() || up == m_bLF_HandsUp)
			return;

		m_bLF_HandsUp = up;
		Replication.BumpMe();
		// A listen host gets no RplProp callback for its own write.
		LF_ApplyHandsUpPose();
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsHandsUp()
	{
		return m_bLF_HandsUp;
	}

	//------------------------------------------------------------------------------------------------
	//! RplProp callback: runs on clients when the server changes the state.
	protected void LF_OnHandsUpReplicated()
	{
		LF_ApplyHandsUpPose();
	}

	//------------------------------------------------------------------------------------------------
	//! The owner's machine animates its own body (the base game replicates the gesture to the rest).
	protected void LF_ApplyHandsUpPose()
	{
		if (SCR_PlayerController.GetLocalControlledEntity() != this)
			return;

		CharacterControllerComponent controller = GetCharacterController();
		if (!controller)
			return;

		if (m_bLF_HandsUp)
		{
			if (!controller.TryStartCharacterGesture(LF_HandsUp.GESTURE_ID, 0))
				LF_Debug.Info(LF_HandsUp.FEATURE, "hands-up gesture did not start (the character cannot gesture right now)");

			return;
		}

		if (controller.IsPlayingGesture())
			controller.StopCharacterGesture();
	}
}

//------------------------------------------------------------------------------------------------
//! The harness verbs of the proof: `handsup state` (what this client draws), `handsup choose <0|1|2>`
//! (this player's own answer), `handsup offer <playerId>` (a dev-gated server offer, standing in for
//! a real held aim).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "handsup")
			return super.Handle(cmd);

		SCR_PlayerController mine = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (cmd.Arg1 == "choose" && mine)
		{
			mine.LF_HandsUpAsk(cmd.Arg2.ToInt());
			LF_AgentHarness.Answer(cmd.Id, "ok", "asked " + cmd.Arg2);
			return true;
		}

		if (cmd.Arg1 == "offer" && mine)
		{
			mine.LF_HandsUpAskOffer(cmd.Arg2.ToInt());
			LF_AgentHarness.Answer(cmd.Id, "ok", "asked offer for player " + cmd.Arg2);
			return true;
		}

		string line = "prompt=" + LF_HandsUpPrompt.Describe();
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(id));
			if (!character)
				continue;

			line = line + string.Format(" p%1=%2", id, character.LF_IsHandsUp());
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", line);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HandsUp's statics (LF_StaticReset finds it by reflection).
class LF_HandsUp_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HandsUp";
	}

	override void Apply()
	{
		LF_HandsUp.ResetStatics();
	}
}
