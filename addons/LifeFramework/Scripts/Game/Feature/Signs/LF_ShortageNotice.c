// LF_ShortageNotice.c - the paper notice on the clinic door while the clinic is short (#4448, 0.3.0 Crime D8).
//
// The only signal of the shortage: no toast to everyone (in-world over pop-ups). The notice reads the
// shortage state the clinic already replicates (LF_ClinicShortage.IsActive: the server's clock, a
// client's replicated flag), so it never holds a clock or a second copy of the truth. While the clinic is
// short the local row source `clinic_shortage` prints why and what changed on the styled face; when the
// shortage ends the rows are gone and the component takes the paper off the door.
//
// Client only. The server prints no face and shows no mesh. Nothing is sent and nothing is persisted.

//------------------------------------------------------------------------------------------------
class LF_ShortageNotice
{
	static const string FEATURE = "Signs";
	static const string SOURCE_KIND = "clinic_shortage";
	static const string SIGN_ID = "clinic_shortage";
	static const string WHY_LABEL = "#LF-ClinicShortage_Why";
	static const string WHY_VALUE = "#LF-ClinicShortage_WhyValue";
	static const string PRICES_LABEL = "#LF-ClinicShortage_Prices";
	static const string PRICES_VALUE = "#LF-ClinicShortage_PricesValue";
	static const string UNTIL_LABEL = "#LF-ClinicShortage_Until";
	static const string UNTIL_VALUE = "#LF-ClinicShortage_UntilValue";

	//------------------------------------------------------------------------------------------------
	//! True while the clinic is short (server clock, or the flag a client received).
	static bool IsShort()
	{
		return LF_ClinicShortage.IsActive();
	}

	//------------------------------------------------------------------------------------------------
	//! A localization key written out, or the key itself where the table has no row (a client without one).
	static string Text(string key)
	{
		string text = LF_SignCatalog.Resolve(key);
		if (text.IsEmpty())
			return key;

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The rows the notice prints: three while the clinic is short, none otherwise (no rows, no notice).
	static void BuildRows(bool isShort, notnull array<ref LF_SignRow> rows)
	{
		if (!isShort)
			return;

		rows.Insert(LF_SignRow.Create(Text(WHY_LABEL), Text(WHY_VALUE)));
		rows.Insert(LF_SignRow.Create(Text(PRICES_LABEL), Text(PRICES_VALUE)));
		rows.Insert(LF_SignRow.Create(Text(UNTIL_LABEL), Text(UNTIL_VALUE)));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the paper hangs on the door: only while the clinic is short.
	static bool Hangs(bool isShort)
	{
		return isShort;
	}
}

//------------------------------------------------------------------------------------------------
//! The local row source: a sign naming `clinic_shortage` prints the shortage rows from the state the
//! client already holds; it asks no one.
class LF_ShortageNoticeSource : LF_SignRowSource
{
	//------------------------------------------------------------------------------------------------
	override string GetKind()
	{
		return LF_ShortageNotice.SOURCE_KIND;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsLocal()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void LocalRows(notnull IEntity sign, notnull array<ref LF_SignRow> rows)
	{
		LF_ShortageNotice.BuildRows(LF_ShortageNotice.IsShort(), rows);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_SignRowSourceSet
{
	override void Collect(notnull array<ref LF_SignRowSource> sources)
	{
		super.Collect(sources);
		sources.Insert(new LF_ShortageNoticeSource());
	}
}

//------------------------------------------------------------------------------------------------
class LF_ShortageNoticeComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! On the notice prefab: hangs the paper only while the clinic is short. A client polls the state every
//! couple of seconds (it is a replicated bool, so a poll is free) and shows or hides the mesh. A dedicated
//! server draws nothing and does not poll.
class LF_ShortageNoticeComponent : ScriptComponent
{
	static const int POLL_MS = 2000;	// lf-fixed: a paper going up or coming down needs no faster check

	//! -1 before the first check, then 0 hidden or 1 shown.
	protected int m_iShown = -1;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		Apply(owner);
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Poll);
	}

	//------------------------------------------------------------------------------------------------
	protected void Poll()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		Apply(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(IEntity owner)
	{
		bool hangs = LF_ShortageNotice.Hangs(LF_ShortageNotice.IsShort());
		int state = 0;
		if (hangs)
			state = 1;

		if (state == m_iShown)
			return;

		m_iShown = state;
		if (hangs)
			owner.SetFlags(EntityFlags.VISIBLE, true);
		else
			owner.ClearFlags(EntityFlags.VISIBLE, true);

		LF_Debug.Info(LF_ShortageNotice.FEATURE, string.Format("clinic shortage notice %1", state));
	}
}
