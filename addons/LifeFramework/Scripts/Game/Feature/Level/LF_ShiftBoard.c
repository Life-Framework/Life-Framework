//------------------------------------------------------------------------------------------------
//! The weekly shift board (WK-W22, #4712, chapter #4170): each work site keeps the week's top five
//! workers by the trade XP they earned there. XP comes from outcomes only: the board listens to
//! LF_OnJobXpAwarded (whatever LF_JobLevels granted after its caps), never to time on site.
//!
//! A site is the nearest workplace (LF_Places kind LF_Workplace.PLACE_KIND) within SITE_RADIUS_M of
//! the worker when the XP lands; XP earned away from any workplace counts on no board. The week
//! turns on the server clock (unix time) every Monday at 06:00 UTC and empties every board.
//!
//! The board kind `shiftboard` (LF_BoardProviders) prints the reader's site: "1. Ada Newcomer,
//! Mining 7, 1,240 XP". Saved in the "shiftboard" section (LF_ShiftBoardSavePayload).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! One worker's XP in one trade at one site this week. Also the save row.
class LF_ShiftBoardRow
{
	string m_sSite;
	string m_sPlayerUid;
	string m_sName;
	string m_sJob;
	int m_iXp;

	//------------------------------------------------------------------------------------------------
	static LF_ShiftBoardRow Create(string site, string playerUid, string name, string job, int xp)
	{
		LF_ShiftBoardRow row = new LF_ShiftBoardRow();
		row.m_sSite = site;
		row.m_sPlayerUid = playerUid;
		row.m_sName = name;
		row.m_sJob = job;
		row.m_iXp = xp;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ShiftBoard : LF_EventSubscriber
{
	static const string FEATURE = "ShiftBoard";
	//! The board kind (LF_BoardProviders).
	static const string KIND = "shiftboard";
	//! The read action class a reader must stand at (LF_ActionSite).
	static const string READ_ACTION = "LF_ShiftBoardReadAction";
	//! Rows one board prints (the card: "the week's top five").
	static const int TOP = 5;	// lf-fixed: the card's top five, not tuning
	//! How far from a workplace XP still counts on its board, metres.
	static const float SITE_RADIUS_M = 150;	// lf-fixed: a work site's yard, not tuning
	static const int WEEK_SECONDS = 604800;	// lf-fixed: the length of a week
	//! Unix time of the first Monday 06:00 UTC (1970-01-05 06:00); weeks count from it.
	static const int WEEK_ANCHOR = 367200;	// lf-fixed: calendar arithmetic
	//! Most rows the store keeps; a full store drops new workers (logged) rather than grow the save.
	static const int MAX_ROWS = 4000;	// lf-fixed: save size guard

	protected static ref LF_ShiftBoard s_Subscriber;
	//! site|uid|job -> row.
	protected static ref map<string, ref LF_ShiftBoardRow> s_mRows;
	//! The week the rows belong to (WeekOf); 0 or -1 before the first award (week 0 is 1970, never live).
	protected static int s_iWeek;
	//! Tests: a fixed server clock; 0 uses System.GetUnixTime().
	protected static int s_iNowForTests;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnJobXpAwarded, s_Subscriber);

		s_Subscriber = null;
		s_mRows = null;
		s_iWeek = -1;
		s_iNowForTests = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen to XP awards. Called at game start.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_ShiftBoard();

		if (LF_EventBus.IsSubscribed(LF_OnJobXpAwarded, s_Subscriber))
			return true;

		return LF_EventBus.Subscribe(LF_OnJobXpAwarded, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobXpAwarded award = LF_OnJobXpAwarded.Cast(busEvent);
		if (!award)
			return false;

		OnAwarded(award.m_sPlayerUid, award.m_sJob, award.m_iAmount, Now());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int Now()
	{
		if (s_iNowForTests > 0)
			return s_iNowForTests;

		return System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The week number of a unix time: weeks start Monday 06:00 UTC.
	static int WeekOf(int unixSeconds)
	{
		if (unixSeconds < WEEK_ANCHOR)
			return 0;

		return (unixSeconds - WEEK_ANCHOR) / WEEK_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The week the stored rows belong to; -1 when none was set.
	static int CurrentWeek()
	{
		return s_iWeek;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties every board when the server clock is in another week than the rows.
	//! \return True when the week turned.
	static bool Roll(int now)
	{
		int week = WeekOf(now);
		if (week == s_iWeek)
			return false;

		int dropped = 0;
		if (s_mRows)
		{
			dropped = s_mRows.Count();
			s_mRows.Clear();
		}

		if (s_iWeek >= 0)
			LF_Debug.Info(FEATURE, string.Format("the week turned (%1 -> %2): %3 row(s) cleared", s_iWeek, week, dropped));

		s_iWeek = week;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an award landed. Finds the worker's site and records it there.
	//! \return The site it counted at; empty when the worker stood at no workplace.
	static string OnAwarded(string uid, string job, int amount, int now)
	{
		if (uid.IsEmpty() || job.IsEmpty() || amount <= 0)
			return string.Empty;

		IEntity body = LF_QuestProgressSignals.BodyOfUid(uid);
		if (!body)
			return string.Empty;

		string site = SiteAt(body.GetOrigin());
		if (site.IsEmpty())
			return string.Empty;

		Record(site, uid, NameOf(uid), job, amount, now);
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The place id of the nearest workplace within SITE_RADIUS_M; empty when none.
	static string SiteAt(vector origin)
	{
		array<IEntity> workplaces = {};
		LF_Places.FindAll(LF_Workplace.PLACE_KIND, workplaces);
		string best;
		float bestDistance = SITE_RADIUS_M * SITE_RADIUS_M;
		foreach (IEntity entity : workplaces)
		{
			if (!entity)
				continue;

			LF_WorkplaceComponent workplace = LF_WorkplaceComponent.Cast(entity.FindComponent(LF_WorkplaceComponent));
			if (!workplace)
				continue;

			float distance = vector.DistanceSq(origin, entity.GetOrigin());
			if (distance > bestDistance)
				continue;

			string id = workplace.GetPlaceId();
			if (id.IsEmpty())
				continue;

			best = id;
			bestDistance = distance;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The worker's active character name, else the player name; empty when the server knows neither.
	protected static string NameOf(string uid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
		{
			LF_PlayerAccount account = accounts.GetFromCache(uid);
			if (account)
			{
				LF_PlayerCharacter character = account.GetActiveCharacter();
				if (character && !character.GetFullName().IsEmpty())
					return character.GetFullName();
			}
		}

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return string.Empty;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			if (LF_Utils.GetPlayerUID(id) == uid)
				return players.GetPlayerName(id);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: adds XP a worker earned in a trade at a site this week (rolls the week first).
	//! \return False when refused (missing field, non-positive XP, full store).
	static bool Record(string site, string uid, string name, string job, int xp, int now)
	{
		if (site.IsEmpty() || uid.IsEmpty() || job.IsEmpty() || xp <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("shift XP refused: site '%1' worker '%2' trade '%3' xp %4", site, uid, job, xp));
			return false;
		}

		Roll(now);
		if (!s_mRows)
			s_mRows = new map<string, ref LF_ShiftBoardRow>();

		string key = Key(site, uid, job);
		LF_ShiftBoardRow row;
		if (!s_mRows.Find(key, row) || !row)
		{
			if (s_mRows.Count() >= MAX_ROWS)
			{
				LF_Debug.Warn(FEATURE, string.Format("shift XP not recorded for %1 at %2: the store holds %3 rows", uid, site, MAX_ROWS));
				return false;
			}

			row = LF_ShiftBoardRow.Create(site, uid, name, job, 0);
			s_mRows.Set(key, row);
		}

		if (!name.IsEmpty())
			row.m_sName = name;

		row.m_iXp = LF_Utils.MinInt(row.m_iXp + xp, 2000000000);	// lf-fixed: int overflow guard
		LF_Debug.Info(FEATURE, string.Format("+%1 %2 xp for %3 at %4 this week (now %5)", xp, job, uid, site, row.m_iXp));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string site, string uid, string job)
	{
		return site + "|" + uid + "|" + job;
	}

	//------------------------------------------------------------------------------------------------
	//! The week's top workers at a site, highest XP first (ties by name). Each row is one worker: the
	//! XP summed over their trades there, and the trade they earned most in.
	static array<ref LF_ShiftBoardRow> Top(string site, int now, int count = TOP)
	{
		Roll(now);
		array<ref LF_ShiftBoardRow> workers = {};
		if (!s_mRows)
			return workers;

		map<string, ref LF_ShiftBoardRow> byWorker = new map<string, ref LF_ShiftBoardRow>();
		map<string, int> bestTrade = new map<string, int>();
		foreach (string key, LF_ShiftBoardRow row : s_mRows)
		{
			if (!row || row.m_sSite != site)
				continue;

			LF_ShiftBoardRow worker;
			if (!byWorker.Find(row.m_sPlayerUid, worker))
			{
				worker = LF_ShiftBoardRow.Create(site, row.m_sPlayerUid, row.m_sName, row.m_sJob, 0);
				byWorker.Set(row.m_sPlayerUid, worker);
				bestTrade.Set(row.m_sPlayerUid, row.m_iXp);
			}
			else if (row.m_iXp > bestTrade.Get(row.m_sPlayerUid))
			{
				worker.m_sJob = row.m_sJob;
				bestTrade.Set(row.m_sPlayerUid, row.m_iXp);
			}

			if (!row.m_sName.IsEmpty())
				worker.m_sName = row.m_sName;

			worker.m_iXp = LF_Utils.MinInt(worker.m_iXp + row.m_iXp, 2000000000);	// lf-fixed: int overflow guard
		}

		foreach (string uid, LF_ShiftBoardRow worker : byWorker)
		{
			workers.Insert(worker);
		}

		Sort(workers);
		while (workers.Count() > count && workers.Count() > 0)
		{
			workers.Remove(workers.Count() - 1);
		}

		return workers;
	}

	//------------------------------------------------------------------------------------------------
	//! Insertion sort, highest XP first, then by name, then by uid (a stable order across reads).
	protected static void Sort(notnull array<ref LF_ShiftBoardRow> rows)
	{
		for (int i = 1; i < rows.Count(); i++)
		{
			LF_ShiftBoardRow row = rows[i];
			int at = i - 1;
			while (at >= 0 && IsBefore(row, rows[at]))
			{
				rows[at + 1] = rows[at];
				at--;
			}

			rows[at + 1] = row;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsBefore(LF_ShiftBoardRow a, LF_ShiftBoardRow b)
	{
		if (a.m_iXp != b.m_iXp)
			return a.m_iXp > b.m_iXp;

		int byName = a.m_sName.Compare(b.m_sName, false);
		if (byName != 0)
			return byName < 0;

		return a.m_sPlayerUid.Compare(b.m_sPlayerUid) < 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every stored row, for the save.
	static array<ref LF_ShiftBoardRow> ExportAll()
	{
		array<ref LF_ShiftBoardRow> rows = {};
		if (!s_mRows)
			return rows;

		foreach (string key, LF_ShiftBoardRow row : s_mRows)
		{
			if (row)
				rows.Insert(LF_ShiftBoardRow.Create(row.m_sSite, row.m_sPlayerUid, row.m_sName, row.m_sJob, row.m_iXp));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store with saved rows of a week (a load; null and -1 empty it). A row with a
	//! missing field or no XP is skipped.
	//! \return Rows kept.
	static int ApplyAll(array<ref LF_ShiftBoardRow> rows, int week)
	{
		s_mRows = new map<string, ref LF_ShiftBoardRow>();
		s_iWeek = week;
		if (!rows)
			return 0;

		foreach (LF_ShiftBoardRow row : rows)
		{
			if (!row || row.m_sSite.IsEmpty() || row.m_sPlayerUid.IsEmpty() || row.m_sJob.IsEmpty() || row.m_iXp <= 0)
				continue;

			if (s_mRows.Count() >= MAX_ROWS)
				break;

			string key = Key(row.m_sSite, row.m_sPlayerUid, row.m_sJob);
			LF_ShiftBoardRow kept;
			if (s_mRows.Find(key, kept) && kept)
			{
				kept.m_iXp = LF_Utils.MinInt(kept.m_iXp + row.m_iXp, 2000000000);	// lf-fixed: int overflow guard
				continue;
			}

			s_mRows.Set(key, LF_ShiftBoardRow.Create(row.m_sSite, row.m_sPlayerUid, row.m_sName, row.m_sJob, row.m_iXp));
		}

		return s_mRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Rows held this week, every site.
	static int Count()
	{
		if (!s_mRows)
			return 0;

		return s_mRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the board answer for a site. Texts: the site id, then name and trade per worker;
	//! numbers: level and XP per worker.
	static LF_BoardAnswer BuildAnswerFor(string site, int now)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		answer.m_aTexts.Insert(site);
		if (site.IsEmpty())
			return answer;

		array<ref LF_ShiftBoardRow> top = Top(site, now);
		foreach (LF_ShiftBoardRow row : top)
		{
			answer.m_aTexts.Insert(row.m_sName);
			answer.m_aTexts.Insert(row.m_sJob);
			answer.m_aNumbers.Insert(LF_JobLevels.LevelOf(row.m_sPlayerUid, row.m_sJob));
			answer.m_aNumbers.Insert(row.m_iXp);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the answer for the site the reader stands at.
	static LF_BoardAnswer BuildAnswer(int playerId)
	{
		string site;
		PlayerManager players = GetGame().GetPlayerManager();
		IEntity reader;
		if (players)
			reader = players.GetPlayerControlledEntity(playerId);

		if (reader)
			site = SiteAt(reader.GetOrigin());

		LF_BoardAnswer answer = BuildAnswerFor(site, Now());
		LF_Debug.Info(FEATURE, string.Format("shift board read by player %1 at '%2': %3 worker(s)", playerId, site, answer.m_aNumbers.Count() / 2));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! "1,240".
	static string GroupThousands(int value)
	{
		string sign;
		if (value < 0)
		{
			sign = "-";
			value = -value;
		}

		string digits = value.ToString();
		string grouped;
		int length = digits.Length();
		for (int i = 0; i < length; i++)
		{
			if (i > 0 && (length - i) % 3 == 0)
				grouped += ",";

			grouped += digits.Get(i);
		}

		return sign + grouped;
	}

	//------------------------------------------------------------------------------------------------
	//! "1. Ada Newcomer, Mining 7, 1,240 XP".
	static string RowText(int place, string name, string job, int level, int xp)
	{
		if (name.IsEmpty())
			name = WidgetManager.Translate("#LF-ShiftBoard_Unnamed");

		return WidgetManager.Translate("#LF-ShiftBoard_Row", place.ToString(), name, LF_PlayerMenuSheet.SkillName(job), level.ToString(), GroupThousands(xp));
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the lines of the board, so a test reads what a player would see.
	static array<ref LF_UIPart> BuildParts(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-ShiftBoard_Title")));
		int rows = answer.m_aNumbers.Count() / 2;
		if (answer.m_aTexts.Count() != 1 + rows * 2 || answer.m_aNumbers.Count() % 2 != 0)
		{
			LF_Debug.Error(FEATURE, string.Format("shift board answer out of step: %1 texts, %2 numbers", answer.m_aTexts.Count(), answer.m_aNumbers.Count()));
			rows = LF_Utils.MinInt(rows, (answer.m_aTexts.Count() - 1) / 2);
		}

		if (answer.m_aTexts.IsEmpty() || answer.m_aTexts[0].IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "NoSite", WidgetManager.Translate("#LF-ShiftBoard_NoSite")));
		else if (rows < 1)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-ShiftBoard_None")));

		for (int i = 0; i < rows; i++)
		{
			string text = RowText(i + 1, answer.m_aTexts[1 + i * 2], answer.m_aTexts[2 + i * 2], answer.m_aNumbers[i * 2], answer.m_aNumbers[i * 2 + 1]);
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Worker" + i, text));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ShiftBoard's statics (LF_StaticReset finds it by reflection).
class LF_ShiftBoard_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ShiftBoard";
	}

	override void Apply()
	{
		LF_ShiftBoard.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_ShiftBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_ShiftBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_ShiftBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_ShiftBoard.BuildAnswer(playerId);
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_ShiftBoard.BuildParts(answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_ShiftBoard.FEATURE, string.Format("shift board card: %1 worker(s)", answer.m_aNumbers.Count() / 2));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_ShiftBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! Subscribes at game start, without editing LF_GameModeRoleplay.c.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_ShiftBoard.EnsureSubscribed();
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the shift board" on a work site's printed board. Local to the reader: it asks the server,
//! which answers with the week's list for the site the reader stands at.
class LF_ShiftBoardReadAction : ScriptedUserAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_JobLevels.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-ShiftBoard_Read";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_JobLevels.SYSTEM, "LF_ShiftBoardReadAction"))
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		// Performed on the server for a remote reader (a server-driven use, as the agent harness does).
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
			SCR_PlayerController remote = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			LF_BoardProviders.ServerRead(remote, LF_ShiftBoard.KIND);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_ShiftBoard.FEATURE, "shift board read not sent: no SCR_PlayerController");
			return;
		}

		controller.LF_AskBoard(LF_ShiftBoard.KIND);
	}
}
