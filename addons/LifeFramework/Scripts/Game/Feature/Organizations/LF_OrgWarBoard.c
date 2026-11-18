// LF_OrgWarBoard.c - the war notice board (M7 gang war, #2677; events #2666). A board kind
// (LF_BoardProviders, #1945) modelled on the wanted board: the server answers a read with the wars
// that are declared, running, or ended within the last hour; the client paints one line per notice.
// Nothing is replicated and nothing is cached; each read asks again.
//
// Where the notices come from (server only):
//   declared / accepted : the live war table (LF_OrgWars), so a restart that loaded the table shows them
//   ended / lapsed      : the LF_OnOrgWarEnded events this board listens to, kept for an hour (not saved:
//                         a notice that old is not worth a save version)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Order is the board's order: what needs an answer first, then what is running, then what ended.
enum LF_EOrgWarNotice
{
	DECLARED,
	ACCEPTED,
	ENDED
}

//------------------------------------------------------------------------------------------------
//! One line of the board. Names are copied when the notice is made, so an organization that is
//! disbanded later does not blank an ended notice.
class LF_OrgWarNotice : Managed
{
	LF_EOrgWarNotice m_eKind;
	string m_sWarId;
	string m_sDeclarerName;
	string m_sTargetName;
	//! How an ended war closed: LF_OrgWars.REASON_*. Empty for an open war.
	string m_sReason;
	//! Unix second the notice happened: declared at, accepted at, ended at.
	int m_iAt;
	//! Unix second the war needs an answer by (DECLARED) or runs until (ACCEPTED). 0 once ended.
	int m_iUntil;

	//------------------------------------------------------------------------------------------------
	static LF_OrgWarNotice Create(LF_EOrgWarNotice kind, string warId, string declarerName, string targetName, string reason, int at, int until)
	{
		LF_OrgWarNotice notice = new LF_OrgWarNotice();
		notice.m_eKind = kind;
		notice.m_sWarId = warId;
		notice.m_sDeclarerName = declarerName;
		notice.m_sTargetName = targetName;
		notice.m_sReason = reason;
		notice.m_iAt = at;
		notice.m_iUntil = until;
		return notice;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgWarBoard
{
	static const string FEATURE = "OrgWarBoard";
	//! The board kind this board registers under (LF_BoardProviders).
	static const string KIND = "orgwars";
	//! The read action class a reader must stand at (LF_ActionSite).
	static const string READ_ACTION = "LF_OrgWarBoardReadAction";
	//! How long an ended war stays on the board (seconds).
	static const int ENDED_KEEP_SECONDS = 3600;	// lf-fixed: the board's rule, one hour (#2677)
	//! Ended notices kept in memory at most; the oldest goes first.
	static const int ENDED_MAX = 16;	// lf-fixed: memory bound, far above a day's wars
	//! Texts per notice in a board answer: declarer, target, reason.
	static const int TEXT_STRIDE = 3;	// lf-fixed: the answer format
	//! Numbers per notice in a board answer: kind, seconds (left to answer or to run; ago for an ended war).
	static const int NUMBER_STRIDE = 2;	// lf-fixed: the answer format

	protected static ref array<ref LF_OrgWarNotice> s_aEnded;
	protected static ref LF_OrgWarBoardSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			Unsubscribe();

		s_aEnded = null;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_OrgWarNotice> Ended()
	{
		if (!s_aEnded)
			s_aEnded = new array<ref LF_OrgWarNotice>();

		return s_aEnded;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Unsubscribe()
	{
		LF_EventBus.Unsubscribe(LF_OnOrgWarDeclared, s_Subscriber);
		LF_EventBus.Unsubscribe(LF_OnOrgWarAccepted, s_Subscriber);
		LF_EventBus.Unsubscribe(LF_OnOrgWarEnded, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen to the war events (idempotent, also after a bus reset). Called when the game
	//! mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber)
			Unsubscribe();

		s_Subscriber = new LF_OrgWarBoardSubscriber();
		bool ok = LF_EventBus.Subscribe(LF_OnOrgWarDeclared, s_Subscriber);
		ok = LF_EventBus.Subscribe(LF_OnOrgWarAccepted, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnOrgWarEnded, s_Subscriber) && ok;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! The name a notice prints for an organization: its registered name, or the id once it is gone.
	protected static string NameOf(string orgId)
	{
		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (org)
			return org.GetName();

		return orgId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one war closed. Keeps its notice for ENDED_KEEP_SECONDS.
	static void OnEnded(notnull LF_OnOrgWarEnded ended)
	{
		array<ref LF_OrgWarNotice> list = Ended();
		list.Insert(LF_OrgWarNotice.Create(LF_EOrgWarNotice.ENDED, ended.m_sWarId, NameOf(ended.m_sDeclarerOrgId), NameOf(ended.m_sTargetOrgId), ended.m_sReason, ended.m_iEndedAt, 0));
		while (list.Count() > ENDED_MAX)
		{
			list.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("notice posted: %1 ended (%2)", ended.m_sWarId, ended.m_sReason));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every notice the board holds at unix second `now`, in board order. Closes the wars the
	//! clock has passed first, so an unanswered declaration does not stay up past its window.
	static void Collect(int now, notnull array<ref LF_OrgWarNotice> outNotices)
	{
		outNotices.Clear();
		LF_OrgWars.Tick(now);
		foreach (LF_OrgWarRecord war : LF_OrgWars.ExportAll())
		{
			if (!war)
				continue;

			if (war.m_eState == LF_EOrgWarState.DECLARED)
				outNotices.Insert(LF_OrgWarNotice.Create(LF_EOrgWarNotice.DECLARED, war.m_sWarId, NameOf(war.m_sDeclarerOrgId), NameOf(war.m_sTargetOrgId), string.Empty, war.m_iDeclaredAt, war.m_iDeclaredAt + war.m_iAnswerWindowSec));
			else if (war.m_eState == LF_EOrgWarState.ACCEPTED)
				outNotices.Insert(LF_OrgWarNotice.Create(LF_EOrgWarNotice.ACCEPTED, war.m_sWarId, NameOf(war.m_sDeclarerOrgId), NameOf(war.m_sTargetOrgId), string.Empty, war.m_iAcceptedAt, war.m_iEndsAt));
		}

		foreach (LF_OrgWarNotice ended : Ended())
		{
			outNotices.Insert(ended);
		}

		Arrange(outNotices, now);
	}

	//------------------------------------------------------------------------------------------------
	//! The board's rules on a list of notices, in place: an ended notice older than ENDED_KEEP_SECONDS
	//! drops; the rest sit declared first, then accepted, then ended, newest first inside each.
	static void Arrange(notnull array<ref LF_OrgWarNotice> notices, int now)
	{
		for (int i = notices.Count() - 1; i >= 0; i--)
		{
			LF_OrgWarNotice notice = notices[i];
			if (!notice || (notice.m_eKind == LF_EOrgWarNotice.ENDED && now - notice.m_iAt >= ENDED_KEEP_SECONDS))
				notices.RemoveOrdered(i);
		}

		// Insertion sort: a handful of notices, and it keeps equal ones in the order they came.
		for (int a = 1; a < notices.Count(); a++)
		{
			LF_OrgWarNotice moving = notices[a];
			int b = a - 1;
			while (b >= 0 && Before(moving, notices[b]))
			{
				notices[b + 1] = notices[b];
				b--;
			}

			notices[b + 1] = moving;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Before(notnull LF_OrgWarNotice first, notnull LF_OrgWarNotice second)
	{
		if (first.m_eKind != second.m_eKind)
			return first.m_eKind < second.m_eKind;

		return first.m_iAt > second.m_iAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the answer for a read at unix second `now`, packed flat for the RPC.
	static LF_BoardAnswer BuildAnswer(int now)
	{
		array<ref LF_OrgWarNotice> notices = {};
		Collect(now, notices);
		LF_BoardAnswer answer = Pack(notices, now);
		LF_Debug.Info(FEATURE, string.Format("board read: %1 notice(s)", notices.Count()));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Notices as one flat answer: per notice TEXT_STRIDE texts and NUMBER_STRIDE numbers. Times cross
	//! as seconds from `now`, so the client needs no clock that agrees with the server's.
	static LF_BoardAnswer Pack(notnull array<ref LF_OrgWarNotice> notices, int now)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		foreach (LF_OrgWarNotice notice : notices)
		{
			answer.m_aTexts.Insert(notice.m_sDeclarerName);
			answer.m_aTexts.Insert(notice.m_sTargetName);
			answer.m_aTexts.Insert(notice.m_sReason);
			answer.m_aNumbers.Insert(notice.m_eKind);
			if (notice.m_eKind == LF_EOrgWarNotice.ENDED)
				answer.m_aNumbers.Insert(NonNegative(now - notice.m_iAt));
			else
				answer.m_aNumbers.Insert(NonNegative(notice.m_iUntil - now));
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Not Math.Max: it works in floats, which lose whole seconds at unix-time size.
	protected static int NonNegative(int value)
	{
		if (value < 0)
			return 0;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! The kind a notice number names; DECLARED for a number out of range, so a bad answer still paints.
	static LF_EOrgWarNotice KindOf(int number)
	{
		if (number == LF_EOrgWarNotice.ACCEPTED)
			return LF_EOrgWarNotice.ACCEPTED;

		if (number == LF_EOrgWarNotice.ENDED)
			return LF_EOrgWarNotice.ENDED;

		return LF_EOrgWarNotice.DECLARED;
	}

	//------------------------------------------------------------------------------------------------
	//! The string key a notice line is written from; its arguments are declarer, target, time.
	static string LineKey(LF_EOrgWarNotice kind, string reason)
	{
		if (kind == LF_EOrgWarNotice.DECLARED)
			return "#LF-OrgWarBoard_Declared";

		if (kind == LF_EOrgWarNotice.ACCEPTED)
			return "#LF-OrgWarBoard_Accepted";

		if (reason == LF_OrgWars.REASON_LAPSED)
			return "#LF-OrgWarBoard_Lapsed";

		if (reason == LF_OrgWars.REASON_FORFEIT)
			return "#LF-OrgWarBoard_Forfeit";

		return "#LF-OrgWarBoard_Ended";
	}

	//------------------------------------------------------------------------------------------------
	//! Whole minutes in a span, rounded up and never below one: "0 min left" reads as already over.
	static int SpanMinutes(int seconds)
	{
		if (seconds <= 0)
			return 1;

		return (seconds + 59) / 60;
	}

	//------------------------------------------------------------------------------------------------
	//! A span as the card prints it: "25 min", or "2 h 5 min" from an hour up.
	static string SpanText(int seconds)
	{
		int minutes = SpanMinutes(seconds);
		if (minutes < 60)
			return WidgetManager.Translate("#LF-OrgWarBoard_Minutes", minutes);

		return WidgetManager.Translate("#LF-OrgWarBoard_HoursMinutes", minutes / 60, minutes % 60);
	}

	//------------------------------------------------------------------------------------------------
	//! One notice as a card line.
	static string LineText(LF_EOrgWarNotice kind, string declarer, string target, string reason, int seconds)
	{
		return WidgetManager.Translate(LineKey(kind, reason), declarer, target, SpanText(seconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the card lines. A malformed answer (texts and numbers out of step) paints the
	//! rows that line up and logs the rest.
	static array<ref LF_UIPart> BuildParts(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-OrgWarBoard_Title")));

		int rows = answer.m_aNumbers.Count() / NUMBER_STRIDE;
		int textRows = answer.m_aTexts.Count() / TEXT_STRIDE;
		if (answer.m_aNumbers.Count() != rows * NUMBER_STRIDE || answer.m_aTexts.Count() != textRows * TEXT_STRIDE || rows != textRows)
		{
			LF_Debug.Error(FEATURE, string.Format("war board answer out of step: %1 texts, %2 numbers", answer.m_aTexts.Count(), answer.m_aNumbers.Count()));
			rows = Math.Min(rows, textRows);
		}

		if (rows < 1)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-OrgWarBoard_None")));

		for (int i = 0; i < rows; i++)
		{
			LF_EOrgWarNotice kind = KindOf(answer.m_aNumbers[i * NUMBER_STRIDE]);
			string line = LineText(kind, answer.m_aTexts[i * TEXT_STRIDE], answer.m_aTexts[i * TEXT_STRIDE + 1], answer.m_aTexts[i * TEXT_STRIDE + 2], answer.m_aNumbers[i * NUMBER_STRIDE + 1]);
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Notice" + i, line));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: the board hears every war transition. An ended war is kept for the hour; the other
//! two are read from the war table on each read, so they only log here.
class LF_OrgWarBoardSubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "OrgWarBoard";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrgWarEnded ended = LF_OnOrgWarEnded.Cast(busEvent);
		if (ended)
		{
			LF_OrgWarBoard.OnEnded(ended);
			return true;
		}

		LF_OnOrgWarDeclared declared = LF_OnOrgWarDeclared.Cast(busEvent);
		if (declared)
		{
			LF_Debug.Info(LF_OrgWarBoard.FEATURE, string.Format("notice posted: %1 declared", declared.m_sWarId));
			return true;
		}

		LF_OnOrgWarAccepted accepted = LF_OnOrgWarAccepted.Cast(busEvent);
		if (accepted)
		{
			LF_Debug.Info(LF_OrgWarBoard.FEATURE, string.Format("notice posted: %1 accepted", accepted.m_sWarId));
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWarBoard's statics (LF_StaticReset finds it by reflection).
class LF_OrgWarBoard_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgWarBoard";
	}

	override void Apply()
	{
		LF_OrgWarBoard.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgWarBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_OrgWarBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_OrgWarBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_OrgWarBoard.BuildAnswer(System.GetUnixTime());
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_OrgWarBoard.BuildParts(answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_OrgWarBoard.FEATURE, string.Format("war board card: %1 notice(s)", answer.m_aNumbers.Count() / LF_OrgWarBoard.NUMBER_STRIDE));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_OrgWarBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the war notices" on the board. Local to the reader: it asks the server, which answers if the
//! reader stands at the board.
class LF_OrgWarBoardReadAction : ScriptedUserAction
{
	//! The Organizations system is off: no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_OrgRules.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-OrgWarBoard_Read";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_OrgRules.SYSTEM, "LF_OrgWarBoardReadAction"))
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
			LF_BoardProviders.ServerRead(remote, LF_OrgWarBoard.KIND);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_OrgWarBoard.FEATURE, "war board read not sent: no SCR_PlayerController");
			return;
		}

		// The card closes when the reader walks off, like the wanted board's.
		LF_WantedBoard.SetPendingBoard(pOwnerEntity.GetID());
		controller.LF_AskBoard(LF_OrgWarBoard.KIND);
	}
}
