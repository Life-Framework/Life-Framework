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
//! PERSISTENCE. Export copies the remaining seconds. Apply writes them back.
//! The key is the player uid, never an EntityID.
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
}

//------------------------------------------------------------------------------------------------
class LF_Jail
{
	static const string CELL_ID = "Roadmap_Jail_Cell";
	static const string DOOR_ID = "Roadmap_Jail_Door";
	static const ResourceName DOOR_PREFAB = "{B597177F09247CE9}Prefabs/Structures/BuildingParts/Doors/Door_PrisonCell_E_A_Left_INT_ST_B.et";

	protected static ref map<string, ref LF_JailSentence> s_mSentences;
	protected static bool s_bCellPlaced;
	protected static vector s_vCellOrigin;
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
			EL_Debug.Error("Jail", "configure clamped: negative seconds per wanted");
			secondsPerWanted = 0;
		}

		if (maxSeconds < 0)
		{
			EL_Debug.Error("Jail", "configure clamped: negative max seconds");
			maxSeconds = 0;
		}

		if (wantedReduction < 0)
		{
			EL_Debug.Error("Jail", "configure clamped: negative wanted reduction");
			wantedReduction = 0;
		}

		if (zoneRadius < 0)
		{
			EL_Debug.Error("Jail", "configure clamped: negative zone radius");
			zoneRadius = 0;
		}

		s_iSecondsPerWanted = secondsPerWanted;
		s_iMaxSeconds = maxSeconds;
		s_iWantedReduction = wantedReduction;
		s_fZoneRadius = zoneRadius;
		EL_Debug.Log("Jail", string.Format("configured per=%1 max=%2 reduction=%3 radius=%4", secondsPerWanted, maxSeconds, wantedReduction, zoneRadius));
	}

	//------------------------------------------------------------------------------------------------
	static void SetCellOrigin(vector origin)
	{
		s_vCellOrigin = origin;
		s_bCellPlaced = true;
		EL_Debug.Log("Jail", string.Format("cell placed at %1 %2 %3", origin[0], origin[1], origin[2]));
	}

	//------------------------------------------------------------------------------------------------
	static void SetDoorTarget(string doorId)
	{
		if (doorId.IsEmpty())
		{
			EL_Debug.Error("Jail", "door target refused: empty id");
			return;
		}

		s_sDoorId = doorId;
		EL_Debug.Log("Jail", string.Format("door target %1", doorId));
	}

	//------------------------------------------------------------------------------------------------
	//! Points the sentence back at the named DebugWorld cell and jail door.
	static void SyncPlacedCell()
	{
		s_sDoorId = DOOR_ID;

		if (!GetGame() || !GetGame().GetWorld())
		{
			EL_Debug.Error("Jail", "placed cell skipped: no world");
			return;
		}

		IEntity cell = GetGame().GetWorld().FindEntityByName(CELL_ID);
		if (!cell)
		{
			EL_Debug.Error("Jail", "placed cell missing: " + CELL_ID);
			return;
		}

		SetCellOrigin(cell.GetOrigin());
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
		return EL_PoliceUtils.SentenceSeconds(wantedLevel, s_iSecondsPerWanted, s_iMaxSeconds);
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
			EL_Debug.Error("Jail", "arrest refused: officer or prisoner uid is empty");
			return false;
		}

		if (!s_bCellPlaced)
		{
			EL_Debug.Error("Jail", "arrest refused: no cell is placed");
			return false;
		}

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			EL_Debug.Error("Jail", "arrest refused: no account manager");
			return false;
		}

		EL_PlayerAccount officer = accounts.GetAccount(officerUid);
		if (!EL_PoliceUtils.IsOnDutyOfficer(officer))
		{
			EL_Debug.Log("Jail", string.Format("arrest refused: %1 is not an on-duty officer", officerUid));
			return false;
		}

		EL_PlayerAccount prisoner = accounts.GetAccount(prisonerUid);
		if (!prisoner)
		{
			EL_Debug.Error("Jail", string.Format("arrest refused: no account for %1", prisonerUid));
			return false;
		}

		if (prisoner.GetWantedLevel() <= 0)
		{
			EL_Debug.Log("Jail", string.Format("arrest refused: %1 is not wanted", prisonerUid));
			return false;
		}

		if (IsServing(prisonerUid))
		{
			EL_Debug.Log("Jail", string.Format("arrest refused: %1 is already serving", prisonerUid));
			return false;
		}

		int seconds = SentenceSeconds(prisoner.GetWantedLevel());
		LF_JailSentence sentence = new LF_JailSentence();
		sentence.m_sPlayerUid = prisonerUid;
		sentence.m_fRemaining = seconds;
		sentence.m_bServing = false;
		if (seconds > 0)
			sentence.m_bServing = true;
		Sentences().Set(prisonerUid, sentence);
		MoveToCell(prisonerUid, standIn);
		EL_Debug.Log("Jail", string.Format("sentence started %1 seconds=%2 wanted=%3", prisonerUid, seconds, prisoner.GetWantedLevel()));

		if (seconds <= 0)
		{
			EL_Debug.Log("Jail", string.Format("zero-duration release %1", prisonerUid));
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
			EL_Debug.Log("Jail", string.Format("early release refused: %1 is not serving", playerUid));
			return false;
		}

		EL_Debug.Log("Jail", string.Format("early release refused: %1 has time left", playerUid));
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
			EL_Debug.Error("Jail", "confine skipped: no cell is placed");
			return current;
		}

		if (vector.Distance(current, s_vCellOrigin) <= s_fZoneRadius)
			return current;

		EL_Debug.Log("Jail", string.Format("returned %1 to the cell", playerUid));
		return s_vCellOrigin;
	}

	//------------------------------------------------------------------------------------------------
	//! Confine check and sentence clock for placed cells. Tests advance time themselves.
	static void Tick(float elapsedSeconds)
	{
		array<string> ids = {};
		foreach (string playerUid, LF_JailSentence sentence : Sentences())
		{
			if (!sentence)
				continue;

			if (!sentence.m_bServing)
				continue;

			ids.Insert(playerUid);
		}

		foreach (string playerUid : ids)
		{
			Advance(playerUid, elapsedSeconds);
			if (!IsServing(playerUid))
				continue;

			PullBack(playerUid);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_JailRecord Export(string playerUid)
	{
		LF_JailSentence sentence = Sentences().Get(playerUid);
		if (!sentence)
		{
			EL_Debug.Error("Jail", string.Format("export refused: %1 has no sentence", playerUid));
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
			EL_Debug.Error("Jail", "apply refused: record is null");
			return false;
		}

		if (record.m_sPlayerUid.IsEmpty() || !record.IsValidFor(record.m_sPlayerUid))
		{
			EL_Debug.Error("Jail", "apply refused: record has no player uid");
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

		EL_Debug.Log("Jail", string.Format("apply-save %1 remaining=%2 serving=%3", record.m_sPlayerUid, record.m_fRemaining, serving));
		return true;
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
	protected static void MoveToCell(string prisonerUid, IEntity standIn)
	{
		IEntity target = standIn;
		if (!target)
		{
			EL_PlayerAccount account;
			target = EL_PoliceUtils.FindPlayerEntityByUid(prisonerUid, account);
		}

		if (!target)
		{
			EL_Debug.Log("Jail", string.Format("no body to move for %1", prisonerUid));
			return;
		}

		target.SetOrigin(s_vCellOrigin);
		EL_Debug.Log("Jail", string.Format("moved %1 to the cell", prisonerUid));
	}

	//------------------------------------------------------------------------------------------------
	protected static void PullBack(string playerUid)
	{
		EL_PlayerAccount account;
		IEntity entity = EL_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (!entity)
			return;

		vector resolved = ResolvePosition(playerUid, entity.GetOrigin());
		if (vector.Distance(resolved, entity.GetOrigin()) < 0.05)
			return;

		entity.SetOrigin(resolved);
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

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			EL_Debug.Error("Jail", "sentence complete skipped: no account manager");
			return;
		}

		EL_PlayerAccount account = accounts.GetAccount(playerUid);
		if (!account)
		{
			EL_Debug.Error("Jail", string.Format("sentence complete skipped wanted: no account for %1", playerUid));
			return;
		}

		int before = account.GetWantedLevel();
		account.IncreaseWantedLevel(-GetWantedReduction());
		accounts.SaveAndReleaseAccount(account);
		EL_Debug.Log("Jail", string.Format("sentence complete %1 wanted %2 -> %3", playerUid, before, account.GetWantedLevel()));
		NotifyReleased(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyReleased(string playerUid)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		EL_PlayerAccount account;
		IEntity entity = EL_PoliceUtils.FindPlayerEntityByUid(playerUid, account);
		if (!entity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
		if (playerId <= 0)
			return;

		EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", "#EL-Jail_Released", 3.0, EL_ENotificationType.SUCCESS);
	}
}
