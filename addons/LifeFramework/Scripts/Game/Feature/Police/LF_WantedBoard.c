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

			LF_PlayerAccount account;
			if (accounts)
				account = accounts.GetFromCache(report.m_sPlayerUid);

			string name;
			if (report.m_bIdentified && account && account.GetActiveCharacter())
				name = account.GetActiveCharacter().GetFullName();

			int level = 1;
			if (account)
				level = account.GetWantedLevel();

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
	//! Server body of the read RPC. The reader must stand at a board; the server finds it, the
	//! client names nothing.
	static void ServerRead(SCR_PlayerController controller)
	{
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		IEntity reader = controller.GetControlledEntity();
		
		if (!reader || !LF_ActionSite.IsNear(reader.GetOrigin(), LF_PoliceConfig.BoardReadDistance(), READ_ACTION))
		{
			LF_Debug.Warn(FEATURE, string.Format("board read refused for player %1: not at a wanted board", playerId));
			return;
		}

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
			LF_Dispatch.BuildBoardEntries(LF_PoliceConfig.DispatchLogLines(), dispatches);
			// Witness statements (#266, #267) ride under the dispatch log.
			LF_Witness.AppendBoardEntries(LF_PoliceConfig.DispatchLogLines(), dispatches);
		}

		LF_Debug.Info(FEATURE, string.Format("board read by player %1: police=%2 posters=%3 dispatches=%4", playerId, police, count, dispatches.Count()));
		controller.LF_ShowWantedBoard(police, names, offences, clothing, vehicles, places, levels, dispatches);
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
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-Wanted_TitlePolice")));
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
				if (LF_Witness.IsBoardEntry(entry))
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
	//! Client: the server's answer arrived; open the card of posters.
	static void OpenCard(bool police, array<string> names, array<string> offences, array<string> clothing, array<string> vehicles, array<string> places, array<int> levels, array<string> dispatches = null)
	{
		if (!OpenParts(BuildParts(police, names, offences, clothing, vehicles, places, levels, dispatches)))
			return;


		int count;
		if (offences)
			count = offences.Count();

		LF_Debug.Info(FEATURE, string.Format("wanted card open: police=%1 posters=%2", police, count));
		if (dispatches)
		{
			foreach (int line, string entry : dispatches)
			{
				LF_Debug.Info(FEATURE, string.Format("dispatch line %1: %2", line, entry));
			}
		}

		bool sizesMatch = names && clothing && vehicles && places && levels && names.Count() == count && clothing.Count() == count && vehicles.Count() == count && places.Count() == count && levels.Count() == count;
		if (!sizesMatch)
			return;

		for (int i = 0; i < count; i++)
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
//! "Read the wanted posters" on the station's cork board. Local to the reader: it plays the
//! paper sound and asks the server, which answers with the posters this reader may see.
class LF_WantedBoardReadAction : ScriptedUserAction
{
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
			controller.LF_AskWantedBoard();
		else
			controller.LF_AskServiceBoard(m_sService);
	}
}

//------------------------------------------------------------------------------------------------
//! Wanted board bridge on the player controller (owned by the reader from join).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the posters of the board this player stands at.
	void LF_AskWantedBoard()
	{
		if (Replication.IsServer())
			RpcAsk_LF_WantedBoard();
		else
			Rpc(RpcAsk_LF_WantedBoard);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player the posters.
	void LF_ShowWantedBoard(bool police, array<string> names, array<string> offences, array<string> clothing, array<string> vehicles, array<string> places, array<int> levels, array<string> dispatches)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowWantedBoard(police, names, offences, clothing, vehicles, places, levels, dispatches);
		else
			Rpc(RpcDo_LF_ShowWantedBoard, police, names, offences, clothing, vehicles, places, levels, dispatches);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_WantedBoard()
	{
		LF_WantedBoard.ServerRead(this);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowWantedBoard(bool police, array<string> names, array<string> offences, array<string> clothing, array<string> vehicles, array<string> places, array<int> levels, array<string> dispatches)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_WantedBoard.FEATURE, "wanted card not opened: no workspace on this machine");
			return;
		}

		LF_WantedBoard.OpenCard(police, names, offences, clothing, vehicles, places, levels, dispatches);
	}
}
