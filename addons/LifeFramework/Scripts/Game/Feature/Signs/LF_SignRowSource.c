// LF_SignRowSource.c: rows a styled sign prints that come from live state (docs/architecture/sign-system.md).
//
// A sign names a source kind (LF_SignComponent.m_sRowSource, "market"). The source says which board kind the
// server answers (LF_BoardProviders, the existing read RPC) and turns the answer into LF_SignRow rows. The
// client asks once when the viewer comes within reach of the board and once per refresh window after that;
// the reply is held here and the sign repaints when it sees a newer version. Nothing is replicated and
// nothing is sent per frame.
//
// A feature adds `modded class LF_SignRowSourceSet` in its own file and inserts its source in Collect.

//------------------------------------------------------------------------------------------------
//! One kind of live rows. Subclass per kind.
class LF_SignRowSource
{
	//! The id LF_SignComponent.m_sRowSource names. Unique per kind.
	string GetKind()
	{
		return string.Empty;
	}

	//! The board kind the server answers for this source (LF_BoardProvider.GetKind).
	string GetBoardKind()
	{
		return string.Empty;
	}

	//! Pure: the rows an answer prints, in the answer's order.
	array<ref LF_SignRow> Rows(notnull LF_BoardAnswer answer)
	{
		array<ref LF_SignRow> rows = {};
		return rows;
	}
}

//------------------------------------------------------------------------------------------------
//! Features add `modded class LF_SignRowSourceSet` and insert their source in Collect (call super first).
class LF_SignRowSourceSet
{
	void Collect(notnull array<ref LF_SignRowSource> sources)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! The ask in flight, so a reply is told apart from a Read card (the same reply RPC carries both).
class LF_SignAskNote
{
	string m_sKind;
	bool m_bFace;
	int m_iTick;
}

//------------------------------------------------------------------------------------------------
class LF_SignRowSources
{
	static const string FEATURE = "Signs";
	//! An ask older than this had no reply (refused or lost); it no longer claims one.
	static const int ASK_EXPIRY_MS = 15000;	// lf-fixed: longer than any reply takes, shorter than a walk away and back
	//! A sign asks again no sooner than this after its last ask.
	static const int REFRESH_MS = 60000;	// lf-fixed: market prices move on sales and hours, not seconds
	//! Longest source kind a prefab may name.
	static const int MAX_KIND_LENGTH = 32;

	protected static ref map<string, ref LF_SignRowSource> s_Sources;
	protected static ref array<ref LF_SignAskNote> s_Asks;
	protected static ref map<string, ref array<ref LF_SignRow>> s_Rows;
	protected static ref map<string, int> s_Versions;

	//------------------------------------------------------------------------------------------------
	protected static void Ensure()
	{
		if (s_Sources)
			return;

		s_Sources = new map<string, ref LF_SignRowSource>();
		s_Asks = {};
		s_Rows = new map<string, ref array<ref LF_SignRow>>();
		s_Versions = new map<string, int>();
		array<ref LF_SignRowSource> found = {};
		LF_SignRowSourceSet sourceSet = new LF_SignRowSourceSet();
		sourceSet.Collect(found);
		foreach (LF_SignRowSource source : found)
		{
			if (!source || source.GetKind().IsEmpty())
			{
				LF_Debug.Error(FEATURE, "sign row source refused: none, or it names no kind");
				continue;
			}

			s_Sources.Set(source.GetKind(), source);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The source for a kind, or null (a prefab typo: the caller logs).
	static LF_SignRowSource Find(string kind)
	{
		Ensure();
		LF_SignRowSource source;
		if (!s_Sources.Find(kind, source))
			return null;

		return source;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this client may ask now: no earlier ask, or the last was a refresh window ago.
	static bool ShouldAsk(int lastAskMs, int nowMs)
	{
		if (lastAskMs == 0)
			return true;

		return nowMs - lastAskMs >= REFRESH_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a viewer at this distance may be answered: the server reads the board only within
	//! the board read distance, so a sign asks inside a margin of it.
	static bool InAskRange(float distance, float readDistance)
	{
		return distance <= readDistance * 0.9;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: record an ask going to the server (every board ask, a Read card or a face).
	static void NoteAsk(string kind, bool face)
	{
		Ensure();
		LF_SignAskNote note = new LF_SignAskNote();
		note.m_sKind = kind;
		note.m_bFace = face;
		note.m_iTick = System.GetTickCount();
		s_Asks.Insert(note);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a board reply arrived. True when it answers a face ask: the rows are stored and the
	//! caller opens no card. A reply with no matching ask is a Read card.
	static bool TakeFaceReply(string kind, notnull LF_BoardAnswer answer)
	{
		return TakeFaceReplyAt(kind, answer, System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! The testable body of TakeFaceReply: expire old asks, take the oldest ask of the kind.
	static bool TakeFaceReplyAt(string kind, notnull LF_BoardAnswer answer, int nowMs)
	{
		Ensure();
		for (int i = s_Asks.Count() - 1; i >= 0; i--)
		{
			if (nowMs - s_Asks[i].m_iTick > ASK_EXPIRY_MS)
				s_Asks.Remove(i);
		}

		int match = -1;
		foreach (int index, LF_SignAskNote note : s_Asks)
		{
			if (note.m_sKind == kind)
			{
				match = index;
				break;
			}
		}

		if (match < 0)
			return false;

		bool face = s_Asks[match].m_bFace;
		s_Asks.Remove(match);
		if (!face)
			return false;

		StoreAnswer(kind, answer);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the rows a board answer makes, under every source that reads that board kind.
	static void StoreAnswer(string boardKind, notnull LF_BoardAnswer answer)
	{
		Ensure();
		foreach (string sourceKind, LF_SignRowSource source : s_Sources)
		{
			if (source.GetBoardKind() != boardKind)
				continue;

			s_Rows.Set(sourceKind, source.Rows(answer));
			int version;
			s_Versions.Find(sourceKind, version);
			s_Versions.Set(sourceKind, version + 1);
			LF_Debug.Info(FEATURE, string.Format("sign rows stored: source=%1 rows=%2 version=%3", sourceKind, s_Rows.Get(sourceKind).Count(), version + 1));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The newest rows for a source kind, or null before the first reply.
	static array<ref LF_SignRow> RowsFor(string sourceKind)
	{
		Ensure();
		array<ref LF_SignRow> rows;
		if (!s_Rows.Find(sourceKind, rows))
			return null;

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Bumps with every stored reply; a sign repaints when it sees a number it has not painted.
	static int VersionFor(string sourceKind)
	{
		Ensure();
		int version;
		s_Versions.Find(sourceKind, version);
		return version;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Sources = null;
		s_Asks = null;
		s_Rows = null;
		s_Versions = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SignRowSources's statics (LF_StaticReset finds it by reflection).
class LF_SignRowSources_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SignRowSources";
	}

	override void Apply()
	{
		LF_SignRowSources.ResetStatics();
	}
}
