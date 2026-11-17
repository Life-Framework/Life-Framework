// LF_RaceRunner.c - one race per course, server only (#1923). A racer pays the entry fee from
// carried cash into the pot at the start sign; the lobby closes after the course's lobby seconds;
// too few entrants refunds everyone; the server watches checkpoint crossings and pays the pot by
// the course split when every racer still in has finished (or the time limit runs out). A leaver
// forfeits the fee, and so does a racer who stays without a living body past the course's AFK grace;
// after the first finisher the race settles at the course's settle delay (#2406). The pot is session state, never persisted: a session end mid-race refunds
// everyone still in before the engine's shutdown save. Money moves only through LF_MoneyUtils.

enum LF_ERaceState
{
	IDLE,
	LOBBY,
	RACING
}

enum LF_ERaceJoin
{
	JOINED,
	ALREADY_IN,
	NOT_ENOUGH_CASH,
	//! The race is running; wait for the next lobby.
	CLOSED,
	FULL,
	BAD_COURSE,
	NO_PLAYER
}

//------------------------------------------------------------------------------------------------
//! How the runner sees the players: the live PlayerManager. A test installs a subclass with a
//! synthetic population (LF_RaceRunner.SetPlayers) to count and time the lookups at 128 players.
class LF_RacePlayers : Managed
{
	//------------------------------------------------------------------------------------------------
	bool Available()
	{
		return GetGame() && GetGame().GetPlayerManager();
	}

	//------------------------------------------------------------------------------------------------
	void GetPlayers(notnull array<int> outIds)
	{
		outIds.Clear();
		if (Available())
			GetGame().GetPlayerManager().GetPlayers(outIds);
	}

	//------------------------------------------------------------------------------------------------
	string UidOf(int playerId)
	{
		return LF_Utils.GetPlayerUID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	IEntity ControlledBy(int playerId)
	{
		if (!Available())
			return null;

		return GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
	}

	//------------------------------------------------------------------------------------------------
	int PlayerOf(IEntity body)
	{
		if (!body || !Available())
			return 0;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(body);
	}
}

//------------------------------------------------------------------------------------------------
//! One racer in one race. The body is re-resolved at every use through the racer's cached player
//! id, checked with one uid compare (#2404); the stored body id is only the fallback for a body
//! that has no player (a test fixture).
class LF_RaceEntrant
{
	string m_sUid;
	//! The player this racer was last seen as; 0 when unknown. Checked against the uid at every use.
	int m_iPlayerId;
	//! World ms before which a racer with no online player is not looked up again.
	float m_fRescanAtMs;
	EntityID m_BodyId = EntityID.INVALID;
	int m_iFee;
	ref LF_RaceProgress m_Progress = new LF_RaceProgress();
	//! World position at the last detection tick, so a fast car is checked along its path.
	vector m_vLast;
	bool m_bHasLast;
	//! The body the last position belongs to. A new body (respawn, character switch) starts a new path.
	EntityID m_LastBodyId = EntityID.INVALID;
	//! World ms since which this racer has had no living body; -1 while there is one (#2406).
	float m_fAbsentSinceMs = -1;

	//------------------------------------------------------------------------------------------------
	//! Notes whether the racer had a living body at nowMs. \return true once the racer has been without
	//! one for graceSeconds (0 never drops a racer).
	bool AbsentTooLong(bool hasBody, float nowMs, int graceSeconds)
	{
		if (hasBody)
		{
			m_fAbsentSinceMs = -1;
			return false;
		}

		if (m_fAbsentSinceMs < 0)
			m_fAbsentSinceMs = nowMs;

		return graceSeconds > 0 && nowMs - m_fAbsentSinceMs >= graceSeconds * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Where this tick's path starts for body bodyId standing at now, and remembers now for the next
	//! tick. The path is only joined to the last spot when the same body stood there: a death,
	//! respawn or character switch would otherwise draw a line from the old body to the new one
	//! and pass any checkpoint on it (docs/reviews/2026-11-17-life.md).
	vector Step(EntityID bodyId, vector now)
	{
		vector from = now;
		if (m_bHasLast && bodyId != EntityID.INVALID && bodyId == m_LastBodyId)
			from = m_vLast;

		m_vLast = now;
		m_bHasLast = true;
		m_LastBodyId = bodyId;
		return from;
	}

	//------------------------------------------------------------------------------------------------
	//! No living body this tick: the next one starts a new path.
	void Lose()
	{
		m_bHasLast = false;
		m_LastBodyId = EntityID.INVALID;
	}
}

//------------------------------------------------------------------------------------------------
class LF_RaceInstance
{
	ref LF_RaceCourseRecord m_Course;
	LF_ERaceState m_eState = LF_ERaceState.LOBBY;
	//! Bumped per race, so a lobby or time-limit timer from an earlier race does nothing.
	int m_iSerial;
	int m_iPot;
	int m_iStarted;
	float m_fStartMs;
	//! World ms of the first finisher's finish; -1 until someone finishes (#2406).
	float m_fFirstFinishMs = -1;
	ref array<ref LF_RaceEntrant> m_aEntrants = new array<ref LF_RaceEntrant>();
	//! Racers who left or were dropped after the start. Their fees stay in the pot; only a session end pays them back.
	ref array<ref LF_RaceEntrant> m_aLeavers = new array<ref LF_RaceEntrant>();

	//------------------------------------------------------------------------------------------------
	LF_RaceEntrant Find(string uid)
	{
		foreach (LF_RaceEntrant entrant : m_aEntrants)
		{
			if (entrant.m_sUid == uid)
				return entrant;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The settle delay after the first finisher has run out at nowMs (the course's m_iSettleSeconds, 0 off).
	bool SettleDue(float nowMs)
	{
		if (!m_Course || m_Course.m_iSettleSeconds <= 0 || m_fFirstFinishMs < 0)
			return false;

		return nowMs - m_fFirstFinishMs >= m_Course.m_iSettleSeconds * 1000;
	}
}

//------------------------------------------------------------------------------------------------
//! The timers. A Managed instance so the call queue holds instance methods, not statics.
class LF_RaceClock : Managed
{
	//------------------------------------------------------------------------------------------------
	void LobbyDue(string courseId, int serial)
	{
		LF_RaceRunner.LobbyDue(courseId, serial);
	}

	//------------------------------------------------------------------------------------------------
	void LimitDue(string courseId, int serial)
	{
		LF_RaceRunner.LimitDue(courseId, serial);
	}

	//------------------------------------------------------------------------------------------------
	void Tick()
	{
		LF_RaceRunner.Tick();
	}
}

//------------------------------------------------------------------------------------------------
class LF_RaceRunner
{
	static const string FEATURE = "Racing";
	static const string SYSTEM = "Racing";
	static const string REASON_FEE = "race_fee";
	static const string REASON_REFUND = "race_refund";
	static const string REASON_PRIZE = "race_prize";
	static const int MAX_ENTRANTS = 16; // lf-fixed: a lobby bound, not a balance knob; a per-course cap belongs on LF_RaceCourseRecord in a later slice
	//! A race that runs this long ends: finishers are paid, and with no finisher everyone still in is refunded.
	static const int LIMIT_MS = 900000; // lf-fixed: a safety stop for a stuck race; a per-course limit belongs on LF_RaceCourseRecord in a later slice
	//! How often the server checks racers against their next checkpoint.
	static const int TICK_MS = 250; // lf-fixed: detection cadence tied to the default 12 m checkpoint radius

	//! How often a racer with no online player is looked up again by a full player scan.
	static const int RESCAN_MS = 1000; // lf-fixed: lookup cadence for an offline racer, not gameplay tuning

	protected static ref map<string, ref LF_RaceInstance> s_mRaces;
	protected static ref LF_RaceClock s_Clock;
	protected static ref LF_RacePlayers s_Players;
	//! Full player scans since the last reset (a perf and regression counter).
	protected static int s_iScans;
	protected static bool s_bTicking;
	protected static int s_iSerial;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_RaceInstance> Races()
	{
		if (!s_mRaces)
			s_mRaces = new map<string, ref LF_RaceInstance>();

		return s_mRaces;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_RaceClock Clock()
	{
		if (!s_Clock)
			s_Clock = new LF_RaceClock();

		return s_Clock;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_RacePlayers Players()
	{
		if (!s_Players)
			s_Players = new LF_RacePlayers();

		return s_Players;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: the population the runner sees; null goes back to the live PlayerManager.
	static void SetPlayers(LF_RacePlayers players)
	{
		s_Players = players;
	}

	//------------------------------------------------------------------------------------------------
	//! Full player scans since the last reset.
	static int GetScanCount()
	{
		return s_iScans;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the 250 ms checkpoint tick is on the call queue. It runs only while a race is RACING.
	static bool IsTicking()
	{
		return s_bTicking;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ERaceState GetState(string courseId)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race)
			return LF_ERaceState.IDLE;

		return race.m_eState;
	}

	//------------------------------------------------------------------------------------------------
	static int GetPot(string courseId)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race)
			return 0;

		return race.m_iPot;
	}

	//------------------------------------------------------------------------------------------------
	static int GetEntrantCount(string courseId)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race)
			return 0;

		return race.m_aEntrants.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The course a racer is in, or empty.
	static string CourseOf(string uid)
	{
		foreach (string courseId, LF_RaceInstance race : Races())
		{
			if (race.Find(uid))
				return courseId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! A player pays the entry fee at the start sign. Server only. The first entrant opens the lobby;
	//! the fee leaves carried cash in full or not at all.
	static LF_ERaceJoin Join(LF_RaceCourseRecord course, string uid, IEntity body)
	{
		if (!Replication.IsServer())
			return LF_ERaceJoin.CLOSED;

		if (!course || !LF_RaceCourse.IsValid(course))
		{
			LF_Debug.Warn(FEATURE, "race join refused: no usable course");
			return LF_ERaceJoin.BAD_COURSE;
		}

		if (uid.IsEmpty() || !body)
		{
			LF_Debug.Warn(FEATURE, string.Format("race '%1' join refused: no player uid or body", course.m_sId));
			return LF_ERaceJoin.NO_PLAYER;
		}

		if (!CourseOf(uid).IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("race '%1' join refused: %2 is already entered", course.m_sId, uid));
			return LF_ERaceJoin.ALREADY_IN;
		}

		LF_RaceInstance race = Races().Get(course.m_sId);
		if (race && race.m_eState != LF_ERaceState.LOBBY)
		{
			LF_Debug.Info(FEATURE, string.Format("race '%1' join refused for %2: the race is running", course.m_sId, uid));
			return LF_ERaceJoin.CLOSED;
		}

		if (race && race.m_aEntrants.Count() >= MAX_ENTRANTS)
		{
			LF_Debug.Info(FEATURE, string.Format("race '%1' join refused for %2: the lobby is full", course.m_sId, uid));
			return LF_ERaceJoin.FULL;
		}

		int fee = course.m_iEntryFee;
		if (fee > 0)
		{
			if (LF_MoneyUtils.GetCash(body) < fee)
			{
				LF_Debug.Info(FEATURE, string.Format("race '%1' join refused for %2: carries less than the %3 fee", course.m_sId, uid, fee));
				return LF_ERaceJoin.NOT_ENOUGH_CASH;
			}

			int removed = LF_MoneyUtils.RemoveCash(body, fee, REASON_FEE);
			if (removed != fee)
			{
				// A partial take goes straight back; the racer is not entered.
				if (removed > 0)
				{
					LF_RaceEntrant giveBack = new LF_RaceEntrant();
					giveBack.m_sUid = uid;
					giveBack.m_BodyId = body.GetID();
					PayTo(giveBack, removed, REASON_REFUND, course.m_sId);
				}

				LF_Debug.Warn(FEATURE, string.Format("race '%1' join refused for %2: took %3 of the %4 fee, handed it back", course.m_sId, uid, removed, fee));
				return LF_ERaceJoin.NOT_ENOUGH_CASH;
			}
		}
		else
		{
			fee = 0;
		}

		if (!race)
		{
			race = new LF_RaceInstance();
			race.m_Course = course;
			s_iSerial++;
			race.m_iSerial = s_iSerial;
			Races().Set(course.m_sId, race);
			ScheduleLobby(race);
			LF_Debug.Info(FEATURE, string.Format("race '%1' lobby open for %2 s", course.m_sId, course.m_iLobbySeconds));
		}

		LF_RaceEntrant entrant = new LF_RaceEntrant();
		entrant.m_sUid = uid;
		entrant.m_BodyId = body.GetID();
		entrant.m_iPlayerId = Players().PlayerOf(body);
		entrant.m_iFee = fee;
		race.m_aEntrants.Insert(entrant);
		race.m_iPot = race.m_iPot + fee;
		LF_Debug.Info(FEATURE, string.Format("race '%1' %2 entered, fee %3, pot %4, entrants %5", course.m_sId, uid, fee, race.m_iPot, race.m_aEntrants.Count()));
		return LF_ERaceJoin.JOINED;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleLobby(LF_RaceInstance race)
	{
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetCallqueue())
			return;

		int ms = race.m_Course.m_iLobbySeconds * 1000;
		game.GetCallqueue().CallLater(Clock().LobbyDue, ms, false, race.m_Course.m_sId, race.m_iSerial);
	}

	//------------------------------------------------------------------------------------------------
	static void LobbyDue(string courseId, int serial)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race || race.m_iSerial != serial || race.m_eState != LF_ERaceState.LOBBY)
			return;

		CloseLobby(courseId);
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the lobby: starts the race with enough entrants, otherwise refunds every entrant.
	//! \return true when the race started.
	static bool CloseLobby(string courseId)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race || race.m_eState != LF_ERaceState.LOBBY)
			return false;

		if (race.m_aEntrants.Count() < race.m_Course.m_iMinEntrants)
		{
			LF_Debug.Info(FEATURE, string.Format("race '%1' cancelled: %2 of %3 entrants", courseId, race.m_aEntrants.Count(), race.m_Course.m_iMinEntrants));
			Refund(Detach(courseId), "too few entrants", false);
			return false;
		}

		race.m_eState = LF_ERaceState.RACING;
		race.m_iStarted = race.m_aEntrants.Count();
		race.m_fStartMs = NowMs();
		ScheduleLimit(race);
		StartTicking();
		foreach (LF_RaceEntrant entrant : race.m_aEntrants)
			Notify(entrant, "#LF-Race_Started", "");

		LF_Debug.Info(FEATURE, string.Format("race '%1' started: %2 racers, pot %3", courseId, race.m_iStarted, race.m_iPot));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleLimit(LF_RaceInstance race)
	{
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetCallqueue())
			return;

		game.GetCallqueue().CallLater(Clock().LimitDue, LIMIT_MS, false, race.m_Course.m_sId, race.m_iSerial);
	}

	//------------------------------------------------------------------------------------------------
	static void LimitDue(string courseId, int serial)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race || race.m_iSerial != serial || race.m_eState != LF_ERaceState.RACING)
			return;

		LF_Debug.Info(FEATURE, string.Format("race '%1' time limit reached", courseId));
		Settle(courseId);
	}

	//------------------------------------------------------------------------------------------------
	//! A racer leaves (disconnect). While the race is in its LOBBY the fee goes back at once, to the
	//! racer's bank (never to the leaving body, so the disconnect save carries no duplicate), and an
	//! emptied lobby closes. Once RACING the racer is dropped and the fee stays in the pot. Nothing
	//! is paid to a leaving body either way.
	//! \return true when the racer was in a race.
	static bool Forfeit(string uid)
	{
		return Drop(uid, "left", "");
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a racer out of the race: a leaver (disconnect) or one dropped for being without a living
	//! body too long. \param toastKey tells the racer, when online, why; "" says nothing.
	//! \return true when the racer was in a race and not a finisher.
	protected static bool Drop(string uid, string why, string toastKey)
	{
		string courseId = CourseOf(uid);
		if (courseId.IsEmpty())
			return false;

		LF_RaceInstance race = Races().Get(courseId);
		LF_RaceEntrant entrant = race.Find(uid);
		if (entrant.m_Progress.m_bFinished)
		{
			// A finisher keeps the place; the prize goes to the bank when the race settles.
			LF_Debug.Info(FEATURE, string.Format("race '%1' finisher %2 %3; the place stands", courseId, uid, why));
			return false;
		}

		if (race.m_eState == LF_ERaceState.LOBBY)
		{
			race.m_aEntrants.RemoveItem(entrant);
			race.m_iPot = race.m_iPot - entrant.m_iFee;
			PayToBank(entrant, entrant.m_iFee, entrant.m_iFee, REASON_REFUND, courseId);
			LF_Debug.Info(FEATURE, string.Format("race '%1' %2 %3 in the lobby; the %4 fee went back to the bank; pot %5", courseId, uid, why, entrant.m_iFee, race.m_iPot));
			if (race.m_aEntrants.IsEmpty())
			{
				Detach(courseId);
				LF_Debug.Info(FEATURE, string.Format("race '%1' lobby closed: nobody is left", courseId));
			}

			return true;
		}

		LF_Debug.Info(FEATURE, string.Format("race '%1' %2 %3 and forfeits the %4 fee; pot %5", courseId, uid, why, entrant.m_iFee, race.m_iPot));
		race.m_aLeavers.Insert(entrant);
		race.m_aEntrants.RemoveItem(entrant);
		if (!toastKey.IsEmpty())
			NotifyOnline(entrant, toastKey, "");

		SettleIfDone(courseId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A racer reached a checkpoint timeMs after the start. Server only. The course rules decide;
	//! the race settles once every racer still in has finished.
	static LF_ERaceCheck PassAt(string courseId, string uid, string checkpointId, int timeMs)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race || race.m_eState != LF_ERaceState.RACING)
			return LF_ERaceCheck.CLOSED;

		LF_RaceEntrant entrant = race.Find(uid);
		if (!entrant)
			return LF_ERaceCheck.CLOSED;

		LF_ERaceCheck verdict = LF_RaceCourse.Pass(race.m_Course, entrant.m_Progress, checkpointId, timeMs);
		if (verdict == LF_ERaceCheck.ACCEPTED || verdict == LF_ERaceCheck.LAP_DONE)
			Notify(entrant, "#LF-Race_Checkpoint", LF_RaceCourse.NextCheckpoint(race.m_Course, entrant.m_Progress));
		else if (verdict == LF_ERaceCheck.FINISHED)
		{
			if (race.m_fFirstFinishMs < 0)
				race.m_fFirstFinishMs = race.m_fStartMs + timeMs;

			SettleIfDone(courseId);
			WarnStragglers(race);
		}

		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	//! After the first finisher, tells the racers still out how long the race waits for them (#2406).
	protected static void WarnStragglers(LF_RaceInstance race)
	{
		if (Races().Get(race.m_Course.m_sId) != race || race.m_Course.m_iSettleSeconds <= 0)
			return;

		int finished = 0;
		foreach (LF_RaceEntrant entrant : race.m_aEntrants)
		{
			if (entrant.m_Progress.m_bFinished)
				finished++;
		}

		if (finished != 1)
			return;

		foreach (LF_RaceEntrant racer : race.m_aEntrants)
		{
			if (!racer.m_Progress.m_bFinished)
				NotifyOnline(racer, "#LF-Race_Closing", race.m_Course.m_iSettleSeconds.ToString());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void SettleIfDone(string courseId)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race)
			return;

		foreach (LF_RaceEntrant entrant : race.m_aEntrants)
		{
			if (!entrant.m_Progress.m_bFinished)
				return;
		}

		Settle(courseId);
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the race and pays the pot by the split among finishers whose body resolves. The winner
	//! also takes what the split leaves (rounding, unfilled places). No finisher refunds the racers
	//! still in. The race is detached before any money moves, so nothing can pay it twice.
	protected static void Settle(string courseId)
	{
		LF_RaceInstance race = Detach(courseId);
		if (!race)
			return;

		array<LF_RaceEntrant> finishers = {};
		array<int> times = {};
		foreach (LF_RaceEntrant entrant : race.m_aEntrants)
		{
			if (!entrant.m_Progress.m_bFinished)
				continue;

			finishers.Insert(entrant);
			times.Insert(entrant.m_Progress.m_iFinishMs);
		}

		if (finishers.IsEmpty())
		{
			if (race.m_aEntrants.IsEmpty())
				LF_Debug.Warn(FEATURE, string.Format("race '%1' ended with nobody left; the %2 pot is forfeit", courseId, race.m_iPot));
			else
				Refund(race, "nobody finished", false);

			return;
		}

		array<int> order = {};
		LF_RaceCourse.FinishOrder(times, order);
		array<int> shares = {};
		int paid = 0;
		for (int place = 0; place < order.Count(); place++)
		{
			int share = LF_RaceCourse.PayoutFor(race.m_Course, place + 1, race.m_iPot);
			shares.Insert(share);
			paid += share;
		}

		shares[0] = shares[0] + race.m_iPot - paid;
		for (int p = 0; p < order.Count(); p++)
		{
			int i = order[p];
			if (shares[p] <= 0)
				continue;

			PayTo(finishers[i], shares[p], REASON_PRIZE, courseId);
			Notify(finishers[i], "#LF-Race_Prize", LF_MoneyFormat.FormatMoney(shares[p]));
			LF_Debug.Info(FEATURE, string.Format("race '%1' place %2 %3 paid %4", courseId, p + 1, finishers[i].m_sUid, shares[p]));
		}

		LF_RaceEntrant winner = finishers[order[0]];
		LF_OnRaceFinished finish = LF_OnRaceFinished.Create(courseId, winner.m_sUid, race.m_iPot, race.m_iStarted, winner.m_Progress.m_iFinishMs);
		// Every finisher, in finish order, so the record board can keep each one's personal best (#2160).
		foreach (int at : order)
		{
			finish.AddFinisher(finishers[at].m_sUid, finishers[at].m_Progress.m_iFinishMs);
		}

		LF_EventBus.Publish(finish);
		LF_Debug.Info(FEATURE, string.Format("race '%1' finished: %2 wins the %3 pot", courseId, winner.m_sUid, race.m_iPot));
	}

	//------------------------------------------------------------------------------------------------
	//! Session end (or a cancelled lobby): every racer still in gets the fee back. Server only.
	//! \return the number of racers refunded.
	static int AbandonAll(string reason)
	{
		array<string> ids = {};
		foreach (string courseId, LF_RaceInstance race : Races())
			ids.Insert(courseId);

		int refunded = 0;
		foreach (string id : ids)
			refunded += Refund(Detach(id), reason, true);

		return refunded;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_RaceInstance Detach(string courseId)
	{
		LF_RaceInstance race = Races().Get(courseId);
		if (!race)
			return null;

		Races().Remove(courseId);
		if (!AnyRacing())
			StopTicking();

		return race;
	}

	//------------------------------------------------------------------------------------------------
	//! True while some race is RACING: only then the checkpoint tick has work (#2406).
	protected static bool AnyRacing()
	{
		foreach (string courseId, LF_RaceInstance race : Races())
		{
			if (race.m_eState == LF_ERaceState.RACING)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays each entrant still in their own fee back; with leavers, the racers who left too (a
	//! session end, where a disconnect may come before the game end). The race must be detached.
	protected static int Refund(LF_RaceInstance race, string reason, bool leavers)
	{
		if (!race)
			return 0;

		array<LF_RaceEntrant> owed = {};
		foreach (LF_RaceEntrant entrant : race.m_aEntrants)
			owed.Insert(entrant);

		if (leavers)
		{
			foreach (LF_RaceEntrant leaver : race.m_aLeavers)
				owed.Insert(leaver);
		}

		int refunded = 0;
		int returned = 0;
		foreach (LF_RaceEntrant racer : owed)
		{
			PayTo(racer, racer.m_iFee, REASON_REFUND, race.m_Course.m_sId);
			Notify(racer, "#LF-Race_Refunded", LF_MoneyFormat.FormatMoney(racer.m_iFee));
			returned += racer.m_iFee;
			refunded++;
		}

		LF_Debug.Info(FEATURE, string.Format("race '%1' refunded %2 racers, %3 of the %4 pot (%5); the rest is forfeit", race.m_Course.m_sId, refunded, returned, race.m_iPot, reason));
		return refunded;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays amount to a racer: into carried cash when the racer has a living body, and whatever
	//! does not fit (or all of it, with no body) into the racer's bank account. Nothing is lost.
	protected static void PayTo(LF_RaceEntrant entrant, int amount, string reason, string courseId)
	{
		if (amount <= 0)
			return;

		int added = 0;
		IEntity body = ResolveBody(entrant);
		if (body)
			added = LF_MoneyUtils.AddCash(body, amount, reason);

		PayToBank(entrant, amount - added, amount, reason, courseId);
	}

	//------------------------------------------------------------------------------------------------
	//! Pays rest (the part of amount the body could not take) into the racer's bank account, the place for
	//! money that has no living body to go to.
	protected static void PayToBank(LF_RaceEntrant entrant, int rest, int amount, string reason, string courseId)
	{
		if (rest <= 0)
			return;

		LF_BankAccount account = LF_ATMManager.GetOrCreate(entrant.m_sUid);
		if (!account)
		{
			LF_Debug.Error(FEATURE, string.Format("race '%1' could not pay %2 of %3 to %4: no body room and no bank", courseId, rest, amount, entrant.m_sUid));
			return;
		}

		account.Deposit(rest, reason);
		LF_Debug.Info(FEATURE, string.Format("race '%1' paid %2 of %3 to %4's bank (%5)", courseId, rest, amount, entrant.m_sUid, reason));
	}

	//------------------------------------------------------------------------------------------------
	//! The racer's current body: the player's controlled entity when the uid is online, else the
	//! body the racer entered with (a fixture has no player).
	protected static IEntity ResolveBody(LF_RaceEntrant entrant)
	{
		LF_RacePlayers players = Players();
		int playerId = PlayerIdOf(entrant);
		if (playerId > 0)
		{
			// Online: the controlled body if it lives, never the corpse the racer entered with.
			IEntity controlled = players.ControlledBy(playerId);
			if (IsAlive(controlled))
				return controlled;

			return null;
		}

		if (entrant.m_BodyId == EntityID.INVALID || !GetGame().GetWorld())
			return null;

		IEntity entered = GetGame().GetWorld().FindEntityByID(entrant.m_BodyId);
		if (!IsAlive(entered))
			return null;

		// A body some other player now controls is not this racer's.
		if (players.PlayerOf(entered) > 0)
			return null;

		return entered;
	}

	//------------------------------------------------------------------------------------------------
	//! The racer's online player id, or 0. The cached id costs one uid compare; only a miss scans the
	//! players, and a racer found offline is scanned for again at most once per RESCAN_MS (#2404).
	protected static int PlayerIdOf(notnull LF_RaceEntrant entrant)
	{
		LF_RacePlayers players = Players();
		if (!players.Available())
			return 0;

		if (entrant.m_iPlayerId > 0)
		{
			if (players.UidOf(entrant.m_iPlayerId) == entrant.m_sUid)
				return entrant.m_iPlayerId;

			entrant.m_iPlayerId = 0;
			entrant.m_fRescanAtMs = 0;
		}

		float now = NowMs();
		if (entrant.m_fRescanAtMs > now)
			return 0;

		s_iScans++;
		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			if (players.UidOf(playerId) != entrant.m_sUid)
				continue;

			entrant.m_iPlayerId = playerId;
			return playerId;
		}

		entrant.m_fRescanAtMs = now + RESCAN_MS;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsAlive(IEntity body)
	{
		if (!body)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (!character)
			return true;

		CharacterControllerComponent controller = character.GetCharacterController();
		return !controller || !controller.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	protected static vector WorldPosition(IEntity entity)
	{
		vector transform[4];
		entity.GetWorldTransform(transform);
		return transform[3];
	}

	//------------------------------------------------------------------------------------------------
	protected static void Notify(LF_RaceEntrant entrant, string key, string arg)
	{
		IEntity body = ResolveBody(entrant);
		PlayerManager players = GetGame().GetPlayerManager();
		if (!body || !players)
			return;

		int playerId = players.GetPlayerIdFromControlledEntity(body);
		if (playerId <= 0)
			return;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Race_Title", key, 6.0, LF_ENotificationType.INFO, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells a racer by player id, so a racer without a living body (dead, still loading) hears it too.
	protected static void NotifyOnline(LF_RaceEntrant entrant, string key, string arg)
	{
		int playerId = PlayerIdOf(entrant);
		if (playerId <= 0)
			return;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Race_Title", key, 6.0, LF_ENotificationType.INFO, arg);
	}

	//------------------------------------------------------------------------------------------------
	protected static float NowMs()
	{
		if (!GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		Clock();
		GetGame().GetCallqueue().CallLater(s_Clock.Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		if (GetGame().GetCallqueue() && s_Clock)
			GetGame().GetCallqueue().Remove(s_Clock.Tick);
	}

	//------------------------------------------------------------------------------------------------
	//! Server checkpoint detection: each racer against the next checkpoint's placed radius.
	static void Tick()
	{
		TickAt(NowMs());
	}

	//------------------------------------------------------------------------------------------------
	//! One detection tick as of world time nowMs (a test passes a later time to run the AFK grace and
	//! the settle delay without waiting): checkpoints, then racers without a body too long, then a
	//! race whose settle delay after the first finisher has run out.
	static void TickAt(float nowMs)
	{
		array<string> ids = {};
		foreach (string courseId, LF_RaceInstance race : Races())
		{
			if (race.m_eState == LF_ERaceState.RACING)
				ids.Insert(courseId);
		}

		foreach (string id : ids)
		{
			LF_RaceInstance running = Races().Get(id);
			if (!running)
				continue;

			int timeMs = (int)(nowMs - running.m_fStartMs);
			array<string> uids = {};
			foreach (LF_RaceEntrant entrant : running.m_aEntrants)
				uids.Insert(entrant.m_sUid);

			foreach (string uid : uids)
			{
				LF_RaceInstance still = Races().Get(id);
				if (!still || still != running)
					break;

				LF_RaceEntrant racer = running.Find(uid);
				if (!racer || racer.m_Progress.m_bFinished)
					continue;

				string next = LF_RaceCourse.NextCheckpoint(running.m_Course, racer.m_Progress);
				IEntity body = ResolveBody(racer);
				if (racer.AbsentTooLong(body != null, nowMs, running.m_Course.m_iAfkGraceSeconds))
				{
					Drop(uid, "was without a living body too long", "#LF-Race_Dropped");
					continue;
				}

				if (!body)
				{
					racer.Lose();
					continue;
				}

				if (next.IsEmpty())
					continue;

				vector now = WorldPosition(body);
				vector from = racer.Step(body.GetID(), now);
				if (LF_RaceCheckpointComponent.Crossed(next, from, now))
					PassAt(id, uid, next, timeMs);
			}

			if (Races().Get(id) == running && running.SettleDue(nowMs))
				SettleLate(id, running);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The settle delay after the first finisher ran out: pays the finishers; racers still out lose their
	//! fee to the pot.
	protected static void SettleLate(string courseId, LF_RaceInstance race)
	{
		int stillOut = 0;
		foreach (LF_RaceEntrant entrant : race.m_aEntrants)
		{
			if (entrant.m_Progress.m_bFinished)
				continue;

			stillOut++;
			NotifyOnline(entrant, "#LF-Race_TimeUp", "");
		}

		LF_Debug.Info(FEATURE, string.Format("race '%1' settles %2 s after the first finisher; %3 racers still out forfeit their fee", courseId, race.m_Course.m_iSettleSeconds, stillOut));
		Settle(courseId);
	}

	//------------------------------------------------------------------------------------------------
	//! The serial is never reset, so a lobby or limit timer queued in an earlier world matches nothing.
	static void ResetStatics()
	{
		StopTicking();
		if (s_Clock && GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(s_Clock.LobbyDue);
			GetGame().GetCallqueue().Remove(s_Clock.LimitDue);
		}

		s_mRaces = null;
		s_Clock = null;
		s_Players = null;
		s_iScans = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! A leaver forfeits; a session end refunds every racer still in before the shutdown save.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
			LF_RaceRunner.Forfeit(LF_Utils.GetPlayerUID(playerId));

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameEnd()
	{
		if (Replication.IsServer())
			LF_RaceRunner.AbandonAll("session end");

		// The world-end reset of the runner's statics. Not a LF_StaticResetHooks layer: that modded
		// chain is past the script stack depth (the suite overflows at about 124 layers).
		LF_RaceRunner.ResetStatics();

		super.OnGameEnd();
	}
}
