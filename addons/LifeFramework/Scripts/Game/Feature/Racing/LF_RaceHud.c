// LF_RaceHud.c - the racer's panel (#1924): lap, next checkpoint, place and time. The server builds
// each racer's panel model after every race tick (LF_RaceClock) and sends it to that racer's own
// client only when the lap, checkpoint, place or field size changed, never per frame
// (RpcDo_LF_RaceHudState on SCR_PlayerController). The client runs the race clock itself from the
// elapsed time in the last update, so it can trail the server by the network delay. The client
// sends nothing; the server's runner (LF_RaceRunner) decides everything.

//------------------------------------------------------------------------------------------------
//! One racer as the place order sees it.
class LF_RaceHudRacer
{
	string m_sUid;
	//! Checkpoints passed over the whole race (laps done x checkpoints + passed in this lap).
	int m_iPasses;
	//! Race milliseconds when m_iPasses last went up; the earlier one ranks first on a tie.
	int m_iPassMs;
	bool m_bFinished;
	int m_iFinishMs = -1;

	//------------------------------------------------------------------------------------------------
	static LF_RaceHudRacer Create(string uid, int passes, int passMs, bool finished, int finishMs)
	{
		LF_RaceHudRacer racer = new LF_RaceHudRacer();
		racer.m_sUid = uid;
		racer.m_iPasses = passes;
		racer.m_iPassMs = passMs;
		racer.m_bFinished = finished;
		racer.m_iFinishMs = finishMs;
		return racer;
	}
}

//------------------------------------------------------------------------------------------------
//! What one racer's panel shows. Built on the server; sent as plain ints.
class LF_RaceHudState
{
	//! 1-based lap the racer is on; the last lap once finished.
	int m_iLap;
	int m_iLaps;
	//! 1-based index of the next checkpoint in the lap; 0 once finished.
	int m_iCheckpoint;
	int m_iCheckpoints;
	//! The next checkpoint's id (server side only; the panel shows the number).
	string m_sNext;
	//! 1-based place among the racers still in; 0 when the racer is not found.
	int m_iPlace;
	int m_iRacers;
	bool m_bFinished;

	//------------------------------------------------------------------------------------------------
	//! What decides a resend: anything but the clock.
	string Signature()
	{
		return string.Format("%1/%2 %3/%4 %5/%6 %7", m_iLap, m_iLaps, m_iCheckpoint, m_iCheckpoints, m_iPlace, m_iRacers, m_bFinished);
	}
}

//------------------------------------------------------------------------------------------------
class LF_RaceHud
{
	static const string FEATURE = "Racing";
	static const ResourceName LAYOUT = "{7C3E91A5D2B84F60}UI/Layouts/Racing/LF_RaceHud.layout";
	static const int TICK_MS = 250; // lf-fixed: client clock refresh
	static const int END_SHOW_MS = 6000; // lf-fixed: how long the panel stays after the race ends
	static const string ROW_LAP = "Lap";
	static const string ROW_NEXT = "Next";
	static const string ROW_PLACE = "Place";
	static const string ROW_TIME = "Time";

	// Server: uid -> the last signature sent, and the place-order inputs the runner does not keep.
	protected static ref map<string, string> s_mSent;
	protected static ref map<string, int> s_mPasses;
	protected static ref map<string, int> s_mPassMs;

	// Client: the panel and its clock.
	protected static Widget s_wRoot;
	protected static ref LF_RaceHudState s_Shown;
	protected static int s_iBaseMs;
	protected static int s_iBaseTick;
	protected static bool s_bClockRuns;
	protected static int s_iHideAtTick;

	//------------------------------------------------------------------------------------------------
	//! Checkpoints a progress has passed over the whole race.
	static int PassesOf(LF_RaceCourseRecord course, LF_RaceProgress progress)
	{
		if (!course || !progress || !course.m_aCheckpoints)
			return 0;

		int count = course.m_aCheckpoints.Count();
		if (progress.m_bFinished)
			return course.m_iLaps * count;

		return progress.m_iLapsDone * count + progress.m_iNextIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! The racer's 1-based place: finishers first by finish time, then most checkpoints passed, then
	//! who reached that count first, then uid so the order never flickers. 0 when uid is not a racer.
	static int PlaceOf(array<ref LF_RaceHudRacer> racers, string uid)
	{
		if (!racers)
			return 0;

		LF_RaceHudRacer me;
		foreach (LF_RaceHudRacer racer : racers)
		{
			if (racer && racer.m_sUid == uid)
			{
				me = racer;
				break;
			}
		}

		if (!me)
			return 0;

		int place = 1;
		foreach (LF_RaceHudRacer other : racers)
		{
			if (other && other != me && Ahead(other, me))
				place++;
		}

		return place;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a ranks above b.
	static bool Ahead(LF_RaceHudRacer a, LF_RaceHudRacer b)
	{
		if (a.m_bFinished != b.m_bFinished)
			return a.m_bFinished;

		if (a.m_bFinished)
		{
			if (a.m_iFinishMs != b.m_iFinishMs)
				return a.m_iFinishMs < b.m_iFinishMs;
		}
		else
		{
			if (a.m_iPasses != b.m_iPasses)
				return a.m_iPasses > b.m_iPasses;

			if (a.m_iPassMs != b.m_iPassMs)
				return a.m_iPassMs < b.m_iPassMs;
		}

		return a.m_sUid < b.m_sUid;
	}

	//------------------------------------------------------------------------------------------------
	//! The panel model for one racer. Null (logged) without a course or progress.
	static LF_RaceHudState Build(LF_RaceCourseRecord course, LF_RaceProgress progress, int place, int racers)
	{
		if (!course || !progress || !course.m_aCheckpoints || course.m_aCheckpoints.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "race panel not built: no course or progress");
			return null;
		}

		LF_RaceHudState state = new LF_RaceHudState();
		state.m_iLaps = course.m_iLaps;
		state.m_iCheckpoints = course.m_aCheckpoints.Count();
		state.m_iPlace = place;
		state.m_iRacers = racers;
		state.m_bFinished = progress.m_bFinished;
		if (progress.m_bFinished)
		{
			state.m_iLap = course.m_iLaps;
			state.m_iCheckpoint = 0;
			return state;
		}

		state.m_iLap = Math.ClampInt(progress.m_iLapsDone + 1, 1, course.m_iLaps);
		state.m_iCheckpoint = progress.m_iNextIndex + 1;
		state.m_sNext = LF_RaceCourse.NextCheckpoint(course, progress);
		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! m:ss.t of a race time.
	static string Clock(int ms)
	{
		if (ms < 0)
			ms = 0;

		int tenths = ms / 100;
		int minutes = tenths / 600;
		int seconds = (tenths / 10) % 60;
		string pad = "";
		if (seconds < 10)
			pad = "0";

		return string.Format("%1:%2%3.%4", minutes, pad, seconds, tenths % 10);
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------
	//! After each race tick or timer: sends every racer whose panel changed, and ends the panel of
	//! every racer whose race is over. Server only.
	static void Sync()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld())
			return;

		if (!s_mSent)
			s_mSent = new map<string, string>();
		if (!s_mPasses)
			s_mPasses = new map<string, int>();
		if (!s_mPassMs)
			s_mPassMs = new map<string, int>();

		float now = GetGame().GetWorld().GetWorldTime();
		set<string> racing = new set<string>();
		foreach (string courseId, LF_RaceInstance race : LF_RaceRunner.LF_HudRaces())
		{
			if (race.m_eState != LF_ERaceState.RACING)
				continue;

			int elapsed = (int)(now - race.m_fStartMs);
			array<ref LF_RaceHudRacer> order = {};
			foreach (LF_RaceEntrant entrant : race.m_aEntrants)
			{
				int passes = PassesOf(race.m_Course, entrant.m_Progress);
				int passMs = 0;
				if (s_mPasses.Contains(entrant.m_sUid))
					passMs = s_mPassMs.Get(entrant.m_sUid);

				if (!s_mPasses.Contains(entrant.m_sUid) || s_mPasses.Get(entrant.m_sUid) != passes)
				{
					if (s_mPasses.Contains(entrant.m_sUid))
						passMs = elapsed;

					s_mPasses.Set(entrant.m_sUid, passes);
					s_mPassMs.Set(entrant.m_sUid, passMs);
				}

				order.Insert(LF_RaceHudRacer.Create(entrant.m_sUid, passes, passMs, entrant.m_Progress.m_bFinished, entrant.m_Progress.m_iFinishMs));
			}

			foreach (LF_RaceEntrant racer : race.m_aEntrants)
			{
				racing.Insert(racer.m_sUid);
				LF_RaceHudState state = Build(race.m_Course, racer.m_Progress, PlaceOf(order, racer.m_sUid), order.Count());
				if (!state)
					continue;

				string signature = state.Signature();
				if (s_mSent.Contains(racer.m_sUid) && s_mSent.Get(racer.m_sUid) == signature)
					continue;

				int clockMs = elapsed;
				if (racer.m_Progress.m_bFinished)
					clockMs = racer.m_Progress.m_iFinishMs;

				SCR_PlayerController controller = ControllerOf(racer);
				if (!controller)
					continue;

				s_mSent.Set(racer.m_sUid, signature);
				controller.LF_RaceHudState(state.m_iLap, state.m_iLaps, state.m_iCheckpoint, state.m_iCheckpoints, state.m_iPlace, state.m_iRacers, clockMs, state.m_bFinished);
				LF_Debug.Info(FEATURE, string.Format("race '%1' panel to %2: %3 next '%4'", courseId, racer.m_sUid, signature, state.m_sNext));
			}
		}

		array<string> over = {};
		foreach (string sentUid, string sent : s_mSent)
		{
			if (!racing.Contains(sentUid))
				over.Insert(sentUid);
		}

		foreach (string overUid : over)
		{
			s_mSent.Remove(overUid);
			s_mPasses.Remove(overUid);
			s_mPassMs.Remove(overUid);
			SCR_PlayerController endController = ControllerOfUid(overUid);
			if (endController)
				endController.LF_RaceHudEnd();

			LF_Debug.Info(FEATURE, string.Format("race panel ended for %1", overUid));
		}

		// A racer who never got a panel (no controller yet) is dropped from the pass tracking too.
		array<string> stale = {};
		foreach (string trackedUid, int tracked : s_mPasses)
		{
			if (!racing.Contains(trackedUid))
				stale.Insert(trackedUid);
		}

		foreach (string staleUid : stale)
		{
			s_mPasses.Remove(staleUid);
			s_mPassMs.Remove(staleUid);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController ControllerOf(LF_RaceEntrant racer)
	{
		IEntity body = LF_RaceRunner.LF_HudBody(racer);
		PlayerManager players = GetGame().GetPlayerManager();
		if (!body || !players)
			return null;

		int playerId = players.GetPlayerIdFromControlledEntity(body);
		if (playerId <= 0)
			return null;

		return SCR_PlayerController.Cast(players.GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController ControllerOfUid(string uid)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return null;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Client
	//------------------------------------------------------------------------------------------------
	//! A panel update from the server. Client only.
	static void Show(LF_RaceHudState state, int clockMs)
	{
		if (!state)
			return;

		if (!s_wRoot && !CreatePanel())
			return;

		s_Shown = state;
		s_iBaseMs = clockMs;
		s_iBaseTick = System.GetTickCount();
		s_bClockRuns = !state.m_bFinished;
		s_iHideAtTick = 0;
		SetRow(ROW_LAP, "#LF-RaceHud_Lap", string.Format("%1 / %2", state.m_iLap, state.m_iLaps));
		if (state.m_bFinished)
			SetRow(ROW_NEXT, "#LF-RaceHud_Next", WidgetManager.Translate("#LF-RaceHud_Finished"));
		else
			SetRow(ROW_NEXT, "#LF-RaceHud_Next", string.Format("%1 / %2", state.m_iCheckpoint, state.m_iCheckpoints));

		SetRow(ROW_PLACE, "#LF-RaceHud_Place", string.Format("%1 / %2", state.m_iPlace, state.m_iRacers));
		SetRow(ROW_TIME, "#LF-RaceHud_Time", Clock(clockMs));
		LF_Debug.Info(FEATURE, string.Format("race panel shown: %1 time %2", state.Signature(), Clock(clockMs)));
	}

	//------------------------------------------------------------------------------------------------
	//! The race is over for this racer: the clock stops and the panel goes after a few seconds.
	static void End()
	{
		if (!s_wRoot)
			return;

		if (s_bClockRuns)
			s_iBaseMs = s_iBaseMs + System.GetTickCount() - s_iBaseTick;

		s_bClockRuns = false;
		s_iHideAtTick = System.GetTickCount() + END_SHOW_MS;
		SetRow(ROW_NEXT, "#LF-RaceHud_Next", WidgetManager.Translate("#LF-RaceHud_Over"));
		SetRow(ROW_TIME, "#LF-RaceHud_Time", Clock(s_iBaseMs));
		LF_Debug.Info(FEATURE, "race panel: race over");
	}

	//------------------------------------------------------------------------------------------------
	//! The panel: a theme-metal plate holding a heading and four kit status rows (LF_UITheme layouts).
	protected static bool CreatePanel()
	{
		WorkspaceWidget workspace;
		if (GetGame())
			workspace = GetGame().GetWorkspace();

		if (workspace)
			s_wRoot = workspace.CreateWidgets(LAYOUT);

		if (!s_wRoot)
		{
			LF_Debug.Error(FEATURE, "the race panel did not load; the race runs without it");
			return false;
		}

		Widget plate = s_wRoot.FindAnyWidget("Plate");
		if (plate)
			plate.SetColor(LF_UITheme.Metal());

		Widget rows = s_wRoot.FindAnyWidget("Rows");
		if (!rows)
		{
			LF_Debug.Error(FEATURE, "the race panel has no Rows; the race runs without it");
			Remove();
			return false;
		}

		TextWidget heading = TextWidget.Cast(workspace.CreateWidgets(LF_UITheme.HEADING_LAYOUT, rows));
		if (heading)
		{
			heading.SetText("#LF-RaceHud_Title");
			heading.SetColor(LF_UITheme.Paper());
		}

		AddRow(workspace, rows, ROW_LAP);
		AddRow(workspace, rows, ROW_NEXT);
		AddRow(workspace, rows, ROW_PLACE);
		AddRow(workspace, rows, ROW_TIME);
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddRow(WorkspaceWidget workspace, Widget rows, string name)
	{
		Widget row = workspace.CreateWidgets(LF_UITheme.STATUS_LAYOUT, rows);
		if (!row)
		{
			LF_Debug.Error(FEATURE, "race panel row " + name + " did not load");
			return;
		}

		row.SetName(name);
		Widget action = row.FindAnyWidget("ActionButton");
		if (action)
			action.SetVisible(false);

		Widget platePart = row.FindAnyWidget("RowPlate");
		if (platePart)
			platePart.SetColor(LF_UITheme.Paper());

		TextWidget label = TextWidget.Cast(row.FindAnyWidget("RowLabel"));
		if (label)
			label.SetColor(LF_UITheme.Ink());

		TextWidget status = TextWidget.Cast(row.FindAnyWidget("RowStatus"));
		if (status)
			status.SetColor(LF_UITheme.Ink());
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetRow(string name, string label, string status)
	{
		if (!s_wRoot)
			return;

		Widget row = s_wRoot.FindAnyWidget(name);
		if (!row)
			return;

		TextWidget labelWidget = TextWidget.Cast(row.FindAnyWidget("RowLabel"));
		if (labelWidget)
			labelWidget.SetText(label);

		TextWidget statusWidget = TextWidget.Cast(row.FindAnyWidget("RowStatus"));
		if (statusWidget)
			statusWidget.SetText(status);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_wRoot)
		{
			Remove();
			return;
		}

		int now = System.GetTickCount();
		if (s_iHideAtTick > 0 && now >= s_iHideAtTick)
		{
			Remove();
			return;
		}

		if (s_bClockRuns)
			SetRow(ROW_TIME, "#LF-RaceHud_Time", Clock(s_iBaseMs + now - s_iBaseTick));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_Shown = null;
		s_bClockRuns = false;
		s_iHideAtTick = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness: what the panel shows now.
	static string Describe()
	{
		if (!s_wRoot || !s_Shown)
			return "hidden";

		int clockMs = s_iBaseMs;
		if (s_bClockRuns)
			clockMs = s_iBaseMs + System.GetTickCount() - s_iBaseTick;

		return string.Format("shown %1 time=%2 running=%3", s_Shown.Signature(), Clock(clockMs), s_bClockRuns);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		Remove();
		s_mSent = null;
		s_mPasses = null;
		s_mPassMs = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The runner's table, read by the panel. Only reads; the runner stays the only writer.
modded class LF_RaceRunner
{
	//------------------------------------------------------------------------------------------------
	static map<string, ref LF_RaceInstance> LF_HudRaces()
	{
		return Races();
	}

	//------------------------------------------------------------------------------------------------
	static IEntity LF_HudBody(LF_RaceEntrant entrant)
	{
		return ResolveBody(entrant);
	}
}

//------------------------------------------------------------------------------------------------
//! The panel follows the runner's own clock: after each detection tick, lobby close and time limit.
modded class LF_RaceClock
{
	//------------------------------------------------------------------------------------------------
	override void LobbyDue(string courseId, int serial)
	{
		super.LobbyDue(courseId, serial);
		LF_RaceHud.Sync();
	}

	//------------------------------------------------------------------------------------------------
	override void LimitDue(string courseId, int serial)
	{
		super.LimitDue(courseId, serial);
		LF_RaceHud.Sync();
	}

	//------------------------------------------------------------------------------------------------
	override void Tick()
	{
		super.Tick();
		LF_RaceHud.Sync();
	}
}

//------------------------------------------------------------------------------------------------
//! The wire: server to the racer's own client. Nothing goes the other way.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: this player's race panel changed.
	void LF_RaceHudState(int lap, int laps, int checkpoint, int checkpoints, int place, int racers, int clockMs, bool finished)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_RaceHudState(lap, laps, checkpoint, checkpoints, place, racers, clockMs, finished);
		else
			Rpc(RpcDo_LF_RaceHudState, lap, laps, checkpoint, checkpoints, place, racers, clockMs, finished);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RaceHudState(int lap, int laps, int checkpoint, int checkpoints, int place, int racers, int clockMs, bool finished)
	{
		LF_RaceHudState state = new LF_RaceHudState();
		state.m_iLap = lap;
		state.m_iLaps = laps;
		state.m_iCheckpoint = checkpoint;
		state.m_iCheckpoints = checkpoints;
		state.m_iPlace = place;
		state.m_iRacers = racers;
		state.m_bFinished = finished;
		LF_RaceHud.Show(state, clockMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: this player's race is over.
	void LF_RaceHudEnd()
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_RaceHudEnd();
		else
			Rpc(RpcDo_LF_RaceHudEnd);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_RaceHudEnd()
	{
		LF_RaceHud.End();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RaceHud's statics (registered at script load; Core/LF_StaticReset.c).
class LF_RaceHud_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_RaceHud_StaticResetHook());

	override string Owner()
	{
		return "LF_RaceHud";
	}

	override void Apply()
	{
		LF_RaceHud.ResetStatics();
	}
}
