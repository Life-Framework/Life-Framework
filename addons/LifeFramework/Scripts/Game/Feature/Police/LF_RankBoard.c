//------------------------------------------------------------------------------------------------
//! The rank board at the station (#4206, chapter #4170, PO-R9): a printed board by the duty desk that
//! lists every officer on the police roster by rank, highest first, with the rank's title. It is a
//! board kind (LF_BoardProviders): the server builds the answer per read from the accounts it holds,
//! so a promotion shows on the next read; nothing is cached or replicated. Anyone may read it.
//!
//! The answer is flat: per officer one text (the name) and one number (the rank), already in board
//! order, so the client paints it as it arrives.
//------------------------------------------------------------------------------------------------
class LF_RankBoard
{
	static const string FEATURE = "RankBoard";
	//! The board kind the rank board registers under (LF_BoardProviders).
	static const string KIND = "rankboard";
	//! The read action class a reader must stand at (LF_ActionSite).
	static const string READ_ACTION = "LF_RankBoardReadAction";
	//! Most officers one board prints; the rest do not fit on the paper.
	static const int MAX_ROWS = 40;	// lf-fixed: the board's paper size, not a rule

	//------------------------------------------------------------------------------------------------
	//! Orders officers highest rank first, then by name. Parallel arrays in, parallel arrays out.
	static void SortRows(notnull array<string> names, notnull array<int> ranks)
	{
		int count = Math.Min(names.Count(), ranks.Count());
		// Insertion sort: a roster is tens of rows, and the arrays are tiny.
		for (int i = 1; i < count; i++)
		{
			string name = names[i];
			int rank = ranks[i];
			int at = i - 1;
			while (at >= 0 && IsBefore(name, rank, names[at], ranks[at]))
			{
				names[at + 1] = names[at];
				ranks[at + 1] = ranks[at];
				at--;
			}

			names[at + 1] = name;
			ranks[at + 1] = rank;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when officer A prints above officer B: the higher rank, then the name in alphabetical order.
	protected static bool IsBefore(string nameA, int rankA, string nameB, int rankB)
	{
		if (rankA != rankB)
			return rankA > rankB;

		return nameA.Compare(nameB, false) < 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The answer for a roster: sorted, capped, one name and one rank per officer. Mismatched arrays
	//! print the rows that line up and log the rest.
	static LF_BoardAnswer BuildAnswerFrom(array<string> names, array<int> ranks)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		if (!names || !ranks)
		{
			LF_Debug.Error(FEATURE, "rank board answer has no officers: roster arrays missing");
			return answer;
		}

		if (names.Count() != ranks.Count())
			LF_Debug.Error(FEATURE, string.Format("rank board roster out of step: %1 names, %2 ranks", names.Count(), ranks.Count()));

		array<string> sortedNames = {};
		array<int> sortedRanks = {};
		int count = Math.Min(names.Count(), ranks.Count());
		for (int i = 0; i < count; i++)
		{
			sortedNames.Insert(names[i]);
			sortedRanks.Insert(ranks[i]);
		}

		SortRows(sortedNames, sortedRanks);
		for (int row = 0; row < sortedNames.Count() && row < MAX_ROWS; row++)
		{
			answer.m_aTexts.Insert(sortedNames[row]);
			answer.m_aNumbers.Insert(sortedRanks[row]);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the roster as it is now, read from the accounts the server holds.
	static LF_BoardAnswer BuildAnswer()
	{
		array<string> names = {};
		array<int> ranks = {};
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
			accounts.CollectOfficerRows(names, ranks);

		LF_BoardAnswer answer = BuildAnswerFrom(names, ranks);
		LF_Debug.Info(FEATURE, string.Format("rank board read: officers=%1", answer.m_aNumbers.Count()));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! "Senior Officer: Sam Senior".
	static string RowText(string name, int rank)
	{
		return string.Format("%1: %2", WidgetManager.Translate("#" + LF_PoliceRanks.TitleKey(rank)), name);
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the lines of the board, so a test reads what a player would see.
	static array<ref LF_UIPart> BuildParts(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-RankBoard_Title")));
		int rows = Math.Min(answer.m_aTexts.Count(), answer.m_aNumbers.Count());
		if (answer.m_aTexts.Count() != answer.m_aNumbers.Count())
			LF_Debug.Error(FEATURE, string.Format("rank board answer out of step: %1 names, %2 ranks", answer.m_aTexts.Count(), answer.m_aNumbers.Count()));

		if (rows < 1)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-RankBoard_None")));

		for (int i = 0; i < rows; i++)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Officer" + i, RowText(answer.m_aTexts[i], answer.m_aNumbers[i])));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! Roster rows from the accounts the server holds. A modded class in this file because the account
//! map is protected and the account feature is not ours to change.
modded class LF_PlayerAccountManager
{
	//------------------------------------------------------------------------------------------------
	//! Every account on the police roster (the whitelist) with a character: the active character's
	//! full name and police rank. An account with no character has no name to print and is left off.
	void CollectOfficerRows(notnull array<string> names, notnull array<int> ranks)
	{
		names.Clear();
		ranks.Clear();
		foreach (string uid, LF_PlayerAccount account : m_mAccounts)
		{
			if (!account || !LF_WhitelistManager.IsWhitelistedForFaction(account.GetPersistentId(), LF_EFactionType.POLICE))
				continue;

			LF_PlayerCharacter character = account.GetActiveCharacter();
			if (!character)
				continue;

			int rank = LF_Utils.ClampInt(account.GetPoliceRank(), 0, LF_PoliceRanks.TopRank());
			names.Insert(character.GetFullName());
			ranks.Insert(rank);
		}
	}
}

//------------------------------------------------------------------------------------------------
class LF_RankBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_RankBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_RankBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_RankBoard.BuildAnswer();
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_RankBoard.BuildParts(answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_RankBoard.FEATURE, string.Format("rank board card: %1 officer(s)", answer.m_aNumbers.Count()));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_RankBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the rank board" on the printed board by the duty desk. Local to the reader: it asks the
//! server, which answers with the roster as it stands.
class LF_RankBoardReadAction : ScriptedUserAction
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
		outName = "#LF-RankBoard_Read";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_RankBoardReadAction"))
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
			LF_BoardProviders.ServerRead(remote, LF_RankBoard.KIND);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_RankBoard.FEATURE, "rank board read not sent: no SCR_PlayerController");
			return;
		}

		controller.LF_AskBoard(LF_RankBoard.KIND);
	}
}
