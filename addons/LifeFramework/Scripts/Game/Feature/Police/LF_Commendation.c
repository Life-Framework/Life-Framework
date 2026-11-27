//------------------------------------------------------------------------------------------------
//! One commendation as the ledger holds it: who earned it, what for, and when (unix seconds).
class LF_CommendationRow
{
	string m_sName;
	string m_sReason;
	int m_iTime;
}

//------------------------------------------------------------------------------------------------
//! The commendation ledger behind the plaque at the station (#4206, chapter #4170, PO-R9): a plain
//! text list kept in the server profile, one commendation per line (`time|name|reason`). Awarding is
//! a server call (`Award`); the plaque only reads. The ledger holds a name and a reason as text, so
//! it needs no account lookup and nothing in a save file; a restart reloads it from the profile.
//!
//! Server only; nothing replicates. A name or reason with the line separator in it has the
//! separator replaced by a space, so a row can never split.
//------------------------------------------------------------------------------------------------
class LF_Commendations
{
	static const string FEATURE = "Commendations";
	static const string SEPARATOR = "|";
	//! The ledger keeps this many commendations; the oldest drop off first.
	static const int MAX_ROWS = 200;	// lf-fixed: memory and file bound for the ledger, not a rule

	protected static ref array<ref LF_CommendationRow> s_aRows;
	protected static string s_sPath;
	protected static bool s_bMemoryOnly;

	//------------------------------------------------------------------------------------------------
	//! World end (LF_Commendations_StaticResetHook): the cached rows and the overrides are dropped; the
	//! next read loads the profile file again.
	static void ResetStatics()
	{
		s_aRows = null;
		s_sPath = "";
		s_bMemoryOnly = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: a ledger that never touches the profile file (loads nothing, saves nothing).
	static void SetMemoryOnly(bool memoryOnly)
	{
		s_bMemoryOnly = memoryOnly;
		s_aRows = null;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPath()
	{
		if (!s_sPath.IsEmpty())
			return s_sPath;

		return "$profile:LifeFramework/police_commendations.txt";
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_CommendationRow> Rows()
	{
		if (s_aRows)
			return s_aRows;

		s_aRows = new array<ref LF_CommendationRow>();
		if (!s_bMemoryOnly)
			Load();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: engraves a commendation. An empty name or reason is refused and logged.
	//! \return True when it was added.
	static bool Award(string name, string reason, int nowUnix)
	{
		string cleanName = Scrub(name);
		string cleanReason = Scrub(reason);
		if (cleanName.IsEmpty() || cleanReason.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "commendation refused: it needs a name and a reason");
			return false;
		}

		array<ref LF_CommendationRow> rows = Rows();
		LF_CommendationRow row = new LF_CommendationRow();
		row.m_sName = cleanName;
		row.m_sReason = cleanReason;
		row.m_iTime = nowUnix;
		rows.Insert(row);
		while (rows.Count() > MAX_ROWS)
		{
			rows.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("commendation engraved: %1 (%2)", cleanName, cleanReason));
		Save();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The commendations newest first, at most maxRows of them (0 for all).
	static void CollectNewestFirst(notnull array<ref LF_CommendationRow> outRows, int maxRows)
	{
		outRows.Clear();
		array<ref LF_CommendationRow> rows = Rows();
		// Latest time first; on a tie the one engraved later.
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			LF_CommendationRow row = rows[i];
			int at = outRows.Count();
			while (at > 0 && outRows[at - 1].m_iTime < row.m_iTime)
			{
				at--;
			}

			outRows.InsertAt(row, at);
		}

		if (maxRows > 0)
		{
			while (outRows.Count() > maxRows)
			{
				outRows.RemoveOrdered(outRows.Count() - 1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The separator out of a text, the ends trimmed.
	static string Scrub(string text)
	{
		string clean = text;
		clean.Replace(SEPARATOR, " ");
		clean.Replace("\n", " ");
		clean.Replace("\r", " ");
		return clean.Trim();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		string path = GetPath();
		if (!FileIO.FileExists(path))
			return;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot open the commendation file: %1", path));
			return;
		}

		string line;
		while (handle.ReadLine(line) >= 0)
		{
			array<string> pieces = {};
			line.Split(SEPARATOR, pieces, false);
			if (pieces.Count() != 3)
				continue;

			LF_CommendationRow row = new LF_CommendationRow();
			row.m_iTime = pieces[0].ToInt();
			row.m_sName = pieces[1];
			row.m_sReason = pieces[2];
			s_aRows.Insert(row);
		}
		handle.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Save()
	{
		if (s_bMemoryOnly)
			return;

		string path = GetPath();
		string parent = FilePath.StripFileName(path);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot write the commendation file: %1", path));
			return;
		}

		foreach (LF_CommendationRow row : Rows())
		{
			handle.WriteLine(row.m_iTime.ToString() + SEPARATOR + row.m_sName + SEPARATOR + row.m_sReason);
		}
		handle.Close();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Commendations' statics (LF_StaticReset finds it by reflection).
class LF_Commendations_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Commendations";
	}

	override void Apply()
	{
		LF_Commendations.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The commendations plaque in the station hall (#4206): a board kind (LF_BoardProviders). The server
//! answers with the ledger, newest first; the client paints one line per commendation. Anyone may
//! read it. Per commendation: the name and the reason in the texts, the time in the numbers.
class LF_CommendationPlaque
{
	//! The board kind the plaque registers under (LF_BoardProviders).
	static const string KIND = "commendations";
	//! The read action class a reader must stand at (LF_ActionSite).
	static const string READ_ACTION = "LF_CommendationPlaqueReadAction";
	//! Texts per commendation in an answer: name, reason.
	static const int TEXT_STRIDE = 2;
	//! Most commendations one read prints; the plaque has room for this many names.
	static const int MAX_ROWS = 30;	// lf-fixed: the plaque's size, not a rule

	//------------------------------------------------------------------------------------------------
	//! Server: the ledger as it stands, newest first, packed flat for the RPC.
	static LF_BoardAnswer BuildAnswer()
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		array<ref LF_CommendationRow> rows = {};
		LF_Commendations.CollectNewestFirst(rows, MAX_ROWS);
		foreach (LF_CommendationRow row : rows)
		{
			answer.m_aTexts.Insert(row.m_sName);
			answer.m_aTexts.Insert(row.m_sReason);
			answer.m_aNumbers.Insert(row.m_iTime);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the lines of the plaque. A malformed answer (texts and numbers out of step) paints
	//! the rows that line up and logs the rest.
	static array<ref LF_UIPart> BuildParts(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-Commendations_Title")));
		int rows = answer.m_aNumbers.Count();
		if (answer.m_aTexts.Count() != rows * TEXT_STRIDE)
		{
			LF_Debug.Error(LF_Commendations.FEATURE, string.Format("plaque answer out of step: %1 texts, %2 times", answer.m_aTexts.Count(), rows));
			rows = Math.Min(rows, answer.m_aTexts.Count() / TEXT_STRIDE);
		}

		if (rows < 1)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-Commendations_None")));

		for (int i = 0; i < rows; i++)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Commendation" + i, answer.m_aTexts[i * TEXT_STRIDE] + ": " + answer.m_aTexts[i * TEXT_STRIDE + 1]));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
class LF_CommendationPlaqueProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_CommendationPlaque.KIND;
	}

	override string GetReadAction()
	{
		return LF_CommendationPlaque.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_CommendationPlaque.BuildAnswer();
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_CommendationPlaque.BuildParts(answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_Commendations.FEATURE, string.Format("plaque card: %1 commendation(s)", answer.m_aNumbers.Count()));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_CommendationPlaqueProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the commendations plaque" on the plaque in the station hall. Local to the reader: it asks
//! the server, which answers with the ledger.
class LF_CommendationPlaqueReadAction : ScriptedUserAction
{
	//! The Police system is off: no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled("Police");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Commendations_Read";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_CommendationPlaqueReadAction"))
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
			LF_BoardProviders.ServerRead(remote, LF_CommendationPlaque.KIND);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_Commendations.FEATURE, "plaque read not sent: no SCR_PlayerController");
			return;
		}

		controller.LF_AskBoard(LF_CommendationPlaque.KIND);
	}
}
