// LF_NewspaperEdition.c: the weekly front page builder (WO-W2.2, decision N31). Server only.
//
// The server keeps a log of this week's public facts, fed by the event bus: arrests (LF_OnArrested),
// rewards and new businesses (LF_OnMoneyMoved with a reason a section lists), market prices
// (LF_OnMarketPriceMoved) and crimes reported (LF_OnCrimeCommitted, printed with no name). After each
// fact it rebuilds the front page from LF_NewspaperSections.conf and publishes it with
// LF_Newspaper.SetCurrent, then sends the words to every client (the game mode's RpcDo_NewspaperEdition).
//
// PUBLIC FACTS ONLY. The log never subscribes to a wanted report, a suspect, a witness, a fence sale,
// a hideout or the crime record: an open case and a hidden record never enter it. Build refuses any
// fact whose kind is not printable even if one does, and prints a name only for arrests, rewards and
// openings. A crime fact never stores who did it. The week is unix time / 7 days; a restart forgets
// the log until the save section (WO-W2.3) keeps it.

//------------------------------------------------------------------------------------------------
//! One public fact of the week. Plain values only.
class LF_NewspaperFact
{
	LF_ENewspaperFact m_eKind;
	//! The character name printed, already resolved on the server. Empty: printed with no name.
	string m_sName;
	//! The detail printed (an offence key, a good's name, a money reason).
	string m_sDetail;
	//! What the fact groups by: the character uid, the good, the offence. Never printed.
	string m_sKey;
	int m_iAmount;
	int m_iAmount2;
	int m_iUnixTime;
}

//------------------------------------------------------------------------------------------------
class LF_NewspaperEditionSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		return LF_NewspaperEdition.Capture(busEvent);
	}
}

//------------------------------------------------------------------------------------------------
class LF_NewspaperEdition
{
	static const int WEEK_SECONDS = 604800; // lf-fixed: the Courier is a weekly
	//! The log keeps at most this many facts; the oldest go first.
	static const int MAX_FACTS = 400; // lf-fixed: a week of a busy server, a few KB
	static const string KEY_DATELINE = "#LF-NewspaperEdition_Dateline";
	static const string KEY_SOMEONE = "#LF-NewspaperEdition_Someone";

	protected static ref array<ref LF_NewspaperFact> s_aFacts;
	protected static ref LF_NewspaperEditionSubscriber s_Subscriber;
	protected static int s_iWeek = -1;
	protected static bool s_bPublishQueued;
	static const int PUBLISH_DELAY_MS = 30000; // lf-fixed: a fresh page every half minute at most
	//! Tests freeze the clock; 0 reads System.GetUnixTime.
	protected static int s_iClockOverride;

	//------------------------------------------------------------------------------------------------
	static int Now()
	{
		if (s_iClockOverride > 0)
			return s_iClockOverride;

		return System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: pin the clock (0 lets it run).
	static void SetClock(int unixTime)
	{
		s_iClockOverride = unixTime;
	}

	//------------------------------------------------------------------------------------------------
	//! The week a unix time falls in.
	static int WeekOf(int unixTime)
	{
		return unixTime / WEEK_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_NewspaperFact> Facts()
	{
		if (!s_aFacts)
			s_aFacts = new array<ref LF_NewspaperFact>();

		return s_aFacts;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes the log to its public sources once. Called when the game starts.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnArrested) > 0)
			return true;

		s_Subscriber = new LF_NewspaperEditionSubscriber();
		bool ok = LF_EventBus.Subscribe(LF_OnArrested, s_Subscriber);
		ok = LF_EventBus.Subscribe(LF_OnMoneyMoved, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnMarketPriceMoved, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnCrimeCommitted, s_Subscriber) && ok;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Only these kinds are ever printed.
	static bool IsPrintable(LF_ENewspaperFact kind)
	{
		return kind == LF_ENewspaperFact.ARREST || kind == LF_ENewspaperFact.REWARD || kind == LF_ENewspaperFact.OPENING
			|| kind == LF_ENewspaperFact.MARKET || kind == LF_ENewspaperFact.CRIME;
	}

	//------------------------------------------------------------------------------------------------
	//! Only these kinds may print a character's name (N31: arrests, rewards, openings).
	static bool NameAllowed(LF_ENewspaperFact kind)
	{
		return kind == LF_ENewspaperFact.ARREST || kind == LF_ENewspaperFact.REWARD || kind == LF_ENewspaperFact.OPENING;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns one bus event into a fact, when it is a public one. \return False on a broken event.
	static bool Capture(LF_Event busEvent)
	{
		LF_NewspaperFact fact = FactFor(busEvent, LF_NewspaperSectionsConfig.Get());
		if (!fact)
			return true;

		Note(fact);
		SchedulePublish();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One rebuild and one broadcast per PUBLISH_DELAY_MS however many facts came in (a run of market
	//! sales would otherwise send the page to every client per sale).
	static void SchedulePublish()
	{
		if (s_bPublishQueued)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
		{
			Publish();
			return;
		}

		s_bPublishQueued = true;
		queue.CallLater(PublishQueued, PUBLISH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PublishQueued()
	{
		s_bPublishQueued = false;
		Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! The fact a bus event stands for, or null when it is not one the edition prints.
	static LF_NewspaperFact FactFor(LF_Event busEvent, LF_NewspaperSectionsConfig config)
	{
		LF_OnArrested arrested = LF_OnArrested.Cast(busEvent);
		if (arrested)
		{
			if (arrested.m_sPlayerUid.IsEmpty())
				return null;

			return Make(LF_ENewspaperFact.ARREST, arrested.m_sPlayerUid, LF_PlayerInteraction.DisplayName(arrested.m_sPlayerUid), "", LF_Utils.MaxInt(arrested.m_iSentenceSeconds / 60, 1), 0);
		}

		LF_OnCrimeCommitted crime = LF_OnCrimeCommitted.Cast(busEvent);
		if (crime)
		{
			// The offence only: who did it is an open case, so it is never kept.
			if (crime.m_sOffence.IsEmpty())
				return null;

			return Make(LF_ENewspaperFact.CRIME, crime.m_sOffence, "", crime.m_sOffence, 1, 0);
		}

		LF_OnMarketPriceMoved market = LF_OnMarketPriceMoved.Cast(busEvent);
		if (market)
		{
			if (market.m_sGood.IsEmpty() || market.m_iOldPrice == market.m_iNewPrice)
				return null;

			return Make(LF_ENewspaperFact.MARKET, market.m_sGood, "", LF_GatherAction.ItemName(market.m_sGood), market.m_iOldPrice, market.m_iNewPrice);
		}

		LF_OnMoneyMoved money = LF_OnMoneyMoved.Cast(busEvent);
		if (money && config)
		{
			if (money.m_sPlayerUid.IsEmpty() || money.m_iAmount <= 0)
				return null;

			foreach (LF_NewspaperSectionDef section : config.m_aSections)
			{
				if (!section || !section.m_aSources || section.m_aSources.Find(money.m_sSource) < 0)
					continue;

				if (section.m_eKind != LF_ENewspaperFact.REWARD && section.m_eKind != LF_ENewspaperFact.OPENING)
					continue;

				// A reward is money in; an opening is the buyer paying. One side of each move counts.
				if (section.m_eKind == LF_ENewspaperFact.REWARD && !money.m_sAccount.IsEmpty() && !money.m_bCredit)
					continue;

				if (section.m_eKind == LF_ENewspaperFact.OPENING && !money.m_sAccount.IsEmpty() && money.m_bCredit)
					continue;

				return Make(section.m_eKind, money.m_sPlayerUid, LF_PlayerInteraction.DisplayName(money.m_sPlayerUid), money.m_sSource, money.m_iAmount, 0);
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_NewspaperFact Make(LF_ENewspaperFact kind, string key, string name, string detail, int amount, int amount2)
	{
		LF_NewspaperFact fact = new LF_NewspaperFact();
		fact.m_eKind = kind;
		fact.m_sKey = key;
		if (NameAllowed(kind))
			fact.m_sName = name;

		fact.m_sDetail = detail;
		fact.m_iAmount = amount;
		fact.m_iAmount2 = amount2;
		fact.m_iUnixTime = Now();
		return fact;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a fact to the log, dropping last week's facts and the oldest past MAX_FACTS.
	static void Note(LF_NewspaperFact fact)
	{
		if (!fact)
			return;

		array<ref LF_NewspaperFact> facts = Facts();
		int week = WeekOf(Now());
		if (week != s_iWeek)
		{
			for (int i = facts.Count() - 1; i >= 0; i--)
			{
				if (!facts[i] || WeekOf(facts[i].m_iUnixTime) != week)
					facts.Remove(i);
			}

			s_iWeek = week;
		}

		facts.Insert(fact);
		while (facts.Count() > MAX_FACTS)
			facts.RemoveOrdered(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the front page for one week from a list of facts. A section prints one story with a line
	//! per fact (or per person, good or offence, summed), newest groups first, at most m_iMaxLines.
	//! A section with no facts this week is left off; with none at all the page keeps the placeholder.
	static LF_NewspaperIssue Build(array<ref LF_NewspaperFact> facts, int week, LF_NewspaperSectionsConfig config)
	{
		LF_NewspaperIssue issue = new LF_NewspaperIssue();
		issue.m_sMasthead = LF_Newspaper.KEY_MASTHEAD;
		issue.m_sDateline = LF_Terms.Text(KEY_DATELINE, WeekNumber(week).ToString());
		if (!facts || !config || !config.m_aSections)
			return issue;

		foreach (LF_NewspaperSectionDef section : config.m_aSections)
		{
			if (!section || !IsPrintable(section.m_eKind))
				continue;

			string body = SectionBody(facts, week, section);
			if (body.IsEmpty())
				continue;

			issue.AddStory(section.m_sHeadline, body);
		}

		return issue;
	}

	//------------------------------------------------------------------------------------------------
	//! Week of the year printed in the dateline, 1 to 53.
	static int WeekNumber(int week)
	{
		// Unix week 0 began on a Thursday, 1 Jan 1970; 52.18 weeks a year is close enough for a dateline.
		int days = week * 7;
		int dayOfYear = days - (days / 1461) * 1461; // lf-fixed: days in four years
		if (dayOfYear >= 365)
			dayOfYear = (dayOfYear - 365) - ((dayOfYear - 365) / 365) * 365;

		return dayOfYear / 7 + 1;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SectionBody(array<ref LF_NewspaperFact> facts, int week, LF_NewspaperSectionDef section)
	{
		// Group by key, newest last seen first.
		array<string> keys = {};
		array<ref LF_NewspaperFact> rows = {};
		for (int i = facts.Count() - 1; i >= 0; i--)
		{
			LF_NewspaperFact fact = facts[i];
			if (!fact || fact.m_eKind != section.m_eKind || !IsPrintable(fact.m_eKind) || WeekOf(fact.m_iUnixTime) != week)
				continue;

			int at = keys.Find(fact.m_sKey);
			if (at < 0)
			{
				keys.Insert(fact.m_sKey);
				LF_NewspaperFact row = new LF_NewspaperFact();
				row.m_eKind = fact.m_eKind;
				row.m_sName = fact.m_sName;
				row.m_sDetail = fact.m_sDetail;
				row.m_iAmount = fact.m_iAmount;
				row.m_iAmount2 = fact.m_iAmount2;
				rows.Insert(row);
				continue;
			}

			// Walking back in time: a market row keeps its newest price and takes the oldest start price.
			if (fact.m_eKind == LF_ENewspaperFact.MARKET)
				rows[at].m_iAmount = fact.m_iAmount;
			else
				rows[at].m_iAmount = rows[at].m_iAmount + fact.m_iAmount;
		}

		string body;
		int printed;
		foreach (LF_NewspaperFact row : rows)
		{
			if (printed >= LF_Utils.MaxInt(section.m_iMaxLines, 1))
				break;

			if (row.m_eKind == LF_ENewspaperFact.MARKET && row.m_iAmount == row.m_iAmount2)
				continue;

			string line = Line(section, row);
			if (line.IsEmpty())
				continue;

			if (!body.IsEmpty())
				body += "<br/>";

			body += line;
			printed++;
		}

		return body;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Line(LF_NewspaperSectionDef section, LF_NewspaperFact row)
	{
		string detail = row.m_sDetail;
		if (LF_SignCatalog.IsKey(detail))
			detail = LF_Terms.Text(detail);

		string amount = row.m_iAmount.ToString();
		string amount2 = row.m_iAmount2.ToString();
		if (row.m_eKind == LF_ENewspaperFact.REWARD || row.m_eKind == LF_ENewspaperFact.OPENING || row.m_eKind == LF_ENewspaperFact.MARKET)
		{
			amount = Money(row.m_iAmount);
			amount2 = Money(row.m_iAmount2);
		}

		string name = row.m_sName;
		if (!section.m_bNames || !NameAllowed(row.m_eKind))
			name = string.Empty;

		string template = section.m_sLine;
		if (name.IsEmpty())
		{
			template = section.m_sLineNoName;
			if (template.IsEmpty())
			{
				template = section.m_sLine;
				name = LF_Terms.Text(KEY_SOMEONE);
			}
		}

		if (template.IsEmpty())
			return string.Empty;

		// %1 name, %2 detail, %3 amount, %4 second amount.
		return LF_Terms.Text(template, Clean(name), Clean(detail), Clean(amount), Clean(amount2));
	}

	//------------------------------------------------------------------------------------------------
	//! "$1,250".
	static string Money(int amount)
	{
		return LF_MoneyFormat.FormatMoney(LF_Utils.MaxInt(amount, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! A printed value never carries markup or the edition wire's separators.
	static string Clean(string value)
	{
		string text = value;
		text.Replace("<", "");
		text.Replace(">", "");
		text.Replace("\n", " ");
		text.Replace("|", "/");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuilds this week's page, publishes it here and sends it to every client.
	static LF_NewspaperIssue Publish()
	{
		LF_NewspaperIssue issue = Build(Facts(), WeekOf(Now()), LF_NewspaperSectionsConfig.Get());
		if (issue.m_aStories.IsEmpty())
			return issue;

		LF_Newspaper.SetCurrent(issue);
		LF_GameModeRoleplay mode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (mode)
			mode.SendNewspaperEdition(issue);

		return issue;
	}

	//------------------------------------------------------------------------------------------------
	//! The wire form: masthead, dateline, then headline and body per story, "\n" between fields.
	static string Encode(LF_NewspaperIssue issue)
	{
		if (!issue)
			return string.Empty;

		string wire = issue.m_sMasthead + "\n" + issue.m_sDateline;
		int count;
		foreach (LF_NewspaperStory story : issue.m_aStories)
		{
			if (count >= LF_Newspaper.MAX_STORIES)
				break;

			// A newline in a translated line would shift every field after it on the client.
			string body = story.m_sBody;
			body.Replace("\n", " ");
			wire += "\n" + Clean(story.m_sHeadline) + "\n" + body;
			count++;
		}

		return wire;
	}

	//------------------------------------------------------------------------------------------------
	static LF_NewspaperIssue Decode(string wire)
	{
		array<string> fields = {};
		wire.Split("\n", fields, false);
		if (fields.Count() < 2)
			return null;

		LF_NewspaperIssue issue = new LF_NewspaperIssue();
		issue.m_sMasthead = fields[0];
		issue.m_sDateline = fields[1];
		for (int i = 2; i + 1 < fields.Count(); i += 2)
			issue.AddStory(fields[i], fields[i + 1]);

		return issue;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: empty the log and free the clock; the bus subscription stays.
	static void ForgetFacts()
	{
		s_aFacts = null;
		s_iWeek = -1;
		s_iClockOverride = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aFacts = null;
		s_Subscriber = null;
		s_bPublishQueued = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(PublishQueued);
		s_iWeek = -1;
		s_iClockOverride = 0;
	}
}

//------------------------------------------------------------------------------------------------
class LF_NewspaperEdition_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_NewspaperEdition";
	}

	override void Apply()
	{
		LF_NewspaperEdition.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The edition reaches clients through the game mode, which every client has. Words only, server to all.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			LF_NewspaperEdition.EnsureSubscribed();
	}

	//------------------------------------------------------------------------------------------------
	//! A late joiner gets the current edition once its body is in.
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		if (Replication.IsServer() && !LF_NewspaperEdition.Facts().IsEmpty())
			SendNewspaperEdition(LF_Newspaper.Current());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every client's page shows this edition from the next Read.
	void SendNewspaperEdition(LF_NewspaperIssue issue)
	{
		string wire = LF_NewspaperEdition.Encode(issue);
		if (wire.IsEmpty())
			return;

		Rpc(RpcDo_NewspaperEdition, wire);
		// A Broadcast never runs on the server: a listen host's own page takes it here.
		RpcDo_NewspaperEdition(wire);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_NewspaperEdition(string wire)
	{
		LF_NewspaperIssue issue = LF_NewspaperEdition.Decode(wire);
		if (!issue)
		{
			LF_Debug.Error(LF_Newspaper.FEATURE, "edition from the server refused: unreadable");
			return;
		}

		LF_Newspaper.SetCurrent(issue);
	}
}
