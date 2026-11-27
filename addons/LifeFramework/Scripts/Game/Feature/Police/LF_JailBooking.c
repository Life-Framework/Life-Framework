//------------------------------------------------------------------------------------------------
//! Booking a suspect into the jail (police arrest flow review, 2026-11-22). An arrest is no longer a
//! "Send to jail" from anywhere: the officer cuffs the suspect, a waypoint points them to the jail,
//! and at the cell (within BOOKING_METERS of the placed LF_JailCellComponent) the suspect offers two
//! scroll actions, "Book into jail: full sentence" and "Book into jail: reduced (cooperated)". The
//! cell's origin is noted on every machine (the cell is a world entity, so clients have it too), which
//! lets the client offer the actions only at the station; the server re-checks everything.
class LF_JailBooking
{
	static const string FEATURE = "Jail";
	//! Metres from the cell origin an officer may book from: the cell block's corridor and door.
	static const float BOOKING_METERS = 15;	// lf-fixed: one police station's cell block
	//! The waypoint id on the officer's client; one booking mark at a time.
	static const string WAYPOINT_ID = "jail-booking";
	static const int FULL_PERCENT = 100;
	//! The reduced sentence never drops below this many seconds.
	static const int MIN_REDUCED_SECONDS = 30;	// lf-fixed: a booking always costs some time

	protected static bool s_bCellKnown;
	protected static vector s_vCell;
	//! Officers told the first-arrest note this session (server).
	protected static ref set<string> s_Coached;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bCellKnown = false;
		s_vCell = vector.Zero;
		s_Coached = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: the placed cell's origin (LF_JailCellComponent.OnPostInit).
	static void NoteCell(vector origin)
	{
		s_bCellKnown = true;
		s_vCell = origin;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsCellKnown()
	{
		return s_bCellKnown;
	}

	//------------------------------------------------------------------------------------------------
	//! True when `position` is close enough to the cell to book a suspect.
	static bool IsAtBooking(vector position)
	{
		if (!s_bCellKnown)
			return false;

		return vector.Distance(position, s_vCell) <= BOOKING_METERS;
	}

	//------------------------------------------------------------------------------------------------
	//! The sentence for a booking option: `percent` of the full sentence, at least MIN_REDUCED_SECONDS
	//! for a reduced one (never more than the full one).
	static int Scaled(int fullSeconds, int percent)
	{
		if (percent >= FULL_PERCENT || fullSeconds <= 0)
			return fullSeconds;

		int seconds = fullSeconds * LF_Utils.MaxInt(percent, 0) / FULL_PERCENT;
		seconds = LF_Utils.MaxInt(seconds, MIN_REDUCED_SECONDS);
		return LF_Utils.MinInt(seconds, fullSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: may this officer book this suspect here? "" when yes, else the key the officer reads.
	static string Refusal(IEntity officer, IEntity suspect)
	{
		if (!officer || !suspect)
			return "#EL-Jail_NotWanted";

		if (!IsAtBooking(officer.GetOrigin()))
			return "#LF-JailBook_NotHere";

		string officerUid = LF_Utils.GetPlayerUid(officer);
		string suspectUid = LF_Utils.GetPlayerUid(suspect);
		bool held = LF_JailEscort.GetHolder(suspectUid) == officerUid && !officerUid.IsEmpty();
		if (!LF_Restraint.IsRestrained(suspect) && !held)
			return "#LF-JailBook_CuffFirst";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! "m:ss" for a toast or the HUD line.
	static string Clock(int seconds)
	{
		int s = LF_Utils.MaxInt(seconds, 0);
		int rest = s % 60;
		string pad = "";
		if (rest < 10)
			pad = "0";

		return string.Format("%1:%2%3", s / 60, pad, rest);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, after a cuff (LF_Restraint.Restrain): a wanted suspect puts the booking waypoint on the
	//! officer's client, and the officer's first cuff this session gets the how-to note.
	static void OnCuffed(IEntity officer, IEntity suspect)
	{
		if (!Replication.IsServer() || !officer || !suspect)
			return;

		string suspectUid = LF_Utils.GetPlayerUid(suspect);
		string officerUid = LF_Utils.GetPlayerUid(officer);
		LF_PlayerAccount suspectAccount = LF_PoliceUtils.GetAccountForEntity(suspect);
		if (!suspectAccount || (suspectAccount.GetWantedLevel() <= 0 && !LF_PrisonBreak.IsEscaped(suspectUid)))
		{
			LF_Debug.Info(FEATURE, string.Format("booking waypoint skipped: %1 is not wanted", suspectUid));
			return;
		}

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (!s_bCellKnown)
		{
			LF_Debug.Warn(FEATURE, "booking waypoint skipped: no jail cell is placed");
			return;
		}

		MarkFor(officerId, true);
		if (!s_Coached)
			s_Coached = new set<string>();

		string note = "#LF-JailBook_Waypoint";
		if (!s_Coached.Contains(officerUid))
		{
			s_Coached.Insert(officerUid);
			note = "#LF-JailBook_FirstArrest";
		}

		if (officerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-Police_Title", note, 8.0, LF_ENotificationType.POLICE_ALERT);

		LF_Debug.Info(FEATURE, string.Format("booking waypoint set for %1 (suspect %2)", officerUid, suspectUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put (or take) the booking waypoint on one player's client.
	static void MarkFor(int playerId, bool on)
	{
		if (playerId <= 0)
			return;

		LF_MapShareComponent channel = LF_MapShareComponent.OfPlayer(playerId);
		if (!channel)
		{
			LF_Debug.Warn(FEATURE, string.Format("booking waypoint: player %1 has no map channel", playerId));
			return;
		}

		if (on)
			channel.ServerJobWaypoint(WAYPOINT_ID, "#LF-JailBook_WaypointLabel", Math.Round(s_vCell[0]), Math.Round(s_vCell[2]));
		else
			channel.ServerJobWaypointClear(WAYPOINT_ID);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: book the suspect. \return true when the sentence started.
	static bool Book(IEntity officer, IEntity suspect, int percent)
	{
		if (!Replication.IsServer())
			return false;

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		string refusal = Refusal(officer, suspect);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("booking refused: %1 (%2)", LF_Utils.GetPlayerUid(suspect), refusal));
			if (officerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-Police_Title", refusal, 4.0, LF_ENotificationType.WARNING);
			return false;
		}

		string officerUid = LF_Utils.GetPlayerUid(officer);
		string suspectUid = LF_Utils.GetPlayerUid(suspect);
		if (!LF_Jail.Arrest(officerUid, suspectUid, suspect, percent))
		{
			if (officerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-Police_Title", "#LF-JailBook_Failed", 4.0, LF_ENotificationType.WARNING);
			return false;
		}

		MarkFor(officerId, false);
		// A booking at the desk earns the officer the $10 duty bonus, paid at clock-out (#4319).
		LF_DutyBonus.AwardUid(officerUid, LF_DutyBonusKind.BOOKING);
		LF_PoliceSkill.OnBooked(officerUid);
		LF_BookingSounds.OnBooked(officer);
		int seconds = Math.Round(LF_Jail.GetRemaining(suspectUid));
		LF_Debug.Info(FEATURE, string.Format("booked %1 by %2 percent=%3 seconds=%4", suspectUid, officerUid, percent, seconds));
		if (officerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-Police_Title", "#LF-JailBook_Booked", 5.0, LF_ENotificationType.SUCCESS);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The prisoner's remaining time on their own HUD: one small line under the top edge. The server writes
//! the seconds left onto the prisoner's character (replicated, throttled to SYNC_STEP_SECONDS) and the
//! owning client counts down between updates.
modded class SCR_ChimeraCharacter
{
	//! Seconds left in jail at the last server write; 0 when not serving.
	[RplProp(onRplName: "LF_OnJailClockReplicated")]
	protected int m_iLF_JailSecondsLeft;

	//------------------------------------------------------------------------------------------------
	//! Server only.
	void LF_SetJailSecondsLeft(int seconds)
	{
		if (!Replication.IsServer())
			return;

		int value = LF_Utils.MaxInt(seconds, 0);
		if (!LF_JailClock.ShouldSync(m_iLF_JailSecondsLeft, value))
			return;

		m_iLF_JailSecondsLeft = value;
		Replication.BumpMe();
		LF_OnJailClockReplicated();
	}

	//------------------------------------------------------------------------------------------------
	int LF_GetJailSecondsLeft()
	{
		return m_iLF_JailSecondsLeft;
	}

	//------------------------------------------------------------------------------------------------
	void LF_OnJailClockReplicated()
	{
		if (this != LF_Utils.GetLocalCharacter())
			return;

		LF_JailClock.OnLocal(m_iLF_JailSecondsLeft);
	}
}

//------------------------------------------------------------------------------------------------
class LF_JailClock
{
	static const ResourceName LAYOUT = "{7C3A1E5B9D2F4A61}UI/Layouts/HUD/LF_JailClock.layout";
	//! A write goes out when the shown time is this far off (the client counts down in between).
	static const int SYNC_STEP_SECONDS = 15;	// lf-fixed: one bump per prisoner per 15 s, not per tick

	protected static Widget s_wRoot;
	protected static TextWidget s_wLine;
	protected static int s_iBase;
	protected static int s_iBaseAtMs;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	//! Pure: does the server write `next` over `current`?
	static bool ShouldSync(int current, int next)
	{
		if (current == next)
			return false;

		if (next <= 0 || current <= 0)
			return true;

		// A credit (jail work) or a new sentence jumps; the plain countdown writes every step.
		return Math.AbsInt(current - next) >= SYNC_STEP_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write the prisoner's time onto their body.
	static void Sync(IEntity body, float remaining)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character)
			character.LF_SetJailSecondsLeft(Math.Ceil(remaining));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the local character's time changed.
	static void OnLocal(int seconds)
	{
		s_iBase = seconds;
		s_iBaseAtMs = System.GetTickCount();
		if (seconds <= 0)
		{
			Remove();
			return;
		}

		Show();
	}

	//------------------------------------------------------------------------------------------------
	//! What the line shows now, counted down from the last write.
	static int SecondsShown()
	{
		int elapsed = (System.GetTickCount() - s_iBaseAtMs) / 1000;
		return LF_Utils.MaxInt(s_iBase - elapsed, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The harness's `seen`: "shown@<m:ss>" or "none".
	static string Describe()
	{
		if (!s_wRoot)
			return "none";

		return "shown@" + LF_JailBooking.Clock(SecondsShown());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show()
	{
		if (!GetGame() || RplSession.Mode() == RplMode.Dedicated)
			return;

		if (!s_wRoot)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			if (!workspace)
				return;

			s_wRoot = workspace.CreateWidgets(LAYOUT);
			if (!s_wRoot)
			{
				LF_Debug.Error(LF_JailBooking.FEATURE, "jail clock layout did not load: " + LAYOUT);
				return;
			}

			s_wLine = TextWidget.Cast(s_wRoot.FindAnyWidget("Line"));
			LF_Debug.Info(LF_JailBooking.FEATURE, string.Format("jail clock shown seconds=%1", s_iBase));
		}

		if (!s_bTicking && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Tick, 1000, true);
			s_bTicking = true;
		}

		Tick();
	}

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		if (s_wRoot)
		{
			s_wRoot.RemoveFromHierarchy();
			LF_Debug.Info(LF_JailBooking.FEATURE, "jail clock removed");
		}

		s_wRoot = null;
		s_wLine = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SCR_ChimeraCharacter me = SCR_ChimeraCharacter.Cast(LF_Utils.GetLocalCharacter());
		if (!me || me.LF_GetJailSecondsLeft() <= 0 || !s_wRoot)
		{
			Remove();
			return;
		}

		if (s_wLine)
			s_wLine.SetText(WidgetManager.Translate("#LF-JailClock_Line", LF_JailBooking.Clock(SecondsShown())));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Remove();
		s_iBase = 0;
		s_iBaseAtMs = 0;
	}
}

//------------------------------------------------------------------------------------------------
class LF_JailBooking_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_JailBooking";
	}

	override void Apply()
	{
		LF_JailBooking.ResetStatics();
		LF_JailClock.ResetStatics();
	}
}
