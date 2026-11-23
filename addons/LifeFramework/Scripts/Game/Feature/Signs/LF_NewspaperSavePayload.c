// LF_NewspaperSavePayload.c: the Courier's save section (WO-W2.3, #4464). Server only.
//
// Save section "newspaper": this week's public facts (LF_NewspaperEdition's log), the edition on sale
// (its unix week and its printed words) and the last few back issues. Before this section a restart
// forgot the log and every paper read the placeholder until the next fact came in.
//
// PUBLIC FACTS ONLY (N31, #4303). The section keeps the edition's name rule on both sides: a fact kind the
// paper never prints (an open case, a hidden record) is neither written nor read, and a name is kept only
// for arrests, rewards and openings, so a crime fact never carries who did it, in the save file or after a
// load. The printed editions are kept as printed: Build already applied the rule to their words.

//------------------------------------------------------------------------------------------------
//! One saved fact. Its own shape, so the save does not move when LF_NewspaperFact grows a field.
class LF_NewspaperFactRow : Managed
{
	int m_iKind;
	string m_sName;
	string m_sDetail;
	string m_sKey;
	int m_iAmount;
	int m_iAmount2;
	int m_iUnixTime;
}

//------------------------------------------------------------------------------------------------
//! One printed edition: its unix week (the edition number) and LF_NewspaperEdition.Encode's words.
class LF_NewspaperIssueRow : Managed
{
	int m_iWeek;
	string m_sWire;
}

//------------------------------------------------------------------------------------------------
//! The editions printed so far, the newest last. The newsagent (WO-W2.4) reads back issues here.
class LF_NewspaperBackIssues
{
	//! Back issues kept, the edition on sale among them.
	static const int MAX_ISSUES = 8; // lf-fixed: two months of weeklies, a few KB
	protected static ref array<ref LF_NewspaperIssueRow> s_aIssues;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_NewspaperIssueRow> Rows()
	{
		if (!s_aIssues)
			s_aIssues = new array<ref LF_NewspaperIssueRow>();

		return s_aIssues;
	}

	//------------------------------------------------------------------------------------------------
	//! The edition printed for a unix week, or null.
	static LF_NewspaperIssue Issue(int week)
	{
		foreach (LF_NewspaperIssueRow row : Rows())
		{
			if (row && row.m_iWeek == week)
				return LF_NewspaperEdition.Decode(row.m_sWire);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Files an edition under its week, replacing that week's earlier printing; the oldest go past MAX_ISSUES.
	static void Keep(int week, string wire)
	{
		if (week <= 0 || wire.IsEmpty())
			return;

		array<ref LF_NewspaperIssueRow> rows = Rows();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			if (!rows[i] || rows[i].m_iWeek == week)
				rows.RemoveOrdered(i);
		}

		LF_NewspaperIssueRow row = new LF_NewspaperIssueRow();
		row.m_iWeek = week;
		row.m_sWire = wire;
		// Newest week last, so a late save of an older week does not push out a newer one.
		int at = rows.Count();
		while (at > 0 && rows[at - 1].m_iWeek > week)
			at--;

		rows.InsertAt(row, at);
		while (rows.Count() > MAX_ISSUES)
			rows.RemoveOrdered(0);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aIssues = null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_NewspaperBackIssues_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_NewspaperBackIssues";
	}

	override void Apply()
	{
		LF_NewspaperBackIssues.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_NewspaperSavePayload : LF_SavePayload
{
	static const string NAME = "newspaper";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! The unix week of the edition on sale: the newest fact's week, 0 with no fact (the placeholder).
	static int EditionWeek()
	{
		int week;
		foreach (LF_NewspaperFact fact : LF_NewspaperEdition.Facts())
		{
			if (fact && LF_NewspaperEdition.IsPrintable(fact.m_eKind))
				week = LF_Utils.MaxInt(week, LF_NewspaperEdition.WeekOf(fact.m_iUnixTime));
		}

		return week;
	}

	//------------------------------------------------------------------------------------------------
	//! A crime groups by its offence (LF_NewspaperEdition.FactFor), never by who did it: whatever key a
	//! crime fact carries, the section keeps the offence.
	static string SafeKey(LF_ENewspaperFact kind, string key, string detail)
	{
		if (kind == LF_ENewspaperFact.CRIME)
			return detail;

		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! The row a fact is saved as, or null when the paper never prints its kind. The name rule applies.
	static LF_NewspaperFactRow ToRow(LF_NewspaperFact fact)
	{
		if (!fact || !LF_NewspaperEdition.IsPrintable(fact.m_eKind))
			return null;

		LF_NewspaperFactRow row = new LF_NewspaperFactRow();
		row.m_iKind = fact.m_eKind;
		if (LF_NewspaperEdition.NameAllowed(fact.m_eKind))
			row.m_sName = fact.m_sName;

		row.m_sDetail = fact.m_sDetail;
		row.m_sKey = SafeKey(fact.m_eKind, fact.m_sKey, fact.m_sDetail);
		row.m_iAmount = fact.m_iAmount;
		row.m_iAmount2 = fact.m_iAmount2;
		row.m_iUnixTime = fact.m_iUnixTime;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The fact a saved row stands for, or null for a kind the paper never prints. The name rule applies
	//! again, so a hand-edited or older save cannot put a name on a crime.
	static LF_NewspaperFact FromRow(LF_NewspaperFactRow row)
	{
		if (!row)
			return null;

		LF_ENewspaperFact kind = row.m_iKind;
		if (!LF_NewspaperEdition.IsPrintable(kind))
			return null;

		LF_NewspaperFact fact = new LF_NewspaperFact();
		fact.m_eKind = kind;
		if (LF_NewspaperEdition.NameAllowed(kind))
			fact.m_sName = row.m_sName;

		fact.m_sDetail = row.m_sDetail;
		fact.m_sKey = SafeKey(kind, row.m_sKey, row.m_sDetail);
		fact.m_iAmount = row.m_iAmount;
		fact.m_iAmount2 = row.m_iAmount2;
		fact.m_iUnixTime = row.m_iUnixTime;
		return fact;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_NewspaperFactRow> facts = new array<ref LF_NewspaperFactRow>();
		foreach (LF_NewspaperFact fact : LF_NewspaperEdition.Facts())
		{
			LF_NewspaperFactRow row = ToRow(fact);
			if (row)
				facts.Insert(row);
		}

		// The edition on sale is filed with the back issues, so the paper reads the same after a restart. It
		// is built here the way Publish builds it, never taken from the page: before the first story the page
		// is the placeholder (or last week's paper) and must not be filed under this week.
		int week = EditionWeek();
		if (week > 0)
		{
			LF_NewspaperIssue edition = LF_NewspaperEdition.Build(LF_NewspaperEdition.Facts(), week, LF_NewspaperSectionsConfig.Get());
			if (edition && !edition.m_aStories.IsEmpty())
				LF_NewspaperBackIssues.Keep(week, LF_NewspaperEdition.Encode(edition));
		}

		array<ref LF_NewspaperIssueRow> issues = LF_NewspaperBackIssues.Rows();
		context.WriteValue("week", week);
		context.WriteValue("facts", facts);
		context.WriteValue("issues", issues);
		LF_Debug.Log("Persistence", string.Format("saved the Courier: week %1, %2 fact(s), %3 edition(s)", week, facts.Count(), issues.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		int week;
		array<ref LF_NewspaperFactRow> rows = new array<ref LF_NewspaperFactRow>();
		array<ref LF_NewspaperIssueRow> issues = new array<ref LF_NewspaperIssueRow>();
		if (!context.ReadValue("week", week) || !context.ReadValue("facts", rows) || !context.ReadValue("issues", issues))
		{
			LF_Debug.Warn(LF_Newspaper.FEATURE, "the Courier's save failed to read; the paper prints from this session's facts");
			return false;
		}

		// Replace, never add: loading twice ends in the same log.
		array<ref LF_NewspaperFact> facts = LF_NewspaperEdition.Facts();
		facts.Clear();
		int dropped;
		foreach (LF_NewspaperFactRow row : rows)
		{
			LF_NewspaperFact fact = LF_NewspaperSavePayload.FromRow(row);
			if (!fact)
			{
				dropped++;
				continue;
			}

			if (facts.Count() < LF_NewspaperEdition.MAX_FACTS)
				facts.Insert(fact);
		}

		LF_NewspaperBackIssues.ResetStatics();
		foreach (LF_NewspaperIssueRow issueRow : issues)
		{
			if (issueRow)
				LF_NewspaperBackIssues.Keep(issueRow.m_iWeek, issueRow.m_sWire);
		}

		LF_NewspaperIssue onSale;
		if (week > 0)
			onSale = LF_NewspaperBackIssues.Issue(week);

		if (onSale)
			LF_Newspaper.SetCurrent(onSale);
		else if (week > 0)
			LF_Debug.Warn(LF_Newspaper.FEATURE, string.Format("the Courier for week %1 was not in the save; the paper waits for the next fact", week));

		if (dropped > 0)
			LF_Debug.Warn(LF_Newspaper.FEATURE, string.Format("%1 saved fact(s) of a kind the paper never prints were dropped", dropped));

		LF_Debug.Info("Persistence", string.Format("applied the Courier: week %1, %2 fact(s), %3 edition(s)", week, facts.Count(), LF_NewspaperBackIssues.Rows().Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the section: no fact or edition was kept, so the paper prints this week from the
	//! facts that come in now (the placeholder until the first).
	override void OnAbsent()
	{
		LF_Debug.Info(LF_Newspaper.FEATURE, "save has no Courier; the paper prints the current week from new facts");
	}
}
