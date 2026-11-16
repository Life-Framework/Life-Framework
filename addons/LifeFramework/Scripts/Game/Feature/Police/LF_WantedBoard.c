//------------------------------------------------------------------------------------------------
//! The wanted board at the police station: a cork board of paper posters, one per crime report
//! (LF_WantedReports). Reading it asks the server, which answers per read with what this reader
//! may see; nothing is replicated continuously.
//!   on-duty police : offence, clothing, vehicle, place, wanted level; the name once identified;
//!                    below the posters, the dispatch log (LF_Dispatch, M1-20)
//!   anyone else    : the public poster, the same without the place
//! The name is never on a poster before the server marked the report identified.
//------------------------------------------------------------------------------------------------
class LF_WantedBoard
{
	static const string FEATURE = "WantedBoard";
	//! The board kind the wanted board registers under (LF_BoardProviders).
	static const string KIND = "wanted";
	//! Texts per poster in a board answer: name, offence, clothing, vehicle, place.
	static const int POSTER_FIELDS = 5;
	static const string READ_ACTION = "LF_WantedBoardReadAction";
	static const string STARS_ON = "*";
	static const string STARS_OFF = "-";

	//! The board the local reader asked about, so the card closes when they walk off.
	protected static EntityID s_PendingBoard;
	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	//! Server: the posters a reader may see, newest first, as parallel arrays for the reply RPC.
	//! \param police True for an on-duty officer (full detail).
	//! \return Number of posters.
	static int BuildPosters(bool police, int maxPosters, notnull array<string> outNames, notnull array<string> outOffences, notnull array<string> outClothing, notnull array<string> outVehicles, notnull array<string> outPlaces, notnull array<int> outLevels)
	{
		outNames.Clear();
		outOffences.Clear();
		outClothing.Clear();
		outVehicles.Clear();
		outPlaces.Clear();
		outLevels.Clear();

		array<LF_WantedReportRecord> reports = {};
		LF_WantedReports.CollectNewestFirst(reports);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		foreach (LF_WantedReportRecord report : reports)
		{
			if (maxPosters > 0 && outOffences.Count() >= maxPosters)
				break;

			// The poster names the character the report is keyed by (#331), not whoever the account
			// plays now.
			LF_PlayerCharacter suspect;
			LF_CharacterStanding standing;
			if (accounts)
			{
				suspect = accounts.FindCharacterByKey(report.m_sPlayerUid);
				standing = accounts.FindStanding(report.m_sPlayerUid);
			}

			string name;
			if (report.m_bIdentified && suspect)
				name = suspect.GetFullName();

			int level = 1;
			if (standing)
				level = standing.m_iWantedLevel;

			string place;
			if (police)
				place = report.m_sPlace;

			outNames.Insert(name);
			outOffences.Insert(report.m_sOffence);
			outClothing.Insert(report.m_sClothing);
			outVehicles.Insert(report.m_sVehicle);
			outPlaces.Insert(place);
			outLevels.Insert(level);
		}

		return outOffences.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the read: the shared board read (LF_BoardProviders) for the wanted kind. The
	//! reader must stand at a board; the server finds it, the client names nothing.
	static void ServerRead(SCR_PlayerController controller)
	{
		LF_BoardProviders.ServerRead(controller, KIND);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the answer for one reader, built fresh per read: the posters this reader may see and,
	//! for an officer, the dispatch log under them.
	static LF_BoardAnswer BuildAnswer(int playerId)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts)
			account = accounts.GetFromCache(playerId);

		bool police = LF_PoliceUtils.IsOnDutyOfficer(account);
		array<string> names = {};
		array<string> offences = {};
		array<string> clothing = {};
		array<string> vehicles = {};
		array<string> places = {};
		array<int> levels = {};
		int count = BuildPosters(police, LF_PoliceConfig.BoardMaxPosters(), names, offences, clothing, vehicles, places, levels);
		// The dispatch log (M1-20) is pinned beside the posters for officers only.
		array<string> dispatches = {};
		if (police)
		{
			// Reported stolen vehicles (#414) come first: the plate is the lead.
			array<string> logEntries = {};
			LF_Dispatch.BuildBoardEntries(LF_PoliceConfig.DispatchLogLines(), logEntries);
			LF_StolenVehicles.BoardEntries(LF_PoliceConfig.DispatchLogLines(), dispatches);
			dispatches.InsertAll(logEntries);
			// Witness statements (#266, #267) ride under the dispatch log.
			LF_Witness.AppendBoardEntries(LF_PoliceConfig.DispatchLogLines(), dispatches);
		}

		LF_Debug.Info(FEATURE, string.Format("board read by player %1: police=%2 posters=%3 dispatches=%4", playerId, police, count, dispatches.Count()));
		return Pack(police, names, offences, clothing, vehicles, places, levels, dispatches);
	}

	//------------------------------------------------------------------------------------------------
	//! The poster arrays and the dispatch log as one flat answer: per poster POSTER_FIELDS texts (name,
	//! offence, clothing, vehicle, place) in m_aTexts and its level in m_aNumbers; the dispatch entries
	//! follow the posters in m_aTexts.
	static LF_BoardAnswer Pack(bool police, array<string> names, array<string> offences, array<string> clothing, array<string> vehicles, array<string> places, array<int> levels, array<string> dispatches)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		answer.m_bPrivileged = police;
		int count;
		if (offences)
			count = offences.Count();

		bool sizesMatch = names && clothing && vehicles && places && levels && names.Count() == count && clothing.Count() == count && vehicles.Count() == count && places.Count() == count && levels.Count() == count;
		if (!sizesMatch)
		{
			LF_Debug.Error(FEATURE, "board answer has no posters: poster arrays differ in size");
			count = 0;
		}

		for (int i = 0; i < count; i++)
		{
			answer.m_aTexts.Insert(names[i]);
			answer.m_aTexts.Insert(offences[i]);
			answer.m_aTexts.Insert(clothing[i]);
			answer.m_aTexts.Insert(vehicles[i]);
			answer.m_aTexts.Insert(places[i]);
			answer.m_aNumbers.Insert(levels[i]);
		}

		if (dispatches)
			answer.m_aTexts.InsertAll(dispatches);

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Client, no UI: the poster arrays and dispatch entries out of an answer (the inverse of Pack). A
	//! text array shorter than its posters need yields no posters.
	static void Unpack(notnull LF_BoardAnswer answer, notnull array<string> names, notnull array<string> offences, notnull array<string> clothing, notnull array<string> vehicles, notnull array<string> places, notnull array<int> levels, notnull array<string> dispatches)
	{
		names.Clear();
		offences.Clear();
		clothing.Clear();
		vehicles.Clear();
		places.Clear();
		levels.Clear();
		dispatches.Clear();
		int count = answer.m_aNumbers.Count();
		if (answer.m_aTexts.Count() < count * POSTER_FIELDS)
		{
			LF_Debug.Error(FEATURE, string.Format("board answer dropped: %1 posters need %2 texts, got %3", count, count * POSTER_FIELDS, answer.m_aTexts.Count()));
			return;
		}

		for (int i = 0; i < count; i++)
		{
			int at = i * POSTER_FIELDS;
			names.Insert(answer.m_aTexts[at]);
			offences.Insert(answer.m_aTexts[at + 1]);
			clothing.Insert(answer.m_aTexts[at + 2]);
			vehicles.Insert(answer.m_aTexts[at + 3]);
			places.Insert(answer.m_aTexts[at + 4]);
			levels.Insert(answer.m_aNumbers[i]);
		}

		for (int line = count * POSTER_FIELDS; line < answer.m_aTexts.Count(); line++)
		{
			dispatches.Insert(answer.m_aTexts[line]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: remembers the board the reader stands at, for the card's walk-away close.
	static void SetPendingBoard(EntityID board)
	{
		s_PendingBoard = board;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the poster lines of the card (no UI), so a test reads what a player would see.
	static array<ref LF_UIPart> BuildParts(bool police, array<string> names, array<string> offences, array<string> clothing, array<string> vehicles, array<string> places, array<int> levels, array<string> dispatches = null)
	{
		array<ref LF_UIPart> parts = {};
		if (police)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", LF_Terms.Text("#LF-Wanted_TitlePolice")));
		else
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-Wanted_Title")));

		int count;
		if (offences)
			count = offences.Count();

		bool sizesMatch = names && clothing && vehicles && places && levels && names.Count() == count && clothing.Count() == count && vehicles.Count() == count && places.Count() == count && levels.Count() == count;
		if (!sizesMatch)
		{
			LF_Debug.Error(FEATURE, "board reply dropped: poster arrays differ in size");
			count = 0;
		}

		if (count == 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-Wanted_None")));

		for (int i = 0; i < count; i++)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Poster" + i, PosterHeadline(names[i], offences[i], levels[i])));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Seen" + i, PosterDescription(clothing[i], vehicles[i], places[i])));
		}

		if (police && dispatches && !dispatches.IsEmpty())
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "DispatchHeading", WidgetManager.Translate("#LF-Dispatch_BoardHeading")));
			foreach (int index, string entry : dispatches)
			{
				string line;
				if (LF_StolenVehicles.IsBoardEntry(entry))
					line = LF_StolenVehicles.BoardLine(entry);
				else if (LF_Witness.IsBoardEntry(entry))
					line = LF_Witness.BoardLine(entry);
				else
					line = LF_Dispatch.BoardLine(entry);

				if (!line.IsEmpty())
					parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Dispatch" + index, line));
			}
		}

		if (!police)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Tip", WidgetManager.Translate("#LF-Wanted_PublicTip")));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! "*** -- WANTED: <name or Unknown person> - <offence>"
	static string PosterHeadline(string name, string offence, int level)
	{
		string who = name;
		if (who.IsEmpty())
			who = WidgetManager.Translate("#LF-Wanted_Unknown");

		return string.Format("%1 - %2 (%3)", who, ResolveKey(offence), WidgetManager.Translate("#LF-Wanted_Level", level));
	}

	//------------------------------------------------------------------------------------------------
	//! "Wearing ..., driving ..., seen at ..." with empty parts left off.
	static string PosterDescription(string clothing, string vehicle, string place)
	{
		string wearing;
		array<string> pieces = {};
		clothing.Split("|", pieces, true);
		foreach (int index, string piece : pieces)
		{
			if (index > 0)
				wearing += ", ";

			wearing += ResolveKey(piece);
		}

		if (wearing.IsEmpty())
			wearing = WidgetManager.Translate("#LF-Wanted_NoDescription");

		string text = WidgetManager.Translate("#LF-Wanted_Wearing", wearing);
		if (vehicle.IsEmpty())
			text += "; " + WidgetManager.Translate("#LF-Wanted_OnFoot");
		else
			text += "; " + WidgetManager.Translate("#LF-Wanted_Driving", ResolveKey(vehicle));

		if (!place.IsEmpty())
			text += "; " + WidgetManager.Translate("#LF-Wanted_SeenAt", ResolvePlace(place));

		return text;
	}

	//------------------------------------------------------------------------------------------------
	static string Stars(int level)
	{
		string stars;
		for (int i = 0; i < 5; i++)
		{
			if (i < level)
				stars += STARS_ON;
			else
				stars += STARS_OFF;
		}

		return stars;
	}

	//------------------------------------------------------------------------------------------------
	static string ResolveKey(string value)
	{
		if (value.StartsWith("#"))
			return WidgetManager.Translate(value);

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! A place is a sign title (key or text) or "<grid key> 002-001".
	static string ResolvePlace(string place)
	{
		if (place.StartsWith(LF_WantedReports.PLACE_GRID_KEY + " "))
			return WidgetManager.Translate(LF_WantedReports.PLACE_GRID_KEY, place.Substring(LF_WantedReports.PLACE_GRID_KEY.Length() + 1, place.Length() - LF_WantedReports.PLACE_GRID_KEY.Length() - 1));

		return ResolveKey(place);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: open a card of lines at the board the reader stands at (the wanted board's own
	//! card and the service dispatch boards share it).
	//! eturn False when the screen did not open.
	static bool OpenParts(array<ref LF_UIPart> parts)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(parts);
		if (!s_Screen)
		{
			LF_Debug.Error(FEATURE, "board card failed to open");
			return false;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		// A read the server started for this player (no local action) names no board: anchor on
		// the board beside the reader, so walking off closes the card either way.
		float range = LF_PoliceConfig.BoardReadDistance();
		if (s_PendingBoard == EntityID.INVALID)
		{
			IEntity reader = LF_Utils.GetLocalCharacter();
			IEntity board;
			if (reader)
				board = LF_ActionSite.FindNear(reader.GetOrigin(), range + 2, READ_ACTION);
			if (board)
				s_PendingBoard = board.GetID();
		}

		if (s_PendingBoard != EntityID.INVALID)
			s_Screen.CloseBeyond(range, s_PendingBoard);

		s_PendingBoard = EntityID.INVALID;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: logs the posters and dispatch lines an opened card shows (the server's answer was
	//! painted by LF_WantedBoardProvider.Paint and opened by LF_BoardProviders.Open).
	static void LogCard(notnull LF_BoardAnswer answer)
	{
		array<string> names = {};
		array<string> offences = {};
		array<string> clothing = {};
		array<string> vehicles = {};
		array<string> places = {};
		array<int> levels = {};
		array<string> dispatches = {};
		Unpack(answer, names, offences, clothing, vehicles, places, levels, dispatches);
		LF_Debug.Info(FEATURE, string.Format("wanted card open: police=%1 posters=%2", answer.m_bPrivileged, offences.Count()));
		foreach (int line, string entry : dispatches)
		{
			LF_Debug.Info(FEATURE, string.Format("dispatch line %1: %2", line, entry));
		}

		for (int i = 0; i < offences.Count(); i++)
		{
			LF_Debug.Info(FEATURE, string.Format("poster %1: name='%2' offence=%3 level=%4 clothing='%5' vehicle='%6' place='%7'", i, names[i], offences[i], levels[i], clothing[i], vehicles[i], places[i]));
		}
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The wanted board as a board kind (LF_BoardProviders): the server answers with the posters this
//! reader may see, the client paints them.
class LF_WantedBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_WantedBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_WantedBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_WantedBoard.BuildAnswer(playerId);
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		array<string> names = {};
		array<string> offences = {};
		array<string> clothing = {};
		array<string> vehicles = {};
		array<string> places = {};
		array<int> levels = {};
		array<string> dispatches = {};
		LF_WantedBoard.Unpack(answer, names, offences, clothing, vehicles, places, levels, dispatches);
		return LF_WantedBoard.BuildParts(answer.m_bPrivileged, names, offences, clothing, vehicles, places, levels, dispatches);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_WantedBoard.LogCard(answer);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_WantedBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the wanted posters" on the station's cork board. Local to the reader: it plays the
//! paper sound and asks the server, which answers with the posters this reader may see.
class LF_WantedBoardReadAction : ScriptedUserAction
{
	//! The Police system is off (#1076): no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled("Police");
	}

	[Attribute("SOUND_PICK_UP", UIWidgets.EditBox, "Sound event on the board's SCR_SoundDataComponent played when the reader takes a poster in hand")]
	protected string m_sReadSound;

	[Attribute("", UIWidgets.EditBox, "Service key (medic, taxi, ...) when this board is a service's dispatch board (#250); empty for the wanted board")]
	protected string m_sService;

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_sService.IsEmpty())
			outName = "#LF-Wanted_Read";
		else
			outName = "#LF-Dispatch_Read";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_WantedBoardReadAction"))
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		// Performed on the server for a remote reader (a server-driven use, as the agent harness
		// does): answer that reader directly. The same distance rule applies in ServerRead.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
			SCR_PlayerController remote = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (m_sService.IsEmpty())
				LF_WantedBoard.ServerRead(remote);
			else
				LF_ServiceBoard.ServerRead(remote, m_sService);

			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_WantedBoard.FEATURE, "board read not sent: no SCR_PlayerController");
			return;
		}

		// The reader reaches up and takes a poster down: the vanilla pick-up gesture, and paper.
		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(pUserEntity.FindComponent(CharacterControllerComponent));
		if (characterController && characterController.CanPlayItemGesture())
			characterController.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);

		if (!m_sReadSound.IsEmpty())
			SCR_SoundManagerModule.CreateAndPlayAudioSource(pOwnerEntity, m_sReadSound);

		LF_WantedBoard.SetPendingBoard(pOwnerEntity.GetID());
		if (m_sService.IsEmpty())
			controller.LF_AskBoard(LF_WantedBoard.KIND);
		else
			controller.LF_AskServiceBoard(m_sService);
	}
}
