// LF_RaceRunner.c - one race per course, server only (#1923). A racer pays the entry fee from
// carried cash into the pot at the start sign; the lobby closes after the course's lobby seconds;
// too few entrants refunds everyone; the server watches checkpoint crossings and pays the pot by
// the course split when every racer still in has finished (or the time limit runs out). A leaver
// forfeits the fee. The pot is session state, never persisted: a session end mid-race refunds
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
//! One racer in one race. The body is re-resolved by uid at every use; the stored id is only the
//! fallback for a body that has no player (a test fixture).
class LF_RaceEntrant
{
	string m_sUid;
	EntityID m_BodyId = EntityID.INVALID;
	int m_iFee;
	ref LF_RaceProgress m_Progress = new LF_RaceProgress();
	//! World position at the last detection tick, so a fast car is checked along its path.
	vector m_vLast;
	bool m_bHasLast;
	//! The body the last position belongs to. A new body (respawn, character switch) starts a new path.
	EntityID m_LastBodyId = EntityID.INVALID;

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
	ref array<ref LF_RaceEntrant> m_aEntrants = new array<ref LF_RaceEntrant>();
	//! Racers who left before finishing. Their fees stay in the pot; only a session end pays them back.
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

	protected static ref map<string, ref LF_RaceInstance> s_mRaces;
	protected static ref LF_RaceClock s_Clock;
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
	//! A racer leaves (disconnect): dropped from the race, the fee stays in the pot. Nothing is paid
	//! out, so a leaving body's saved inventory cannot carry a duplicate.
	//! \return true when the racer was in a race.
	static bool Forfeit(string uid)
	{
		string courseId = CourseOf(uid);
		if (courseId.IsEmpty())
			return false;

		LF_RaceInstance race = Races().Get(courseId);
		LF_RaceEntrant entrant = race.Find(uid);
		if (entrant.m_Progress.m_bFinished)
		{
			// A finisher keeps the place; the prize goes to the bank when the race settles.
			LF_Debug.Info(FEATURE, string.Format("race '%1' finisher %2 left; the place stands", courseId, uid));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("race '%1' %2 left and forfeits the %3 fee; pot %4", courseId, uid, entrant.m_iFee, race.m_iPot));
		race.m_aLeavers.Insert(entrant);
		race.m_aEntrants.RemoveItem(entrant);

		if (race.m_eState == LF_ERaceState.RACING)
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
			SettleIfDone(courseId);

		return verdict;
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
		if (Races().IsEmpty())
			StopTicking();

		return race;
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

		int rest = amount - added;
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
		PlayerManager players = GetGame().GetPlayerManager();
		if (players)
		{
			array<int> ids = {};
			players.GetPlayers(ids);
			foreach (int playerId : ids)
			{
				if (LF_Utils.GetPlayerUID(playerId) != entrant.m_sUid)
					continue;

				// Online: the controlled body if it lives, never the corpse the racer entered with.
				IEntity controlled = players.GetPlayerControlledEntity(playerId);
				if (IsAlive(controlled))
					return controlled;

				return null;
			}
		}

		if (entrant.m_BodyId == EntityID.INVALID || !GetGame().GetWorld())
			return null;

		IEntity entered = GetGame().GetWorld().FindEntityByID(entrant.m_BodyId);
		if (!IsAlive(entered))
			return null;

		// A body some other player now controls is not this racer's.
		if (players && players.GetPlayerIdFromControlledEntity(entered) > 0)
			return null;

		return entered;
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

			int timeMs = (int)(NowMs() - running.m_fStartMs);
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
		}
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
