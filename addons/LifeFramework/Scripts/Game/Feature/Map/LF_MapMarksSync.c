//------------------------------------------------------------------------------------------------
//! D-M7: vanilla shared markers are off. m_iPlacedMarkerLimit is a protected field the mission
//! header sets, not an attribute, so the limit is forced to 0 after OnPostInit: the server's
//! RPC_AskAddStaticMarker then returns before it adds anything. The client side ask is refused
//! here too, so no networked add leaves this machine.
modded class SCR_MapMarkerSyncComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_iPlacedMarkerLimit = 0;
	}

	//------------------------------------------------------------------------------------------------
	override void AskAddStaticMarker(notnull SCR_MapMarkerBase marker)
	{
		LF_MapMarks.RefuseNetworked("sync component ask");
	}
}

//------------------------------------------------------------------------------------------------
//! Every player-placed marker is local (D-M7). The marker dialog, the quick menu and a drag all
//! end in InsertStaticMarker or RemoveStaticMarker, so this is the one choke point: a networked
//! player add becomes a local one, the 21st local mark is refused, and each change of the player's
//! own marks is sent to the server for saving (LF_MapMarksClient). Server markers (-1 owner) pass.
modded class SCR_MapMarkerManagerComponent
{
	protected bool m_bLFMapEntityMissingLogged;

	//------------------------------------------------------------------------------------------------
	//! Vanilla reads the map entity once, in OnPostInit, and every later use (OnMapOpen, OnMapPanEnd, Update)
	//! dereferences that copy. A world that places its SCR_MapEntity in a layer loaded after the game mode
	//! (the playtest town, #4726) leaves the copy null, and OnMapOpen threw a VME every frame the map was
	//! open (249 in one session). Pick the entity up again here; with no map entity at all, log once and
	//! let the caller skip the map work.
	//! \return true when the manager has a map entity to work with.
	bool LF_BindMapEntity()
	{
		if (m_MapEntity)
			return true;

		m_MapEntity = SCR_MapEntity.GetMapInstance();
		if (m_MapEntity)
		{
			LF_Debug.Info(LF_MapMarks.FEATURE, "marker manager picked up the map entity after its own init (the world placed it later)");
			return true;
		}

		if (!m_bLFMapEntityMissingLogged)
		{
			m_bLFMapEntityMissingLogged = true;
			LF_Debug.Error(LF_MapMarks.FEATURE, "marker manager has no map entity (this world has no SCR_MapEntity): map marks are off");
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the missing-map-entity error was logged (the test reads it).
	bool LF_MapEntityMissingLogged()
	{
		return m_bLFMapEntityMissingLogged;
	}

	//------------------------------------------------------------------------------------------------
	//! Test hook: forget the map entity, as a game mode that initialised before the world placed one.
	void LF_ForgetMapEntityForTest()
	{
		m_MapEntity = null;
		m_bLFMapEntityMissingLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMapOpen(MapConfiguration mapConfig)
	{
		if (!LF_BindMapEntity())
			return;

		super.OnMapOpen(mapConfig);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnMapPanEnd(float x, float y)
	{
		if (!LF_BindMapEntity())
			return;

		super.OnMapPanEnd(x, y);
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		if (!LF_BindMapEntity())
			return;

		super.Update(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	override void InsertStaticMarker(SCR_MapMarkerBase marker, bool isLocal, bool isServerMarker = false)
	{
		if (!marker || isServerMarker)
		{
			super.InsertStaticMarker(marker, isLocal, isServerMarker);
			return;
		}

		if (!isLocal)
		{
			LF_MapMarks.RefuseNetworked("placed as local instead");
			if (!GetGame().GetPlayerController())
				return; // a dedicated server has no local player to own the mark
		}

		if (LF_MapMarksClient.IsPersonal(marker) && !LF_MapMarksClient.IsRestoring() && LF_MapMarksClient.CountLocal(this) >= LF_MapMarks.MAX_PER_CHARACTER)
		{
			LF_Debug.Warn(LF_MapMarks.FEATURE, string.Format("mark refused: the map already holds %1 personal marks", LF_MapMarks.MAX_PER_CHARACTER));
			return;
		}

		super.InsertStaticMarker(marker, true, false);
		if (LF_MapMarksClient.IsPersonal(marker))
		{
			LF_MapMarksClient.ScheduleUpload();
			// A mark the player places also shows in the world as the HUD waypoint; a restore is not a placing.
			if (!LF_MapMarksClient.IsRestoring())
				LF_MapMarkWaypoint.OnMarkerPlaced(marker);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void RemoveStaticMarker(SCR_MapMarkerBase marker)
	{
		bool personal = marker && marker.GetMarkerID() == -1 && LF_MapMarksClient.IsPersonal(marker);
		super.RemoveStaticMarker(marker);
		if (personal)
		{
			LF_MapMarksClient.ScheduleUpload();
			LF_MapMarkWaypoint.OnMarkerRemoved(marker);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Client: the local player's personal marks. Uploads the whole list a moment after a change (one
//! RPC for a burst of edits or a drag) and re-inserts the server's list on spawn.
class LF_MapMarksClient
{
	static const int UPLOAD_DELAY_MS = 2000; // lf-fixed: batches a burst of edits under the MAP rate window

	protected static bool s_bRestoring;
	protected static bool s_bUploadQueued;
	//! The generation of the server's latest restore; every upload echoes it. 0: no restore yet.
	protected static int s_iGeneration;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		CancelUpload();
		s_bRestoring = false;
		s_iGeneration = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a queued upload. The controlled character changed: the queued list holds the previous
	//! character's marks, and uploading it now would save it under the new character's key (#2546).
	//! The generation is forgotten too, so nothing uploads until the new character's restore lands.
	static void ForgetCharacter()
	{
		if (s_bUploadQueued)
			LF_Debug.Info(LF_MapMarks.FEATURE, "queued mark upload cancelled: the controlled character changed");

		CancelUpload();
		s_iGeneration = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CancelUpload()
	{
		if (s_bUploadQueued && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Upload);

		s_bUploadQueued = false;
	}

	//------------------------------------------------------------------------------------------------
	//! A mark the player placed with the dialog (custom or military), not a scripted marker. A shared
	//! mark (LF_MapShareComponent) is a local PLACED_CUSTOM marker too, so it is told apart by being
	//! read-only: it sets CanBeRemovedByOwner false before its insert. Counting it as personal let shared
	//! marks fill the 20 cap (a full map refused dispatch and org marks) and uploaded them into the
	//! player's saved personal marks, so an org mark outlived leaving the org (review 2026-11-17).
	static bool IsPersonal(notnull SCR_MapMarkerBase marker)
	{
		if (!marker.CanBeRemovedByOwner())
			return false;

		SCR_EMapMarkerType type = marker.GetType();
		return type == SCR_EMapMarkerType.PLACED_CUSTOM || type == SCR_EMapMarkerType.PLACED_MILITARY;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRestoring()
	{
		return s_bRestoring;
	}

	//------------------------------------------------------------------------------------------------
	//! The local personal marks among `visible` and `disabled`, into `into`. The marker manager moves every
	//! static marker outside the shown map frame from its static list to its disabled list each map frame
	//! (SCR_MapMarkerManagerComponent.Update), so a mark off screen is only in the disabled list: counting,
	//! uploading or restoring from GetStaticMarkers alone misses it (the map proof pass, #2236).
	//! \return How many were added.
	static int CollectPersonal(array<SCR_MapMarkerBase> visible, array<SCR_MapMarkerBase> disabled, notnull array<SCR_MapMarkerBase> into)
	{
		return CollectPersonalFrom(visible, into) + CollectPersonalFrom(disabled, into);
	}

	//------------------------------------------------------------------------------------------------
	protected static int CollectPersonalFrom(array<SCR_MapMarkerBase> list, notnull array<SCR_MapMarkerBase> into)
	{
		if (!list)
			return 0;

		int added = 0;
		foreach (SCR_MapMarkerBase marker : list)
		{
			if (marker && marker.GetMarkerID() == -1 && IsPersonal(marker) && !into.Contains(marker))
			{
				into.Insert(marker);
				added++;
			}
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Every local personal mark the manager holds, on screen or off.
	static array<SCR_MapMarkerBase> PersonalOf(notnull SCR_MapMarkerManagerComponent manager)
	{
		array<SCR_MapMarkerBase> marks = {};
		CollectPersonal(manager.GetStaticMarkers(), manager.GetDisabledMarkers(), marks);
		return marks;
	}

	//------------------------------------------------------------------------------------------------
	//! Local personal marks the manager holds, on screen or off.
	static int CountLocal(notnull SCR_MapMarkerManagerComponent manager)
	{
		return PersonalOf(manager).Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ScheduleUpload()
	{
		if (s_bRestoring || !GetGame().GetPlayerController())
			return;

		if (s_bUploadQueued)
			GetGame().GetCallqueue().Remove(Upload);

		s_bUploadQueued = true;
		GetGame().GetCallqueue().CallLater(Upload, UPLOAD_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Upload()
	{
		s_bUploadQueued = false;
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!controller || !manager)
			return;

		if (s_iGeneration == 0)
		{
			LF_Debug.Info(LF_MapMarks.FEATURE, "mark upload skipped: the server's restore has not arrived yet");
			return;
		}

		array<int> ints = {};
		array<string> texts = {};
		foreach (SCR_MapMarkerBase marker : PersonalOf(manager))
		{
			int pos[2];
			marker.GetWorldPos(pos);
			LF_MapMarkRow row = LF_MapMarkRow.Make("", marker.GetType(), pos[0], pos[1], marker.GetIconEntry(), marker.GetColorEntry(), marker.GetRotation(), marker.GetMarkerConfigID(), marker.GetFlags(), marker.GetCustomText());
			LF_MapMarksWire.Append(row, ints, texts);
		}

		controller.LF_AskMapMarksSet(s_iGeneration, ints, texts);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the map's personal marks with the server's list (spawn, respawn, character switch).
	//! \param[in] generation The server's restore generation; later uploads echo it.
	static void Restore(int generation, array<int> ints, array<string> texts)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
		{
			LF_Debug.Warn(LF_MapMarks.FEATURE, "restore skipped: no marker manager on this machine");
			return;
		}

		array<ref LF_MapMarkRow> rows = LF_MapMarksWire.Read("", ints, texts);
		if (!rows)
		{
			LF_Debug.Error(LF_MapMarks.FEATURE, "restore skipped: the server's mark list is malformed");
			return;
		}

		// An upload queued before this restore holds the map as it was; the restore replaces that map.
		CancelUpload();
		s_iGeneration = generation;
		s_bRestoring = true;
		foreach (SCR_MapMarkerBase old : PersonalOf(manager))
		{
			// A local remove only searches the shown list: bring an off-screen mark back to it first.
			manager.SetStaticMarkerDisabled(old, false);
			manager.RemoveStaticMarker(old);
		}

		ChimeraWorld world = GetGame().GetWorld();
		foreach (LF_MapMarkRow row : rows)
		{
			SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
			marker.SetType(row.m_iType);
			marker.SetWorldPos(row.m_iPosX, row.m_iPosY);
			marker.SetIconEntry(row.m_iIcon);
			marker.SetColorEntry(row.m_iColor);
			marker.SetRotation(row.m_iRotation);
			marker.SetMarkerConfigID(row.m_iConfigId);
			marker.SetFlags(row.m_iFlags);
			marker.SetCustomText(row.m_sText);
			if (world)
				marker.SetTimestamp(world.GetServerTimestamp());

			manager.InsertStaticMarker(marker, true);
		}

		s_bRestoring = false;
		LF_Debug.Info(LF_MapMarks.FEATURE, string.Format("restored %1 personal mark(s)", rows.Count()));
	}
}

//------------------------------------------------------------------------------------------------
//! Server: the spawn restore again a moment later, for a player whose character key was not ready.
class LF_MapMarksSyncRetry
{
	static const int RETRIES = 5; // lf-fixed: join timing bound, not a gameplay knob
	static const int RETRY_MS = 2000; // lf-fixed: see RETRIES

	//------------------------------------------------------------------------------------------------
	static void Retry(int playerId, int retries)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller)
			controller.LF_SendMapMarksAll(retries);
	}
}

//------------------------------------------------------------------------------------------------
//! The personal-marks channel on the player's own controller: the client's upload and the
//! server's spawn restore. The server trusts nothing but the shape and the cap.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (!to || GetGame().GetPlayerController() != this)
			return;

		// A queued upload holds the previous character's marks: drop it before asking for the new list.
		LF_MapMarksClient.ForgetCharacter();

		// The owner pulls the (maybe new) character's marks on join, respawn and character switch.
		if (Replication.IsServer())
			RpcAsk_LF_MapMarksSync();
		else
			Rpc(RpcAsk_LF_MapMarksSync);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the whole personal mark list after a change.
	//! \param[in] generation The restore generation the list was edited under (LF_MapMarks.SetFromPlayer).
	void LF_AskMapMarksSet(int generation, array<int> ints, array<string> texts)
	{
		if (Replication.IsServer())
			RpcAsk_LF_MapMarksSet(generation, ints, texts);
		else
			Rpc(RpcAsk_LF_MapMarksSet, generation, ints, texts);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_MapMarksSync()
	{
		if (!LF_MapMarks.AllowAsk(GetPlayerId(), "map marks sync"))
			return;

		LF_SendMapMarksAll(LF_MapMarksSyncRetry.RETRIES);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_MapMarksSet(int generation, array<int> ints, array<string> texts)
	{
		int playerId = GetPlayerId();
		if (!LF_MapMarks.AllowAsk(playerId, "map marks save"))
			return;

		// A real client never holds more than the cap; a flood is refused before any row is built.
		if (texts && texts.Count() > LF_MapMarks.MAX_PER_CHARACTER * 2)
		{
			LF_Debug.Warn(LF_MapMarks.FEATURE, string.Format("mark list refused: player %1 sent %2 marks", playerId, texts.Count()));
			return;
		}

		string characterId = LF_Utils.GetPlayerUID(playerId);
		array<ref LF_MapMarkRow> rows = LF_MapMarksWire.Read(characterId, ints, texts);
		if (!rows)
		{
			LF_Debug.Warn(LF_MapMarks.FEATURE, string.Format("mark list refused: malformed list from player %1", playerId));
			return;
		}

		int kept = LF_MapMarks.SetFromPlayer(playerId, characterId, generation, rows);
		if (kept < 0)
			return; // stale generation, logged by SetFromPlayer

		LF_Debug.Info(LF_MapMarks.FEATURE, string.Format("player %1 saved %2 of %3 mark(s)", playerId, kept, rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the current character's marks to this player.
	//! \param[in] retries Tries left while the character key is not known yet.
	void LF_SendMapMarksAll(int retries = 0)
	{
		string characterId = LF_Utils.GetPlayerUID(GetPlayerId());
		if (characterId.IsEmpty())
		{
			if (retries > 0)
			{
				GetGame().GetCallqueue().CallLater(LF_MapMarksSyncRetry.Retry, LF_MapMarksSyncRetry.RETRY_MS, false, GetPlayerId(), retries - 1);
				return;
			}

			LF_Debug.Warn(LF_MapMarks.FEATURE, string.Format("restore skipped: player %1 has no character key", GetPlayerId()));
			return;
		}

		array<int> ints = {};
		array<string> texts = {};
		int count = LF_MapMarks.ExportCharacter(characterId, ints, texts);
		int generation = LF_MapMarks.IssueGeneration(GetPlayerId(), characterId);
		LF_Debug.Info(LF_MapMarks.FEATURE, string.Format("restore: %1 mark(s) to player %2, generation %3", count, GetPlayerId(), generation));
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_MapMarksSync(generation, ints, texts);
		else
			Rpc(RpcDo_LF_MapMarksSync, generation, ints, texts);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_MapMarksSync(int generation, array<int> ints, array<string> texts)
	{
		LF_MapMarksClient.Restore(generation, ints, texts);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the client upload (LF_StaticReset finds it by reflection).
class LF_MapMarksClient_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapMarksClient";
	}

	override void Apply()
	{
		LF_MapMarksClient.ResetStatics();
	}
}
