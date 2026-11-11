// Ported from LNGCLifeRP_Public (MIT), author LNGCGenetic.
// Copyright (c) 2026 LNGCGenetic
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------------------------------
//! Jail sentence. An on-duty officer arrests a wanted player at the placed cell.
//! Sentence length is config, scaled by wanted level. The door is the shared
//! breach target: a locked door returns a prisoner who leaves the volume, and an
//! open door does not. Leaving does not clear the sentence. Expiry reduces wanted
//! by the configured amount. Early release is refused.
//!
//! ONLINE TIME. The sentence counts down only while the prisoner is connected (Tick reads the
//! connected players). An offline prisoner keeps the remaining time they had at logoff.
//!
//! IN THE WORLD. Sentencing hands the prisoner a sentence slip (LF_JailSlip) and the slip shows
//! the online minutes left. Release opens the cell door (the vanilla door motion and its sound),
//! places the prisoner outside the cell, stamps the slip, and then shows a short toast. A
//! prisoner seated in a vehicle is dismounted straight to the target; the vehicle stays put.
//!
//! PERSISTENCE. Export copies the remaining seconds. Apply writes them back.
//! The key is the player uid, never an EntityID. ExportAll / ApplyAll are the
//! "jail" payload of LF_PersistenceComponentSerializer (save version 5+), so a
//! sentence survives a server restart.
//------------------------------------------------------------------------------------------------
class LF_JailRecord : Managed
{
	static const int VERSION = 1;

	string m_sPlayerUid;
	float m_fRemaining;
	bool m_bServing;

	//------------------------------------------------------------------------------------------------
	static LF_JailRecord Create(string playerUid, float remaining, bool serving)
	{
		LF_JailRecord record();
		record.m_sPlayerUid = playerUid;
		record.m_fRemaining = remaining;
		record.m_bServing = serving;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	bool IsValidFor(string playerUid)
	{
		if (m_sPlayerUid.IsEmpty() || playerUid.IsEmpty())
			return false;

		return m_sPlayerUid == playerUid;
	}
}

//------------------------------------------------------------------------------------------------
class LF_JailSentence : Managed
{
	string m_sPlayerUid;
	float m_fRemaining;
	bool m_bServing;

	//! Session-only: never saved. The body moved to the cell, for a body with no player (a test
	//! stand-in). A connected player is always resolved through their uid.
	EntityID m_BodyId = EntityID.INVALID;
	bool m_bPaused;
	int m_iReleases;
	//! Session-only: set when the prisoner walked out through an open door, so the escape is
	//! reported once. Cleared when they are back inside the volume.
	bool m_bEscaped;
}

//------------------------------------------------------------------------------------------------
//! How a body is moved to the cell or the release point.
enum LF_EJailMove
{
	DIRECT,
	TELEPORT_PLAYER,
	DISMOUNT
}

//------------------------------------------------------------------------------------------------
//! Pure move rule, for LOGIC tests. A seated body is always dismounted: TeleportPlayer moves
//! the vehicle of a seated player ("When in a vehicle, teleport the vehicle instead",
//! Scripts/Game/Global/Functions.c), and a server SetOrigin leaves the body in the seat.
class LF_JailMoveRules
{
	static LF_EJailMove Pick(bool seated, bool isPlayer)
	{
		if (seated)
			return LF_EJailMove.DISMOUNT;

		if (isPlayer)
			return LF_EJailMove.TELEPORT_PLAYER;

		return LF_EJailMove.DIRECT;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Jail
{
	static const string CELL_ID = "Roadmap_Jail_Cell";
	static const string DOOR_ID = "Roadmap_Jail_Door";
	static const ResourceName DOOR_PREFAB = "{B597177F09247CE9}Prefabs/Structures/BuildingParts/Doors/Door_PrisonCell_E_A_Left_INT_ST_B.et";

	static const int RELEASE_DOOR_OPEN_MS = 5000;
	static const int RELEASE_TOAST_DELAY_MS = 1500;
	static const int DISMOUNT_RETRY_MS = 250;
	static const int DISMOUNT_MAX_TRIES = 20;

	protected static ref map<string, ref LF_JailSentence> s_mSentences;
	protected static bool s_bCellPlaced;
	protected static vector s_vCellOrigin;
	protected static vector s_vReleaseOffset;
	protected static int s_iDoorOpenings;
	protected static string s_sDoorId;
	protected static int s_iSecondsPerWanted;
	protected static int s_iMaxSeconds;
	protected static int s_iWantedReduction;
	protected static float s_fZoneRadius;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_JailSentence> Sentences()
	{
		if (!s_mSentences)
			s_mSentences = new map<string, ref LF_JailSentence>();

		return s_mSentences;
	}

	//------------------------------------------------------------------------------------------------
	static void Configure(int secondsPerWanted, int maxSeconds, int wantedReduction, float zoneRadius)
	{
		if (secondsPerWanted < 0)
		{
			LF_Debug.Error("Jail", "configure clamped: negative seconds per wanted");
			secondsPerWanted = 0;
		}

		if (maxSeconds < 0)
		{
			LF_Debug.Error("Jail", "configure clamped: negative max seconds");
			maxSeconds = 0;
		}

		if (wantedReduction < 0)
		{
			LF_Debug.Error("Jail", "configure clamped: negative wanted reduction");
			wantedReduction = 0;
		}

		if (zoneRadius < 0)
		{
			LF_Debug.Error("Jail", "configure clamped: negative zone radius");
			zoneRadius = 0;
		}

		s_iSecondsPerWanted = secondsPerWanted;
		s_iMaxSeconds = maxSeconds;
		s_iWantedReduction = wantedReduction;
		s_fZoneRadius = zoneRadius;
		LF_Debug.Log("Jail", string.Format("configured per=%1 max=%2 reduction=%3 radius=%4", secondsPerWanted, maxSeconds, wantedReduction, zoneRadius));
	}

	//------------------------------------------------------------------------------------------------
	static void SetCellOrigin(vector origin)
	{
		s_vCellOrigin = origin;
		s_bCellPlaced = true;
		LF_Debug.Log("Jail", string.Format("cell placed at %1 %2 %3", origin[0], origin[1], origin[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! Where a released prisoner stands, relative to the cell origin in world axes. Zero keeps
	//! them on the cell origin.
	static void SetReleaseOffset(vector worldOffset)
	{
		s_vReleaseOffset = worldOffset;
		LF_Debug.Log("Jail", string.Format("release offset %1 %2 %3", worldOffset[0], worldOffset[1], worldOffset[2]));
	}

	//------------------------------------------------------------------------------------------------
	static vector GetReleasePoint()
	{
		return s_vCellOrigin + s_vReleaseOffset;
	}

	//------------------------------------------------------------------------------------------------
	static void SetDoorTarget(string doorId)
	{
		if (doorId.IsEmpty())
		{
			LF_Debug.Error("Jail", "door target refused: empty id");
			return;
		}

		s_sDoorId = doorId;
		LF_Debug.Log("Jail", string.Format("door target %1", doorId));
	}

	//------------------------------------------------------------------------------------------------
	//! Points the sentence back at the named DebugWorld cell and jail door.
	static void SyncPlacedCell()
	{
		s_sDoorId = DOOR_ID;

		if (!GetGame() || !GetGame().GetWorld())
		{
			LF_Debug.Error("Jail", "placed cell skipped: no world");
			return;
		}

		IEntity cell = GetGame().GetWorld().FindEntityByName(CELL_ID);
		if (!cell)
		{
			LF_Debug.Error("Jail", "placed cell missing: " + CELL_ID);
			return;
		}

		SetCellOrigin(cell.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Drops sentences, the cell, the door, and the sentence config. Called on game-mode start/end
	//! (LF_StaticReset) so a world reload does not inherit the previous world's jail.
	static void ResetStatics()
	{
		s_mSentences = null;
		s_bCellPlaced = false;
		s_vCellOrigin = "0 0 0";
		s_vReleaseOffset = "0 0 0";
		s_iDoorOpenings = 0;
		s_sDoorId = "";
		s_iSecondsPerWanted = 0;
		s_iMaxSeconds = 0;
		s_iWantedReduction = 0;
		s_fZoneRadius = 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasCell()
	{
		return s_bCellPlaced;
	}

	//------------------------------------------------------------------------------------------------
	static vector GetCellOrigin()
	{
		return s_vCellOrigin;
	}

	//------------------------------------------------------------------------------------------------
	static int SentenceSeconds(int wantedLevel)
	{
		return LF_PoliceUtils.SentenceSeconds(wantedLevel, s_iSecondsPerWanted, s_iMaxSeconds);
	}

	//------------------------------------------------------------------------------------------------
	static int GetWantedReduction()
	{
		if (s_iWantedReduction < 0)
			return 0;

		return s_iWantedReduction;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a sentence for a wanted prisoner and moves standIn (or their connected
	//! body) to the placed cell. A zero-length config releases in the same call.
	//! \return False when the officer is not on duty, the prisoner is clean, the cell
	//! is missing, or the prisoner is already serving.
	static bool Arrest(string officerUid, string prisonerUid, IEntity standIn)
	{
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			LF_Debug.Error("Jail", "arrest refused: officer or prisoner uid is empty");
			return false;
		}

		if (!s_bCellPlaced)
		{
			LF_Debug.Error("Jail", "arrest refused: no cell is placed");
			return false;
		}

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			LF_Debug.Error("Jail", "arrest refused: no account manager");
			return false;
		}

		LF_PlayerAccount officer = accounts.GetAccount(officerUid);
		if (!LF_PoliceUtils.IsOnDutyOfficer(officer))
		{
			LF_Debug.Log("Jail", string.Format("arrest refused: %1 is not an on-duty officer", officerUid));
			return false;
		}

		// One server gate for every arrest path (action, menu, escort to the cell).
		if (!LF_PoliceRanks.CanArrestRank(officer.GetPoliceRank()))
		{
			LF_Debug.Warn("Jail", string.Format("arrest refused: rank %1 is below the arrest rank %2", officer.GetPoliceRank(), LF_PoliceConfig.ArrestMinRank()));
			return false;
		}

		LF_PlayerAccount prisoner = accounts.GetAccount(prisonerUid);
		if (!prisoner)
		{
			LF_Debug.Error("Jail", string.Format("arrest refused: no account for %1", prisonerUid));
			return false;
		}

		if (prisoner.GetWantedLevel() <= 0)
		{
			LF_Debug.Log("Jail", string.Format("arrest refused: %1 is not wanted", prisonerUid));
			return false;
		}

		if (IsServing(prisonerUid))
		{
			LF_Debug.Log("Jail", string.Format("arrest refused: %1 is already serving", prisonerUid));
			return false;
		}

		return Imprison(prisonerUid, SentenceSeconds(prisoner.GetWantedLevel()), prisoner.GetWantedLevel(), standIn);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a sentence of `seconds` and moves standIn (or the prisoner's connected body) to the
	//! placed cell. Arrest checks who may do this and for how long; the playtest board
	//! (LF_DebugJailMe) calls it directly. Returns false when no cell is placed, the uid is empty
	//! or the prisoner is already serving.
	static bool Imprison(string prisonerUid, int seconds, int wantedLevel, IEntity standIn)
	{
		if (prisonerUid.IsEmpty() || !s_bCellPlaced || IsServing(prisonerUid))
		{
			LF_Debug.Log("Jail", string.Format("imprison refused: uid=%1 cell=%2", prisonerUid, s_bCellPlaced));
			return false;
		}

		LF_JailSentence sentence = new LF_JailSentence();
		sentence.m_sPlayerUid = prisonerUid;
		sentence.m_fRemaining = seconds;
		sentence.m_bServing = false;
		if (seconds > 0)
			sentence.m_bServing = true;
		Sentences().Set(prisonerUid, sentence);
		IEntity body = MoveToCell(prisonerUid, standIn);
		if (body)
			sentence.m_BodyId = body.GetID();

		LF_Debug.Log("Jail", string.Format("sentence started %1 seconds=%2 wanted=%3", prisonerUid, seconds, wantedLevel));
		LF_EventBus.Publish(LF_OnArrested.Create(prisonerUid, seconds, wantedLevel));
		if (seconds > 0)
			HandSlip(prisonerUid, body);

		if (seconds <= 0)
		{
			LF_Debug.Log("Jail", string.Format("zero-duration release %1", prisonerUid));
			Complete(prisonerUid);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Always refuses while a sentence still has time left.
	static bool ReleaseEarly(string playerUid)
	{
		if (!IsServing(playerUid))
		{
			LF_Debug.Log("Jail", string.Format("early release refused: %1 is not serving", playerUid));
			return false;
		}

		LF_Debug.Log("Jail", string.Format("early release refused: %1 has time left", playerUid));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a running sentence forward. At zero, wanted drops and the prisoner is free.
	//! \return Remaining seconds after the step. Zero when the sentence is over.
	static float Advance(string playerUid, float elapsedSeconds)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence || !sentence.m_bServing)
			return 0;

		if (elapsedSeconds < 0)
			elapsedSeconds = 0;

		sentence.m_fRemaining = sentence.m_fRemaining - elapsedSeconds;
		if (sentence.m_fRemaining <= 0)
		{
			sentence.m_fRemaining = 0;
			Complete(playerUid);
			return 0;
		}

		return sentence.m_fRemaining;
	}

	//------------------------------------------------------------------------------------------------
	static float GetRemaining(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence || !sentence.m_bServing)
			return 0;

		return sentence.m_fRemaining;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsServing(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence)
			return false;

		return sentence.m_bServing && sentence.m_fRemaining > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this prisoner still has time and the jail door is locked.
	static bool Confines(string playerUid)
	{
		if (!IsServing(playerUid))
			return false;

		return DoorIsLocked();
	}

	//------------------------------------------------------------------------------------------------
	//! Locked door: a position outside the volume becomes the cell origin.
	//! Open door, or no time left: the position is unchanged. Remaining time is not touched.
	static vector ResolvePosition(string playerUid, vector current)
	{
		if (!Confines(playerUid))
			return current;

		if (!s_bCellPlaced)
		{
			LF_Debug.Error("Jail", "confine skipped: no cell is placed");
			return current;
		}

		if (vector.Distance(current, s_vCellOrigin) <= s_fZoneRadius)
			return current;

		LF_Debug.Log("Jail", string.Format("returned %1 to the cell", playerUid));
		return s_vCellOrigin;
	}

	//------------------------------------------------------------------------------------------------
	//! Confine check and sentence clock for placed cells. Only connected prisoners serve time.
	static void Tick(float elapsedSeconds)
	{
		array<string> online = {};
		CollectOnlineUids(online);
		TickOnline(elapsedSeconds, online);
	}

	//------------------------------------------------------------------------------------------------
	//! The sentence clock with the connected uids given. A prisoner not in onlineUids is paused:
	//! the remaining time does not move. Public so a LOGIC test drives both sides.
	static void TickOnline(float elapsedSeconds, notnull array<string> onlineUids)
	{
		array<string> ids = {};
		foreach (string playerUid, LF_JailSentence sentence : Sentences())
		{
			if (!sentence)
				continue;

			if (!sentence.m_bServing)
				continue;

			bool online = onlineUids.Find(playerUid) >= 0;
			if (online == sentence.m_bPaused)
			{
				sentence.m_bPaused = !online;
				if (online)
					LF_Debug.Info("Jail", string.Format("sentence resumed %1 remaining=%2", playerUid, sentence.m_fRemaining));
				else
					LF_Debug.Info("Jail", string.Format("sentence paused %1 (offline) remaining=%2", playerUid, sentence.m_fRemaining));
			}

			if (online)
				ids.Insert(playerUid);
		}

		foreach (string playerUid : ids)
		{
			Advance(playerUid, elapsedSeconds);
			if (!IsServing(playerUid))
				continue;

			PullBack(playerUid);
			CheckEscape(playerUid);
			RefreshSlip(playerUid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The uid of every connected player. A disconnected player is not listed.
	static void CollectOnlineUids(notnull array<string> outUids)
	{
		outUids.Clear();
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string uid = LF_Utils.GetPlayerUID(playerId);
			if (!uid.IsEmpty())
				outUids.Insert(uid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A respawned or rejoined prisoner with time left goes back to the cell with their slip.
	//! Called on every spawn path (LF_SpawnLogic).
	static void OnPlayerSpawned(string playerUid, IEntity body)
	{
		if (playerUid.IsEmpty() || !body || !IsServing(playerUid))
			return;

		if (!s_bCellPlaced)
		{
			LF_Debug.Error("Jail", string.Format("spawned prisoner %1 not returned: no cell is placed", playerUid));
			return;
		}

		MoveBody(body, s_vCellOrigin);
		HandSlip(playerUid, body);
		LF_Debug.Info("Jail", string.Format("spawned prisoner %1 returned to the cell remaining=%2", playerUid, GetRemaining(playerUid)));
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many times this prisoner's sentence has released (door, placement, slip stamp).
	static int GetReleaseCount(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence)
			return 0;

		return sentence.m_iReleases;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many times a release opened the placed cell door this session.
	static int GetDoorOpenings()
	{
		return s_iDoorOpenings;
	}

	//------------------------------------------------------------------------------------------------
	static LF_JailRecord Export(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence)
		{
			LF_Debug.Error("Jail", string.Format("export refused: %1 has no sentence", playerUid));
			return null;
		}

		return LF_JailRecord.Create(playerUid, sentence.m_fRemaining, sentence.m_bServing);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a saved remaining time back onto the prisoner. Idempotent.
	//! A record with an empty uid is rejected. A missing live sentence is created.
	static bool Apply(LF_JailRecord record)
	{
		if (!record)
		{
			LF_Debug.Error("Jail", "apply refused: record is null");
			return false;
		}

		if (record.m_sPlayerUid.IsEmpty() || !record.IsValidFor(record.m_sPlayerUid))
		{
			LF_Debug.Error("Jail", "apply refused: record has no player uid");
			return false;
		}

		LF_JailSentence sentence = Sentences().Get(record.m_sPlayerUid);
		if (!sentence)
		{
			sentence = new LF_JailSentence();
			sentence.m_sPlayerUid = record.m_sPlayerUid;
			Sentences().Set(record.m_sPlayerUid, sentence);
		}

		sentence.m_fRemaining = record.m_fRemaining;
		sentence.m_bServing = record.m_bServing;
		int serving = 0;
		if (record.m_bServing)
			serving = 1;

		LF_Debug.Log("Jail", string.Format("apply-save %1 remaining=%2 serving=%3", record.m_sPlayerUid, record.m_fRemaining, serving));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every sentence that still has time left, for the game-mode serializer.
	//! Finished sentences are not saved; a restart must not re-jail a released prisoner.
	//! \return Records (never null), one per serving prisoner.
	static array<ref LF_JailRecord> ExportAll()
	{
		array<ref LF_JailRecord> records = new array<ref LF_JailRecord>();
		foreach (string playerUid, LF_JailSentence sentence : Sentences())
		{
			if (!sentence || playerUid.IsEmpty())
				continue;

			if (!sentence.m_bServing || sentence.m_fRemaining <= 0)
				continue;

			records.Insert(LF_JailRecord.Create(playerUid, sentence.m_fRemaining, sentence.m_bServing));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the save is a complete snapshot of serving sentences. Live sentences
	//! absent from it are dropped and every record is applied, so a second pass ends on the
	//! same state (idempotent on a live session). A bad record is skipped and logged.
	static void ApplyAll(notnull array<ref LF_JailRecord> records)
	{
		Sentences().Clear();

		int applied = 0;
		foreach (LF_JailRecord record : records)
		{
			if (Apply(record))
				applied++;
		}

		LF_Debug.Log("Jail", string.Format("apply-save restored %1 sentence(s)", applied));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool DoorIsLocked()
	{
		string doorId = s_sDoorId;
		if (doorId.IsEmpty())
			doorId = DOOR_ID;

		return LF_Breach.IsLocked(doorId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The body that was moved, or null when the prisoner has none.
	protected static IEntity MoveToCell(string prisonerUid, IEntity standIn)
	{
		IEntity target = standIn;
		if (!target)
		{
			LF_PlayerAccount account;
			target = LF_PoliceUtils.FindPlayerEntityByUid(prisonerUid, account);
		}

		if (!target)
		{
			LF_Debug.Log("Jail", string.Format("no body to move for %1", prisonerUid));
			return null;
		}

		MoveBody(target, s_vCellOrigin);
		LF_Debug.Log("Jail", string.Format("moved %1 to the cell", prisonerUid));
		return target;
	}

	//------------------------------------------------------------------------------------------------
	//! The connected body of a prisoner, else the session body recorded at arrest (a stand-in).
	protected static IEntity FindBody(string playerUid)
	{
		LF_PlayerAccount account;
		IEntity body = LF_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (body)
			return body;

		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence || sentence.m_BodyId == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(sentence.m_BodyId);
	}

	//------------------------------------------------------------------------------------------------
	//! A player's character is owned by their client, which simulates its movement: a server-only
	//! SetOrigin is overwritten by the next owner update and the prisoner snaps back. A connected
	//! player is moved through SCR_Global.TeleportPlayer, the vanilla server-side player teleport
	//! that reaches the owning client. A body with no player (a test stand-in) is set directly.
	//! A seated body is dismounted first: TeleportPlayer moves the vehicle when the player sits
	//! in one ("When in a vehicle, teleport the vehicle instead", Scripts/Game/Global/Functions.c).
	static void MoveBody(IEntity body, vector position)
	{
		if (!body)
			return;

		int playerId = 0;
		if (GetGame() && GetGame().GetPlayerManager())
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(body);

		LF_EJailMove how = LF_JailMoveRules.Pick(IsSeated(body), playerId > 0);
		if (how == LF_EJailMove.DISMOUNT)
		{
			DismountTo(body, position);
			return;
		}

		if (how == LF_EJailMove.TELEPORT_PLAYER)
		{
			bool teleported = SCR_Global.TeleportPlayer(playerId, position);
			// On a dedicated server TeleportPlayer alone moved the body on the server only: the
			// owning client's next movement update put the prisoner back where they were arrested
			// (M1 RC QA: "moved ... to the cell" logged, the client never left the garage). The
			// owner has to make the move itself.
			if (AskOwnerToMove(body, position))
				return;

			if (teleported)
				return;

			LF_Debug.Warn("Jail", string.Format("player teleport refused for player %1; setting the origin on the server", playerId));
		}

		body.SetOrigin(position);
	}

	//------------------------------------------------------------------------------------------------
	//! A body a remote client owns is moved by that client, through the vanilla Game Master path
	//! (SCR_EditableCharacterComponent.SetTransform asks the owner). False for a body this machine
	//! owns (a listen host, a test stand-in): the caller's server move is enough there.
	protected static bool AskOwnerToMove(IEntity body, vector position)
	{
		RplComponent rpl = RplComponent.Cast(body.FindComponent(RplComponent));
		if (!rpl || rpl.IsOwner())
			return false;

		SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(body.FindComponent(SCR_EditableCharacterComponent));
		if (!editable)
		{
			LF_Debug.Error("Jail", "prisoner not moved on their client: no SCR_EditableCharacterComponent to ask the owner");
			return false;
		}

		vector transform[4];
		body.GetWorldTransform(transform);
		transform[3] = position;
		editable.SetTransform(transform);
		LF_Debug.Info("Jail", string.Format("asked the owning client to move the prisoner to %1", position));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A seated body leaves the vehicle straight at position (GetOutVehicle_NoDoor). The owner
	//! does it: a server-owned body here, a client-owned player through the vanilla Game Master
	//! path SCR_EditableCharacterComponent.SetTransform, which asks the owning client.
	//! \return False when the body is not seated (the caller moves it). True when the dismount
	//! was started or refused; the vehicle is never moved.
	protected static bool IsSeated(IEntity body)
	{
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(body.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;

		return access.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool DismountTo(IEntity body, vector position)
	{
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(body.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
			return false;

		vector transform[4];
		body.GetWorldTransform(transform);
		transform[3] = position;

		RplComponent rpl = RplComponent.Cast(body.FindComponent(RplComponent));
		if (!rpl || rpl.IsOwner())
		{
			access.GetOutVehicle_NoDoor(transform, false, true);
			LF_Debug.Info("Jail", "seated prisoner dismounted on the server; the vehicle stays put");
		}
		else
		{
			SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(body.FindComponent(SCR_EditableCharacterComponent));
			if (!editable)
			{
				LF_Debug.Error("Jail", "seated prisoner not moved: no SCR_EditableCharacterComponent to ask the owner to dismount");
				return true;
			}

			editable.SetTransform(transform);
			LF_Debug.Info("Jail", "seated prisoner asked to dismount at the target; the vehicle stays put");
		}

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(FinishDismount, DISMOUNT_RETRY_MS, false, body, position, 1);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the dismount, then makes sure the body stands at position. A body still seated
	//! after DISMOUNT_MAX_TRIES is left where it is (logged) rather than moving the vehicle.
	static void FinishDismount(IEntity body, vector position, int attempt)
	{
		if (!body)
		{
			LF_Debug.Warn("Jail", "dismount check skipped: the body is gone");
			return;
		}

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(body.FindComponent(CompartmentAccessComponent));
		if (access && access.IsInCompartment())
		{
			if (attempt >= DISMOUNT_MAX_TRIES)
			{
				LF_Debug.Error("Jail", "prisoner still seated after the dismount request; not moved so the vehicle stays put");
				return;
			}

			GetGame().GetCallqueue().CallLater(FinishDismount, DISMOUNT_RETRY_MS, false, body, position, attempt + 1);
			return;
		}

		if (vector.Distance(body.GetOrigin(), position) > 1.0)
			MoveBody(body, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Prints the sentence on the slip the prisoner carries, or hands them one.
	protected static void HandSlip(string playerUid, IEntity body)
	{
		if (!body)
		{
			LF_Debug.Warn("Jail", string.Format("no slip for %1: no body", playerUid));
			return;
		}

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return;

		LF_PlayerAccount account = accounts.GetAccount(playerUid);
		if (!account)
			return;

		string name = playerUid;
		LF_PlayerCharacter character = account.GetActiveCharacter();
		if (character)
			name = character.GetFirstName() + " " + character.GetLastName();

		if (!LF_JailSlip.Grant(body, playerUid, name, account.GetWantedLevel(), GetRemaining(playerUid)))
			return;

		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(body);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", "#LF-Jail_SlipHanded", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RefreshSlip(string playerUid)
	{
		IEntity body = FindBody(playerUid);
		if (!body)
			return;

		LF_JailSlipComponent slip = LF_JailSlip.FindFor(body, playerUid);
		if (slip && slip.GetHolderUid() == playerUid)
			slip.Refresh(GetRemaining(playerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Release in the world: the cell door swings open (its vanilla door sound), the prisoner
	//! stands outside the cell, the slip is stamped, and the toast follows.
	protected static void Release(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (sentence)
			sentence.m_iReleases++;

		OpenCellDoor();

		IEntity body = FindBody(playerUid);
		if (body)
		{
			MoveBody(body, GetReleasePoint());
			LF_JailSlipComponent slip = LF_JailSlip.FindFor(body, playerUid);
			if (slip && slip.GetHolderUid() == playerUid)
				slip.Stamp();
		}
		else
		{
			LF_Debug.Info("Jail", string.Format("released %1 with no body in the world", playerUid));
		}

		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(NotifyReleased, RELEASE_TOAST_DELAY_MS, false, playerUid);

		LF_Debug.Info("Jail", string.Format("released %1 outside the cell", playerUid));
	}

	//------------------------------------------------------------------------------------------------
	protected static DoorComponent FindCellDoor()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		string doorId = s_sDoorId;
		if (doorId.IsEmpty())
			doorId = DOOR_ID;

		IEntity door = GetGame().GetWorld().FindEntityByName(doorId);
		if (!door)
			return null;

		return DoorComponent.Cast(door.FindComponent(DoorComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenCellDoor()
	{
		DoorComponent door = FindCellDoor();
		if (!door)
		{
			LF_Debug.Warn("Jail", "release door not opened: no placed door with a DoorComponent");
			return;
		}

		door.SetControlValue(1, RplId.Invalid());
		s_iDoorOpenings++;
		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(CloseCellDoor);
			GetGame().GetCallqueue().CallLater(CloseCellDoor, RELEASE_DOOR_OPEN_MS, false);
		}

		LF_Debug.Info("Jail", "release door opened");
	}

	//------------------------------------------------------------------------------------------------
	static void CloseCellDoor()
	{
		DoorComponent door = FindCellDoor();
		if (!door)
			return;

		door.SetControlValue(0, RplId.Invalid());
		LF_Debug.Info("Jail", "release door closed");
	}

	//------------------------------------------------------------------------------------------------
	protected static void PullBack(string playerUid)
	{
		LF_PlayerAccount account;
		IEntity entity = LF_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (!entity)
			return;

		vector resolved = ResolvePosition(playerUid, entity.GetOrigin());
		if (vector.Distance(resolved, entity.GetOrigin()) < 0.05)
			return;

		MoveBody(entity, resolved);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a serving prisoner standing outside the cell volume while the door is open has
	//! escaped. The escape is reported once as a crime (dispatch and the bus) and the sentence
	//! is left alone, so a recapture continues it. Back inside the volume re-arms the check.
	protected static void CheckEscape(string playerUid)
	{
		LF_PlayerAccount account;
		IEntity entity = LF_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (!entity)
			return;

		NoteEscape(playerUid, entity.GetOrigin(), entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The escape rule with the position given. Public so a LOGIC test drives it.
	//! eturn True when this call reported a new escape.
	static bool NoteEscape(string playerUid, vector current, IEntity body)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence || !IsServing(playerUid) || !s_bCellPlaced)
			return false;

		if (vector.Distance(current, s_vCellOrigin) <= s_fZoneRadius)
		{
			sentence.m_bEscaped = false;
			return false;
		}

		if (DoorIsLocked() || sentence.m_bEscaped)
			return false;

		sentence.m_bEscaped = true;
		LF_Debug.Warn("Jail", string.Format("%1 escaped the cell through the open door remaining=%2", playerUid, sentence.m_fRemaining));
		LF_Dispatch.Report(playerUid, body, LF_WantedReports.OFFENCE_JAILBREAK, vector.Zero);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Complete(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (sentence)
		{
			sentence.m_bServing = false;
			sentence.m_fRemaining = 0;
		}

		Release(playerUid);

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			LF_Debug.Error("Jail", "sentence complete skipped wanted: no account manager");
			return;
		}

		LF_PlayerAccount account = accounts.GetAccount(playerUid);
		if (!account)
		{
			LF_Debug.Error("Jail", string.Format("sentence complete skipped wanted: no account for %1", playerUid));
			return;
		}

		int before = account.GetWantedLevel();
		account.IncreaseWantedLevel(-GetWantedReduction());
		accounts.SaveAndReleaseAccount(account);
		LF_Debug.Log("Jail", string.Format("sentence complete %1 wanted %2 -> %3", playerUid, before, account.GetWantedLevel()));
	}

	//------------------------------------------------------------------------------------------------
	static void NotifyReleased(string playerUid)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		LF_PlayerAccount account;
		IEntity entity = LF_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", "#EL-Jail_Released", 3.0, LF_ENotificationType.SUCCESS);
	}
}
