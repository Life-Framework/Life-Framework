//------------------------------------------------------------------------------------------------
//! Cuffing as something done with the hands (docs/asset-pipeline/states.md, "Use states"). The Cuff and
//! Uncuff actions are short holds (`Duration` on the action in Character_Roleplay.et: the vanilla
//! progress bar). The server runs the physical sequence around them:
//!
//!   Cuff:   hold begins -> the cuffs in the officer's hand swing open -> the hold ends -> they swing
//!           shut with a click -> they leave the hand and are on the suspect's wrists
//!           (LF_Restraint.Restrain).
//!   Uncuff: hold begins -> the cuffs on the wrist unlock and open -> the hold ends -> they come off
//!           and go back to the officer (LF_Restraint.Release).
//!
//! The server owns every step. The acting client only reports that its hold began or was let go
//! (LF_AskCuffing, like the revive hold); the server checks reach and rules at the start, again every
//! WATCH_MS while the hold runs, and again at the end, and a hold that fails any check closes the
//! cuffs and stops: either party moved out of reach, the officer's hand no longer holds the cuffs,
//! someone went down or died. Completion is accepted only for a hold the server saw begin and only
//! after most of the action's duration, so a modified client cannot cuff in an instant.
//! The open state of the cuffs in hand is LF_AssetAnimComponent's replicated flag; the open state of
//! the cuffs on a wrist is the character's replicated m_bLF_CuffsOpen, so a late joiner sees both.
//------------------------------------------------------------------------------------------------
enum LF_ECuffingKind
{
	CUFF,
	RELEASE
}

//------------------------------------------------------------------------------------------------
//! What the acting client reports. FINISH and PIN are the agent harness's (behind the dev gate): a
//! session proof cannot hold a key down, and a picture takes longer than the hold.
enum LF_ECuffingPhase
{
	START,
	CANCEL,
	FINISH,
	PIN
}

//------------------------------------------------------------------------------------------------
enum LF_ECuffingStep
{
	KEEP,
	STOP_REACH,
	STOP_REFUSED,
	STOP_TIMEOUT
}

//------------------------------------------------------------------------------------------------
class LF_CuffingSession : Managed
{
	EntityID m_Officer;
	EntityID m_Target;
	//! The cuffs in the officer's hand (a Cuff hold); the entity that is opened and must be closed again.
	EntityID m_Cuffs;
	LF_ECuffingKind m_eKind;
	float m_fStartedAt;
	float m_fMaxSeconds;
	//! The hold ended; the cuffs are swinging shut and nothing may cancel the transfer now.
	bool m_bClosing;
}

//------------------------------------------------------------------------------------------------
class LF_Cuffing
{
	static const string FEATURE = "Police";
	//! Share of the action's duration the server insists on before it accepts the end of a hold: the
	//! network may delay the start report, never the client's own bar.
	static const float HOLD_SLACK = 0.8; // lf-fixed: network allowance, not a gameplay knob
	//! A hold the server never hears the end of (a lost cancel) is dropped after this many seconds.
	static const float MAX_SECONDS = 8; // lf-fixed: watchdog
	//! A hold pinned by the harness for a picture.
	static const float PINNED_SECONDS = 600; // lf-fixed: harness only
	static const int WATCH_MS = 250; // lf-fixed: how often the server rechecks the pair
	//! How long the strands take to shut (spec.anim.bones seconds 0.5) plus a moment; the cuffs leave
	//! the hand after it.
	static const int CLOSE_MS = 650; // lf-fixed: geometry of the handcuffs animation

	protected static ref array<ref LF_CuffingSession> s_aSessions = {};
	protected static bool s_bWatching;
	protected static int s_iBegun;
	protected static int s_iStopped;
	protected static int s_iFinished;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aSessions.Clear();
		s_bWatching = false;
		s_iBegun = 0;
		s_iStopped = 0;
		s_iFinished = 0;
	}

	//------------------------------------------------------------------------------------------------
	static int GetActiveCount()
	{
		return s_aSessions.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int GetBegunCount()
	{
		return s_iBegun;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStoppedCount()
	{
		return s_iStopped;
	}

	//------------------------------------------------------------------------------------------------
	static int GetFinishedCount()
	{
		return s_iFinished;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does a running hold go on? `reach` is LF_PlayerInteraction.CheckPair, `rule` the cuffing rule
	//! (LF_Restraint.CheckRestrain / CheckRelease), both read now. Reach is judged first: walking away
	//! is the commonest reason and the one the player is told.
	static LF_ECuffingStep Step(LF_EInteractionRefusal reach, LF_ERestrain rule, float elapsedSeconds, float maxSeconds)
	{
		if (reach != LF_EInteractionRefusal.NONE)
			return LF_ECuffingStep.STOP_REACH;

		if (rule != LF_ERestrain.OK)
			return LF_ECuffingStep.STOP_REFUSED;

		if (elapsedSeconds > maxSeconds)
			return LF_ECuffingStep.STOP_TIMEOUT;

		return LF_ECuffingStep.KEEP;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: has the hold run long enough for the server to accept its end?
	static bool HoldLongEnough(float elapsedSeconds, float durationSeconds)
	{
		return elapsedSeconds >= durationSeconds * HOLD_SLACK;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		float milliseconds = GetGame().GetWorld().GetWorldTime();
		return milliseconds / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Toast to the officer, when a player controls them.
	protected static void Tell(IEntity officer, string message)
	{
		int playerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Restrain_Title", message, 3.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ERestrain Rule(IEntity officer, IEntity target, LF_ECuffingKind kind)
	{
		if (kind == LF_ECuffingKind.RELEASE)
			return LF_Restraint.CheckRelease(officer, target);

		return LF_Restraint.CheckRestrain(officer, target);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CuffingSession Find(IEntity officer)
	{
		if (!officer)
			return null;

		foreach (LF_CuffingSession session : s_aSessions)
		{
			if (session.m_Officer == officer.GetID())
				return session;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Acting client, from OnActionStart / OnActionCanceled: tell the server the hold began or ended.
	static void AskFromAction(IEntity user, IEntity target, LF_ECuffingKind kind, LF_ECuffingPhase phase)
	{
		if (!user || !target || user != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskCuffing(LF_NetworkUtils.GetRplId(target), kind, phase);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer's hold began. Checks reach and the rule, opens the cuffs (in the hand, or on
	//! the wrist) and starts watching the pair.
	//! \return true when a hold is running for this officer on this target.
	static bool Begin(IEntity officer, IEntity target, LF_ECuffingKind kind, bool pinned = false)
	{
		if (LF_Systems.Refuses("Police", "cuffing"))
			return false;

		if (!Replication.IsServer())
			return false;

		if (!officer || !target)
		{
			LF_Debug.Error(FEATURE, "cuffing hold refused: officer or target is null");
			return false;
		}

		LF_CuffingSession existing = Find(officer);
		if (existing)
		{
			if (existing.m_Target == target.GetID() && existing.m_eKind == kind)
				return true;

			End(officer, "a new hold began");
		}

		if (!LF_RateLimit.AllowCharacter(officer, LF_ERequestKind.SPAWN, "cuffing hold"))
			return false;

		LF_EInteractionRefusal reach = LF_PlayerInteraction.CheckPair(officer, target);
		if (reach != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, "cuffing hold refused: " + typename.EnumToString(LF_EInteractionRefusal, reach));
			return false;
		}

		LF_ERestrain rule = Rule(officer, target, kind);
		if (rule != LF_ERestrain.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("cuffing hold refused: %1 -> %2 (%3)", LF_Utils.GetPlayerUID(officer), LF_Utils.GetPlayerUID(target), typename.EnumToString(LF_ERestrain, rule)));
			Tell(officer, LF_Restraint.RefusalKey(rule));
			return false;
		}

		IEntity cuffs;
		if (kind == LF_ECuffingKind.CUFF)
			cuffs = LF_Restraint.HeldCuffs(officer);

		return StartHold(officer, target, kind, cuffs, pinned);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open what the hold opens (the cuffs in `cuffs`, or the cuffs on the suspect's wrists for
	//! an Uncuff) and start watching the pair. Begin has judged reach and the rule; tests call this on
	//! its own.
	static bool StartHold(IEntity officer, IEntity target, LF_ECuffingKind kind, IEntity cuffs, bool pinned = false)
	{
		LF_CuffingSession session = new LF_CuffingSession();
		session.m_Officer = officer.GetID();
		session.m_Target = target.GetID();
		session.m_eKind = kind;
		session.m_fStartedAt = Now();
		session.m_fMaxSeconds = MAX_SECONDS;
		if (pinned)
			session.m_fMaxSeconds = PINNED_SECONDS;

		if (kind == LF_ECuffingKind.CUFF)
		{
			LF_HandcuffsComponent handcuffs;
			if (cuffs)
				handcuffs = LF_HandcuffsComponent.Cast(cuffs.FindComponent(LF_HandcuffsComponent));

			if (!handcuffs || !handcuffs.SetOpen(true))
			{
				LF_Debug.Error(FEATURE, "cuffing hold refused: the cuffs in hand cannot be opened");
				return false;
			}

			session.m_Cuffs = cuffs.GetID();
		}
		else
		{
			SCR_ChimeraCharacter prisoner = SCR_ChimeraCharacter.Cast(target);
			if (!prisoner)
				return false;

			prisoner.LF_SetCuffsOpen(true);
		}

		s_aSessions.Insert(session);
		s_iBegun++;
		LF_Debug.Info(FEATURE, string.Format("cuffing hold began: %1 %2 %3", LF_Utils.GetPlayerUID(officer), typename.EnumToString(LF_ECuffingKind, kind), LF_Utils.GetPlayerUID(target)));
		if (!s_bWatching)
		{
			s_bWatching = true;
			GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player let go of the hold. A hold already closing the cuffs goes through.
	static void Cancel(IEntity officer)
	{
		LF_CuffingSession session = Find(officer);
		if (!session || session.m_bClosing)
			return;

		End(officer, "the hold was let go");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stop the officer's hold and close what it opened.
	static void End(IEntity officer, string reason)
	{
		LF_CuffingSession session = Find(officer);
		if (!session)
			return;

		s_aSessions.RemoveItem(session);
		s_iStopped++;
		Close(session);
		LF_Debug.Info(FEATURE, "cuffing hold stopped: " + reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Shut whatever the hold opened.
	protected static void Close(LF_CuffingSession session)
	{
		if (session.m_eKind == LF_ECuffingKind.CUFF)
		{
			IEntity cuffs = GetGame().GetWorld().FindEntityByID(session.m_Cuffs);
			if (!cuffs)
				return;

			LF_HandcuffsComponent handcuffs = LF_HandcuffsComponent.Cast(cuffs.FindComponent(LF_HandcuffsComponent));
			if (handcuffs)
				handcuffs.SetOpen(false);

			return;
		}

		// Another officer may be holding an Uncuff on the same suspect: the wrists stay open for them.
		foreach (LF_CuffingSession other : s_aSessions)
		{
			if (other != session && other.m_eKind == LF_ECuffingKind.RELEASE && other.m_Target == session.m_Target)
				return;
		}

		SCR_ChimeraCharacter prisoner = SCR_ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(session.m_Target));
		if (prisoner)
			prisoner.LF_SetCuffsOpen(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the action's PerformAction: the hold ran its full time. A Cuff shuts the cuffs and, a
	//! moment later, puts them on the suspect; an Uncuff takes them off now.
	//! \param durationSeconds The action's own duration; the hold must have run most of it unless forced.
	//! \param force The harness: skip the time check.
	static LF_ERestrain Complete(IEntity officer, IEntity target, LF_ECuffingKind kind, float durationSeconds, bool force = false)
	{
		if (!Replication.IsServer() || !officer || !target)
			return LF_ERestrain.NO_TARGET;

		LF_CuffingSession session = Find(officer);
		if (!session || session.m_bClosing || session.m_Target != target.GetID() || session.m_eKind != kind)
		{
			LF_Debug.Warn(FEATURE, "cuffing completion refused: no matching hold was running (its start never reached the server)");
			return LF_ERestrain.NO_TARGET;
		}

		float elapsed = Now() - session.m_fStartedAt;
		if (!force && !HoldLongEnough(elapsed, durationSeconds))
		{
			LF_Debug.Warn(FEATURE, string.Format("cuffing completion refused: the hold ran %1 s of %2 s", elapsed, durationSeconds));
			End(officer, "ended too early");
			return LF_ERestrain.NO_TARGET;
		}

		if (kind == LF_ECuffingKind.RELEASE)
		{
			s_aSessions.RemoveItem(session);
			s_iFinished++;
			LF_ERestrain released = LF_Restraint.Release(officer, target);
			if (released != LF_ERestrain.OK)
			{
				SCR_ChimeraCharacter prisoner = SCR_ChimeraCharacter.Cast(target);
				if (prisoner)
					prisoner.LF_SetCuffsOpen(false);
			}

			return released;
		}

		// Cuff: the strands swing shut now (with the click); the cuffs go to the wrists when they have.
		session.m_bClosing = true;
		IEntity cuffs = GetGame().GetWorld().FindEntityByID(session.m_Cuffs);
		if (cuffs)
		{
			LF_HandcuffsComponent handcuffs = LF_HandcuffsComponent.Cast(cuffs.FindComponent(LF_HandcuffsComponent));
			if (handcuffs)
				handcuffs.SetOpen(false);
		}

		GetGame().GetCallqueue().CallLater(FinishCuff, CLOSE_MS, false, officer, target);
		LF_Debug.Info(FEATURE, "cuffing hold done: the cuffs are closing");
		return LF_ERestrain.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the cuffs have shut; put them on the suspect if the pair is still in reach.
	protected static void FinishCuff(IEntity officer, IEntity target)
	{
		if (officer)
		{
			LF_CuffingSession session = Find(officer);
			if (session)
				s_aSessions.RemoveItem(session);
		}

		if (!officer || !target)
		{
			LF_Debug.Warn(FEATURE, "cuffing hold lost: officer or suspect is gone");
			return;
		}

		LF_EInteractionRefusal reach = LF_PlayerInteraction.CheckPair(officer, target);
		if (reach != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, "cuffing hold lost: " + typename.EnumToString(LF_EInteractionRefusal, reach) + " as the cuffs shut; they stay in hand");
			Tell(officer, "#LF-Restrain_Interrupted");
			return;
		}

		s_iFinished++;
		LF_Restraint.Restrain(officer, target);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one watch pass over the running holds (the timer calls it; tests call it directly).
	static void Watch()
	{
		for (int i = s_aSessions.Count() - 1; i >= 0; i--)
		{
			LF_CuffingSession session = s_aSessions[i];
			if (session.m_bClosing)
				continue;

			IEntity officer = GetGame().GetWorld().FindEntityByID(session.m_Officer);
			IEntity target = GetGame().GetWorld().FindEntityByID(session.m_Target);
			if (!officer || !target)
			{
				s_aSessions.Remove(i);
				s_iStopped++;
				Close(session);
				LF_Debug.Info(FEATURE, "cuffing hold stopped: officer or suspect is gone");
				continue;
			}

			LF_ECuffingStep step = Step(LF_PlayerInteraction.CheckPair(officer, target), Rule(officer, target, session.m_eKind), Now() - session.m_fStartedAt, session.m_fMaxSeconds);
			if (step == LF_ECuffingStep.KEEP)
				continue;

			End(officer, typename.EnumToString(LF_ECuffingStep, step));
			if (step != LF_ECuffingStep.STOP_TIMEOUT)
				Tell(officer, "#LF-Restrain_Interrupted");
		}

		if (s_aSessions.IsEmpty() && s_bWatching)
		{
			s_bWatching = false;
			GetGame().GetCallqueue().Remove(Watch);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Client -> server: the Cuff or Uncuff hold began or ended. The server takes the officer from this
//! controller's character and the suspect from an RplId, and checks everything itself.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskCuffing(RplId target, LF_ECuffingKind kind, LF_ECuffingPhase phase)
	{
		if (Replication.IsServer())
			RpcAsk_LF_Cuffing(target, kind, phase);
		else
			Rpc(RpcAsk_LF_Cuffing, target, kind, phase);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Cuffing(RplId target, int kind, int phase)
	{
		IEntity officer = GetControlledEntity();
		// The payload comes from a client: an unknown kind or phase is dropped, not guessed at.
		if (!officer || kind < LF_ECuffingKind.CUFF || kind > LF_ECuffingKind.RELEASE || phase < LF_ECuffingPhase.START || phase > LF_ECuffingPhase.PIN)
		{
			LF_Debug.Warn(LF_Cuffing.FEATURE, string.Format("cuffing request dropped: kind %1, phase %2, officer %3", kind, phase, officer != null));
			return;
		}

		IEntity suspect = LF_NetworkUtils.FindEntityByRplId(target);
		LF_ECuffingKind cuffingKind = kind;
		if (phase != LF_ECuffingPhase.CANCEL && !suspect)
		{
			LF_Debug.Warn(LF_Cuffing.FEATURE, "cuffing request dropped: the suspect's RplId does not resolve");
			return;
		}

		if (phase == LF_ECuffingPhase.CANCEL)
		{
			LF_Cuffing.Cancel(officer);
			return;
		}

		if (phase == LF_ECuffingPhase.START)
		{
			LF_Cuffing.Begin(officer, suspect, cuffingKind);
			return;
		}

		// FINISH and PIN skip what a held key does: harness proofs only.
		if (!LF_DevGate.CheckHarnessRequest(officer, "cuffing"))
		{
			LF_Debug.Warn(LF_Cuffing.FEATURE, "cuffing harness phase refused by the dev gate");
			return;
		}

		if (phase == LF_ECuffingPhase.PIN)
			LF_Cuffing.Begin(officer, suspect, cuffingKind, true);
		else if (phase == LF_ECuffingPhase.FINISH)
			LF_Cuffing.Complete(officer, suspect, cuffingKind, 0, true);
	}
}
