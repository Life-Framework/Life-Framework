//------------------------------------------------------------------------------------------------
//! A held robbery (#936). The robber starts it in the attendant's dialogue with a weapon drawn
//! (LF_RobDialogue.c) and must then keep the attendant at gunpoint, within the till's radius,
//! until the hold runs out; the till then pays as the instant robbery does. The server judges
//! every quarter second. The robber's screen shows the time left, the distance to the till and
//! why it ended (LF_RobHoldHud). The attendant begs during the hold and cries out at the end,
//! in its own voice, to everyone in earshot (LF_Dialogue.Announce).
//------------------------------------------------------------------------------------------------
enum LF_ERobHoldEnd
{
	NONE,	//!< Still holding
	PAID,	//!< The hold was kept and the till paid
	LEFT,	//!< The robber left the radius
	WEAPON,	//!< The robber put the weapon away
	POLICE,	//!< An on-duty officer came within the police radius
	DOWNED,	//!< The robber went down or died
	GONE,	//!< The robber left the server, or the till or attendant is gone
	TILL	//!< The hold was kept but the till could not pay (no room for the cash)
}

//------------------------------------------------------------------------------------------------
//! The rules of a hold as pure functions: no world, no clock. LOGIC-tested (LF_Test_RobHold).
class LF_RobHoldRules
{
	//! Past this share of the radius the robber is warned to come back.
	static const float EDGE_SHARE = 0.75; // lf-fixed: when the warning shows, not a balance value
	//! The attendant begs at these shares of the hold (after its "I am opening it" line).
	static const float BEG_SHARE = 0.4; // lf-fixed: pacing of voiced lines, follows their length
	static const float STAMMER_SHARE = 0.73; // lf-fixed: pacing of voiced lines, follows their length

	//------------------------------------------------------------------------------------------------
	//! What the hold is at now. A robber who is gone, down, unarmed or outside the radius loses it
	//! before an officer's arrival is counted, and the arrival before the clock runs out.
	static LF_ERobHoldEnd Judge(bool present, bool downed, bool armed, float distance, float radius, bool policeNear, int elapsedMs, int holdMs)
	{
		if (!present)
			return LF_ERobHoldEnd.GONE;

		if (downed)
			return LF_ERobHoldEnd.DOWNED;

		if (!armed)
			return LF_ERobHoldEnd.WEAPON;

		if (distance > radius)
			return LF_ERobHoldEnd.LEFT;

		if (policeNear)
			return LF_ERobHoldEnd.POLICE;

		if (elapsedMs >= holdMs)
			return LF_ERobHoldEnd.PAID;

		return LF_ERobHoldEnd.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Time left as the timer shows it, "0:20": seconds rounded up, so it reads 0:00 only at the end.
	static string Clock(int msLeft)
	{
		if (msLeft < 0)
			msLeft = 0;

		int seconds = (msLeft + 999) / 1000;
		int minutes = seconds / 60;
		seconds = seconds % 60;
		string pad = "";
		if (seconds < 10)
			pad = "0";

		return string.Format("%1:%2%3", minutes, pad, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! How full the bar is, 0 to 1.
	static float Progress(int elapsedMs, int holdMs)
	{
		if (holdMs <= 0)
			return 1;

		float elapsed = elapsedMs;
		return Math.Clamp(elapsed / holdMs, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	static bool NearEdge(float distance, float radius)
	{
		return distance >= radius * EDGE_SHARE;
	}

	//------------------------------------------------------------------------------------------------
	//! 0 before the first begging line is due, 1 after it, 2 after the second.
	static int BegStage(int elapsedMs, int holdMs)
	{
		float share = Progress(elapsedMs, holdMs);
		if (share >= STAMMER_SHARE)
			return 2;

		if (share >= BEG_SHARE)
			return 1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What the robber's screen says when the hold ends.
	static string EndKey(LF_ERobHoldEnd end)
	{
		switch (end)
		{
			case LF_ERobHoldEnd.PAID: return "#LF-RobHold_Success";
			case LF_ERobHoldEnd.LEFT: return "#LF-RobHold_Fail_Left";
			case LF_ERobHoldEnd.WEAPON: return "#LF-RobHold_Fail_Weapon";
			case LF_ERobHoldEnd.POLICE: return "#LF-RobHold_Fail_Police";
			case LF_ERobHoldEnd.DOWNED: return "#LF-RobHold_Fail_Downed";
			case LF_ERobHoldEnd.TILL: return "#LF-RobHold_Fail_Till";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! What the attendant cries out when the hold ends (a key it has a voiced line for), or empty.
	static string ClerkKey(LF_ERobHoldEnd end)
	{
		switch (end)
		{
			case LF_ERobHoldEnd.PAID: return "#LF-RobHold_ClerkPaid";
			case LF_ERobHoldEnd.POLICE: return "#LF-RobHold_ClerkPolice";
			case LF_ERobHoldEnd.LEFT: return "#LF-RobHold_ClerkGone";
			case LF_ERobHoldEnd.WEAPON: return "#LF-RobHold_ClerkGone";
			case LF_ERobHoldEnd.DOWNED: return "#LF-RobHold_ClerkGone";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The caption the attendant's conversation shows for a refusal (LF_RobAction.REFUSED_* or
	//! LF_RobHold.REFUSED_*).
	static string RefusalKey(string refusal)
	{
		if (refusal == LF_RobAction.REFUSED_COOLDOWN)
			return "#LF-RobHold_Refused_Cooldown";

		if (refusal == LF_RobAction.REFUSED_POLICE)
			return "#LF-RobHold_Refused_Police";

		if (refusal == LF_RobAction.REFUSED_NOT_CIVILIAN || refusal == LF_RobAction.REFUSED_NO_ACCOUNT)
			return "#LF-RobHold_Refused_Civilian";

		if (refusal == LF_RobHold.REFUSED_BUSY)
			return "#LF-RobHold_Refused_Busy";

		if (refusal == LF_RobHold.REFUSED_RATE)
			return "#LF-RobHold_Refused_Rate";

		if (refusal == LF_RobHold.REFUSED_WEAPON)
			return "#LF-RobHold_Refused_Weapon";

		if (refusal == LF_RobAction.REFUSED_OFF)
			return "#LF-Systems_Off";

		return "#LF-RobHold_Refused_NoTill";
	}
}

//------------------------------------------------------------------------------------------------
class LF_RobHoldSession : Managed
{
	int m_iPlayerId;
	//! Weak: the till's action goes when its entity does, and the hold then ends as GONE.
	LF_RobAction m_Till;
	EntityID m_NpcId = EntityID.INVALID;
	vector m_vTill;
	int m_iStartMs;
	int m_iHoldMs;
	float m_fRadius;
	float m_fPoliceRadius;
	int m_iBegStage;
}

//------------------------------------------------------------------------------------------------
//! The server's held robberies.
class LF_RobHold
{
	static const string FEATURE = "Crime";
	//! How far from its attendant a till may stand to be that attendant's till.
	static const float TILL_REACH = 8.0; // lf-fixed: a counter's depth, not a balance value
	static const int TICK_MS = 250; // lf-fixed: judging cadence
	static const string REFUSED_BUSY = "robbery refused: this till is already being robbed";
	static const string REFUSED_NO_TILL = "robbery refused: no dialogue till by this attendant";
	static const string REFUSED_WEAPON = "robbery refused: no weapon drawn";
	static const string REFUSED_RATE = "robbery refused: too soon after the last attempt";
	static const string REFUSED_REACH = "robbery refused: not within the till's radius";

	protected static ref array<ref LF_RobHoldSession> s_aActive;
	protected static bool s_bTicking;
	protected static LF_ERobHoldEnd s_eLastEnd = LF_ERobHoldEnd.NONE;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aActive = null;
		s_eLastEnd = LF_ERobHoldEnd.NONE;
		if (s_bTicking && GetGame())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_RobHoldSession> Active()
	{
		if (!s_aActive)
			s_aActive = {};

		return s_aActive;
	}

	//------------------------------------------------------------------------------------------------
	static int ActiveCount()
	{
		return Active().Count();
	}

	//------------------------------------------------------------------------------------------------
	static LF_ERobHoldEnd LastEnd()
	{
		return s_eLastEnd;
	}

	//------------------------------------------------------------------------------------------------
	//! The till an attendant keeps: the nearest dialogue-only till behind its counter, or null.
	static LF_RobAction TillOf(IEntity npc)
	{
		if (!npc)
			return null;

		return LF_RobAction.FindDialogueTill(npc.GetOrigin(), TILL_REACH);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_RobHoldSession FindByTill(LF_RobAction till)
	{
		foreach (LF_RobHoldSession session : Active())
		{
			if (session.m_Till == till)
				return session;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why `player` cannot rob the till of `npc` now, or empty. Reads only, logs nothing.
	static string Refusal(IEntity player, IEntity npc)
	{
		if (!LF_Systems.IsEnabled(LF_RobAction.SYSTEM))
			return LF_RobAction.REFUSED_OFF;

		if (!LF_HoldUp.IsArmed(player))
			return REFUSED_WEAPON;

		LF_RobAction till = TillOf(npc);
		if (!till)
			return REFUSED_NO_TILL;

		if (FindByTill(till))
			return REFUSED_BUSY;

		if (vector.Distance(player.GetOrigin(), till.GetOwner().GetOrigin()) > till.GetHoldRadius())
			return REFUSED_REACH;

		return till.HeldRefusal(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the robber demanded the till. Checks everything again, starts the cooldown, reports
	//! the crime and starts the hold on the robber's screen. \return The refusal, or empty.
	static string Start(IEntity player, IEntity npc)
	{
		if (!Replication.IsServer() || !player || !npc)
			return REFUSED_NO_TILL;

		PlayerManager players = GetGame().GetPlayerManager();
		int playerId = players.GetPlayerIdFromControlledEntity(player);
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.CRIME, "robbery start", false))
			return REFUSED_RATE;

		string refusal = Refusal(player, npc);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("held robbery not started for player %1: %2", playerId, refusal));
			return refusal;
		}

		LF_RobAction till = TillOf(npc);
		refusal = till.BeginHeld(player);
		if (!refusal.IsEmpty())
			return refusal;

		LF_RobHoldSession session = new LF_RobHoldSession();
		session.m_iPlayerId = playerId;
		session.m_Till = till;
		session.m_NpcId = npc.GetID();
		session.m_vTill = till.GetOwner().GetOrigin();
		session.m_iStartMs = System.GetTickCount();
		session.m_iHoldMs = (int)(till.GetHoldSeconds() * 1000);
		session.m_fRadius = till.GetHoldRadius();
		session.m_fPoliceRadius = till.GetPoliceRadius();
		Active().Insert(session);

		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		if (controller)
			controller.LF_RobHoldBegin(session.m_vTill, session.m_fRadius, session.m_iHoldMs);

		LF_Debug.Info(FEATURE, string.Format("held robbery begins: player %1 keeps the till at %2 for %3 s within %4 m (police within %5 m end it)", playerId, session.m_vTill, till.GetHoldSeconds(), session.m_fRadius, session.m_fPoliceRadius));
		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		array<ref LF_RobHoldSession> active = Active();
		for (int i = active.Count() - 1; i >= 0; i--)
		{
			LF_RobHoldSession session = active[i];
			LF_ERobHoldEnd end = Step(session);
			if (end == LF_ERobHoldEnd.NONE)
				continue;

			Finish(session, end);
			active.RemoveOrdered(i);
		}

		if (active.IsEmpty() && s_bTicking)
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bTicking = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ERobHoldEnd Step(LF_RobHoldSession session)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(session.m_iPlayerId);
		IEntity npc = GetGame().GetWorld().FindEntityByID(session.m_NpcId);
		bool present = player && npc && session.m_Till && session.m_Till.GetOwner();
		bool downed;
		bool armed;
		float distance;
		bool policeNear;
		if (present)
		{
			downed = LF_Downed.IsDowned(player) || IsDead(player);
			armed = LF_HoldUp.IsArmed(player);
			distance = vector.Distance(player.GetOrigin(), session.m_vTill);
			policeNear = PoliceWithin(session.m_vTill, session.m_fPoliceRadius);
		}

		int elapsed = System.GetTickCount() - session.m_iStartMs;
		LF_ERobHoldEnd end = LF_RobHoldRules.Judge(present, downed, armed, distance, session.m_fRadius, policeNear, elapsed, session.m_iHoldMs);
		if (end != LF_ERobHoldEnd.NONE)
			return end;

		int stage = LF_RobHoldRules.BegStage(elapsed, session.m_iHoldMs);
		if (stage > session.m_iBegStage)
		{
			session.m_iBegStage = stage;
			if (stage == 1)
				LF_Dialogue.Announce(npc, "#LF-RobHold_ClerkBeg");
			else
				LF_Dialogue.Announce(npc, "#LF-RobHold_ClerkStammer");
		}

		return LF_ERobHoldEnd.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(LF_RobHoldSession session, LF_ERobHoldEnd end)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		IEntity player = players.GetPlayerControlledEntity(session.m_iPlayerId);
		IEntity npc = GetGame().GetWorld().FindEntityByID(session.m_NpcId);
		int paid = 0;
		if (end == LF_ERobHoldEnd.PAID)
		{
			paid = session.m_Till.PayHeld(player);
			if (paid > 0)
				LF_Utils.NotifyEntity(player, "#EL-Stole_Money", "#EL-Robbery_Successful", 3.0, paid.ToString());
			else
				end = LF_ERobHoldEnd.TILL;
		}

		string cry = LF_RobHoldRules.ClerkKey(end);
		if (npc && !cry.IsEmpty())
			LF_Dialogue.Announce(npc, cry);

		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(session.m_iPlayerId));
		if (controller)
			controller.LF_RobHoldEnd(end, paid);

		s_eLastEnd = end;
		LF_Debug.Info(FEATURE, string.Format("held robbery ends for player %1: %2, paid %3", session.m_iPlayerId, typename.EnumToString(LF_ERobHoldEnd, end), paid));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDead(IEntity player)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	//! An on-duty officer stands within `radius` of `at`.
	static bool PoliceWithin(vector at, float radius)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!players || !accounts)
			return false;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity body = players.GetPlayerControlledEntity(id);
			if (!body || vector.Distance(body.GetOrigin(), at) > radius)
				continue;

			LF_PlayerAccount account = accounts.GetAccount(LF_Utils.GetAccountUID(id));
			if (LF_AccountStateBits.IsOnDutyPolice(LF_AccountStateBits.FromAccount(account)))
				return true;
		}

		return false;
	}
}
