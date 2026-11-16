// LF_RaceBoard.c - the record board at the track (#1925): a board kind (LF_BoardProviders, #1945).
// The server answers a read with the top times of every course that has one (LF_RaceRecords); the
// client paints a card: a heading per course, then up to five rows of place, name and time. Nothing
// is cached or replicated; each read asks again, so a new record shows on the next read.

//------------------------------------------------------------------------------------------------
class LF_RaceBoard
{
	static const string KIND = "racerecords";
	//! The read action class a reader must stand at (LF_ActionSite).
	static const string READ_ACTION = "LF_RaceBoardReadAction";
	//! Per row: course id and name in the texts, the time in the numbers.
	static const int TEXT_STRIDE = 2;

	//------------------------------------------------------------------------------------------------
	//! Server: the top times of every course with a record, packed flat for the RPC.
	static LF_BoardAnswer BuildAnswer()
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		array<string> courses = {};
		LF_RaceRecords.Courses(courses);
		array<ref LF_RaceRecordRow> top = {};
		foreach (string courseId : courses)
		{
			LF_RaceRecords.Top(courseId, LF_RaceRecords.BOARD_ROWS, top);
			foreach (LF_RaceRecordRow row : top)
			{
				answer.m_aTexts.Insert(row.m_sCourseId);
				answer.m_aTexts.Insert(row.m_sName);
				answer.m_aNumbers.Insert(row.m_iBestMs);
			}
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! One row as the card shows it: "1. Name  1:23.4". A record with no name shows a stand-in.
	static string RowText(int place, string name, int ms)
	{
		if (name.IsEmpty())
			name = WidgetManager.Translate("#LF-RaceBoard_Unknown");

		return string.Format("%1. %2  %3", place, name, LF_RaceHud.Clock(ms));
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the card lines. A malformed answer (texts and numbers out of step) paints the
	//! rows that line up and logs the rest.
	static array<ref LF_UIPart> BuildParts(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-RaceBoard_Title")));

		int rows = answer.m_aNumbers.Count();
		if (answer.m_aTexts.Count() != rows * TEXT_STRIDE)
		{
			LF_Debug.Error(LF_RaceRecords.FEATURE, string.Format("record board answer out of step: %1 texts, %2 times", answer.m_aTexts.Count(), rows));
			rows = Math.Min(rows, answer.m_aTexts.Count() / TEXT_STRIDE);
		}

		if (rows < 1)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-RaceBoard_None")));

		string course;
		int place = 0;
		for (int i = 0; i < rows; i++)
		{
			string rowCourse = answer.m_aTexts[i * TEXT_STRIDE];
			if (i == 0 || rowCourse != course)
			{
				course = rowCourse;
				place = 0;
				parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Course" + i, course));
			}

			place++;
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Record" + i, RowText(place, answer.m_aTexts[i * TEXT_STRIDE + 1], answer.m_aNumbers[i])));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
class LF_RaceBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_RaceBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_RaceBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_RaceBoard.BuildAnswer();
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_RaceBoard.BuildParts(answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_RaceRecords.FEATURE, string.Format("record board card: %1 row(s)", answer.m_aNumbers.Count()));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_RaceBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the record board" on the board at the track. Local to the reader: it asks the server, which
//! answers if the reader stands at the board.
class LF_RaceBoardReadAction : ScriptedUserAction
{
	//! The Racing system is off: no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_RaceRunner.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-RaceBoard_Read";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses(LF_RaceRunner.SYSTEM, "LF_RaceBoardReadAction"))
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
			LF_BoardProviders.ServerRead(remote, LF_RaceBoard.KIND);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_RaceRecords.FEATURE, "record board read not sent: no SCR_PlayerController");
			return;
		}

		controller.LF_AskBoard(LF_RaceBoard.KIND);
	}
}
