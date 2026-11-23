//------------------------------------------------------------------------------------------------
//! The law book on its lectern, read by section (#4429, police L8). One "Read: <Section>" row per
//! section of the book (`LF_LawSection`); a read lists that section's codes with their fine and time,
//! straight from the crime catalog, so the pack's amounts are what the page shows. Each section is a
//! board kind (LF_BoardProviders): the server builds the page fresh for the reader and replies once,
//! nothing is replicated, and the reader must stand at the lectern.
//! Wire shape: per code one text (the offence id, a #key) in m_aTexts and three numbers in m_aNumbers:
//! the fine, the time in seconds and 1 when the code is jail only.
//------------------------------------------------------------------------------------------------
class LF_LawBookRead
{
	static const string FEATURE = "Law";
	static const string KIND_PREFIX = "law_";
	static const string READ_ACTION = "LF_LawBookReadAction";
	static const int NUMBERS_PER_CODE = 3;	// lf-fixed: wire stride (fine, seconds, jail-only flag), not a tunable

	//------------------------------------------------------------------------------------------------
	//! The board kind of a section ("law_road").
	static string KindOf(LF_LawSection section)
	{
		return KIND_PREFIX + LF_LawCode.SectionKey(section);
	}

	//------------------------------------------------------------------------------------------------
	//! The string key of a section's name ("#LF-LawBook_Section_road").
	static string SectionNameKey(LF_LawSection section)
	{
		switch (section)
		{
			case LF_LawSection.ROAD: return "#LF-LawBook_Section_road";
			case LF_LawSection.NARCOTICS: return "#LF-LawBook_Section_narcotics";
			case LF_LawSection.WEAPONS: return "#LF-LawBook_Section_weapons";
		}

		return "#LF-LawBook_Section_criminal";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the page for these codes, in the order given. An entry with no id is skipped.
	static LF_BoardAnswer BuildAnswerFor(notnull array<ref LF_CrimeEntry> codes)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		foreach (LF_CrimeEntry code : codes)
		{
			if (!code || code.m_sCrimeId.IsEmpty())
				continue;

			answer.m_aTexts.Insert(code.m_sCrimeId);
			answer.m_aNumbers.Insert(code.m_iFine);
			answer.m_aNumbers.Insert(code.m_iSentenceSeconds);
			if (code.m_bJailOnly)
				answer.m_aNumbers.Insert(1);
			else
				answer.m_aNumbers.Insert(0);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the page of one section from the catalog now in force (core rows plus the pack's).
	static LF_BoardAnswer BuildAnswer(LF_LawSection section)
	{
		array<LF_CrimeEntry> found = {};
		LF_LawCode.CollectSection(section, found);
		array<ref LF_CrimeEntry> codes = {};
		foreach (LF_CrimeEntry code : found)
		{
			codes.Insert(code);
		}

		LF_BoardAnswer answer = BuildAnswerFor(codes);
		LF_Debug.Info(FEATURE, string.Format("law book read: section=%1 codes=%2", LF_LawCode.SectionKey(section), answer.m_aTexts.Count()));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! "Speeding  $200, 5 min", "Murder  jail only, 15 min" or "Insult  $50". The time is left off at 0.
	static string CodeLine(string offenceKey, int fine, int seconds, bool jailOnly)
	{
		string name = LF_WantedBoard.ResolveKey(offenceKey);
		int minutes = LF_BookingDesk.Minutes(seconds);
		if (jailOnly)
			return WidgetManager.Translate("#LF-LawBook_LineJail", name, minutes.ToString());

		if (minutes > 0)
			return WidgetManager.Translate("#LF-LawBook_LineFineTime", name, fine.ToString(), minutes.ToString());

		return WidgetManager.Translate("#LF-LawBook_LineFine", name, fine.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the page's lines. A text with no triple of numbers is dropped, and logged.
	static array<ref LF_UIPart> BuildParts(LF_LawSection section, notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-LawBook_Title", WidgetManager.Translate(SectionNameKey(section)))));

		int codes = answer.m_aTexts.Count();
		if (answer.m_aNumbers.Count() < codes * NUMBERS_PER_CODE)
		{
			LF_Debug.Error(FEATURE, string.Format("law book reply dropped: %1 codes need %2 numbers, got %3", codes, codes * NUMBERS_PER_CODE, answer.m_aNumbers.Count()));
			codes = 0;
		}

		if (codes == 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-LawBook_Empty")));

		for (int i = 0; i < codes; i++)
		{
			int at = i * NUMBERS_PER_CODE;
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Code" + i, CodeLine(answer.m_aTexts[i], answer.m_aNumbers[at], answer.m_aNumbers[at + 1], answer.m_aNumbers[at + 2] != 0)));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! One section's board kind.
class LF_LawBookProvider : LF_BoardProvider
{
	protected LF_LawSection m_eSection;

	void LF_LawBookProvider(LF_LawSection section)
	{
		m_eSection = section;
	}

	override string GetKind()
	{
		return LF_LawBookRead.KindOf(m_eSection);
	}

	override string GetReadAction()
	{
		return LF_LawBookRead.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_LawBookRead.BuildAnswer(m_eSection);
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		return LF_LawBookRead.BuildParts(m_eSection, answer);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_LawBookRead.FEATURE, string.Format("law book page open: section=%1 codes=%2", LF_LawCode.SectionKey(m_eSection), answer.m_aTexts.Count()));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_LawBookProvider(LF_LawSection.CRIMINAL));
		providers.Insert(new LF_LawBookProvider(LF_LawSection.ROAD));
		providers.Insert(new LF_LawBookProvider(LF_LawSection.NARCOTICS));
		providers.Insert(new LF_LawBookProvider(LF_LawSection.WEAPONS));
	}
}

//------------------------------------------------------------------------------------------------
//! "Read: Traffic" on the lectern. One row per section; the row's `m_sSection` is the owner-config key
//! of the section it opens. Local to the reader: it asks the server, which answers that reader once.
class LF_LawBookReadAction : ScriptedUserAction
{
	[Attribute("criminal", UIWidgets.EditBox, "The section this row opens: criminal, road, narcotics or weapons")]
	protected string m_sSection;

	//------------------------------------------------------------------------------------------------
	protected bool Section(out LF_LawSection section)
	{
		return LF_LawCode.ParseSection(m_sSection, section);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		LF_LawSection section;
		return Section(section);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_LawSection section;
		if (!Section(section))
			return false;

		outName = WidgetManager.Translate("#LF-LawBook_Read", WidgetManager.Translate(LF_LawBookRead.SectionNameKey(section)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;

		LF_LawSection section;
		if (!Section(section))
		{
			LF_Debug.Error(LF_LawBookRead.FEATURE, string.Format("law book read refused: '%1' is not a section", m_sSection));
			return;
		}

		string kind = LF_LawBookRead.KindOf(section);
		// A server-driven use (the agent harness) answers that reader directly; the range rule applies.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
			LF_BoardProviders.ServerRead(SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId)), kind);
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_LawBookRead.FEATURE, "law book read not sent: no SCR_PlayerController");
			return;
		}

		LF_WantedBoard.SetPendingBoard(pOwnerEntity.GetID());
		controller.LF_AskBoard(kind);
	}
}
