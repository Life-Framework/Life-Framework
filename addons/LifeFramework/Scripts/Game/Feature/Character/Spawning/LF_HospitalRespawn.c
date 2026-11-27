// LF_HospitalRespawn.c — you wake up in hospital (decision 2026-11-08, M1-26).
//
// Server-authoritative. A death marks the player's uid with a pending wake. The next spawn of
// that player (the death-screen Respawn, or a reconnect while dead) takes the pending wake once:
// the body is placed at a free hospital bed, a paper treatment bill is written (LF_TreatmentBills,
// paid in cash at the hospital desk, never taken from the bank) and goes into the
// new body's pockets. The owning client then plays the base game's get-out-of-bed animation and
// reads the bill. The client never sends an amount or a bed.
//
// The pending wake is session state keyed by uid: a server restart between a death and the
// respawn forgets it (the player spawns at the plaza with no bill).

class LF_HospitalRespawn
{
	//! A living character this close (metres, horizontal) to a bed's wake point occupies it.
	static const float BED_CLEARANCE = 1.0;
	//! Delay before the owner is told to wake: ownership of the new body has to reach the client.
	// lf-fixed: UI sequencing delay (let the engine settle before the next step), not a balance value
	static const int WAKE_NOTIFY_DELAY_MS = 1000;

	protected static ref map<string, bool> s_mPendingWake;
	protected static ref map<int, bool> s_mWokeInBed;
	protected static int s_iBedCursor;

	//------------------------------------------------------------------------------------------------
	//! Server: a player died. The next spawn of this uid wakes in hospital and pays the bill once,
	//! however many death reports arrive before it.
	static void MarkDied(string uid)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Respawn", "death not recorded: empty uid");
			return;
		}

		if (!s_mPendingWake)
			s_mPendingWake = new map<string, bool>();

		s_mPendingWake.Set(uid, true);
		LF_Debug.Info("Respawn", string.Format("death recorded for %1: next spawn wakes in hospital", uid));
	}

	//------------------------------------------------------------------------------------------------
	static bool HasPendingWake(string uid)
	{
		if (!s_mPendingWake || uid.IsEmpty())
			return false;

		return s_mPendingWake.Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Consumes the pending wake.
	//! \return True when there was one.
	protected static bool TakePendingWake(string uid)
	{
		if (!HasPendingWake(uid))
			return false;

		s_mPendingWake.Remove(uid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Clears all session state. Tests and game mode teardown.
	static void Reset()
	{
		s_mPendingWake = null;
		s_mWokeInBed = null;
		s_iBedCursor = 0;
		LF_NewLifeRule.Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the pending wake for a uid and writes the treatment bill on paper (not the bank).
	//! \return The bill, or null when there was no death to bill for or the bill is off (amount 0).
	static LF_TreatmentBillRecord BillOnWake(string uid, int amount, string issued)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Respawn", "bill refused: empty uid");
			return null;
		}

		if (!TakePendingWake(uid))
			return null;

		LF_TreatmentBillRecord record = LF_TreatmentBills.Issue(uid, LF_PlayerInteraction.DisplayName(uid), "", "", amount, 0, issued);
		if (record)
			LF_Debug.Info("Respawn", string.Format("hospital bill #%1 of %2 written for %3", record.m_iNumber, amount, uid));

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks a bed: the first free one from the cursor on, wrapping. A full ward gives the cursor bed.
	//! \param beds Wake position of each bed.
	//! \param occupants Positions of living characters.
	//! \param cursor Rotating start index (any non-negative value; wrapped here).
	//! \return Index into beds, or -1 when there are none.
	static int PickBed(array<vector> beds, array<vector> occupants, int cursor)
	{
		if (!beds || beds.IsEmpty())
			return -1;

		int count = beds.Count();
		int start = cursor % count;
		if (start < 0)
			start += count;

		for (int i = 0; i < count; i++)
		{
			int index = (start + i) % count;
			if (!IsOccupied(beds[index], occupants))
				return index;
		}

		return start;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsOccupied(vector bed, array<vector> occupants)
	{
		if (!occupants)
			return false;

		foreach (vector occupant : occupants)
		{
			if (vector.DistanceSqXZ(bed, occupant) < BED_CLEARANCE * BED_CLEARANCE && Math.AbsFloat(bed[1] - occupant[1]) < 2)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.GetCreationPosition: a player with a pending wake spawns at a bed.
	//! \return False when this spawn is not a hospital wake or no bed exists (plaza fallback).
	static bool ResolveWake(int playerId, string uid, out vector position, out vector yawPitchRoll)
	{
		if (!HasPendingWake(uid))
			return false;

		LF_RespawnConfig config = LF_RespawnConfig.Get();
		if (!config || !config.m_bWakeInHospital)
			return false;

		array<LF_HospitalBedComponent> beds = {};
		LF_HospitalBedComponent.GetBeds(beds);
		array<vector> positions = {};
		LF_HospitalBedComponent.GetBedPositions(positions);
		if (beds.IsEmpty())
		{
			LF_Debug.Warn("Respawn", string.Format("no hospital bed in the world; player %1 respawns at the plaza", playerId));
			return false;
		}

		array<vector> occupants = {};
		CollectLivingCharacters(occupants);
		int index = PickBed(positions, occupants, s_iBedCursor);
		s_iBedCursor = index + 1;

		vector transform[4];
		beds[index].GetSitTransform(transform);
		position = transform[3];
		yawPitchRoll = Math3D.MatrixToAngles(transform);

		if (!s_mWokeInBed)
			s_mWokeInBed = new map<int, bool>();
		s_mWokeInBed.Set(playerId, true);

		LF_Debug.Info("Respawn", string.Format("player %1 wakes in hospital bed %2 of %3 at %4", playerId, index + 1, beds.Count(), position));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectLivingCharacters(notnull array<vector> outPositions)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			if (!controlled)
				continue;

			CharacterControllerComponent controller = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
			if (controller && controller.GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			outPositions.Insert(controlled.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.OnCharacterCreated (after the inventory): charge the bill, print it
	//! into the new body's pockets, toast the amount, and tell the owner to wake up. A spawn with no
	//! pending wake does nothing.
	static void OnBodyCreated(int playerId, string uid, IEntity character)
	{
		bool inBed;
		if (s_mWokeInBed)
		{
			inBed = s_mWokeInBed.Contains(playerId);
			s_mWokeInBed.Remove(playerId);
		}

		if (!HasPendingWake(uid))
			return;

		LF_RespawnConfig config = LF_RespawnConfig.Get();
		int amount = 0;
		int minutes = LF_MedicConfig.Get().m_iNewLifeMinutes;
		int loiterType = 0;
		if (config)
		{
			amount = config.m_iBillAmount;
			loiterType = config.m_iWakeLoiterType;
		}

		string date = LF_HospitalBill.InGameDate();
		LF_TreatmentBillRecord bill = BillOnWake(uid, amount, date);
		int charged = 0;
		int number = 0;
		if (bill)
		{
			charged = bill.m_iAmount;
			number = bill.m_iNumber;
			LF_HospitalBill.Grant(character, bill, minutes);
		}

		// A real death (a pending wake) is the only way here, so this is the New Life wristband.
		LF_NewLifeRule.Issue(uid, character);

		if (!inBed)
			loiterType = 0;

		GetGame().GetCallqueue().CallLater(NotifyOwner, WAKE_NOTIFY_DELAY_MS, false, character, loiterType, charged, number, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyOwner(IEntity character, int loiterType, int charged, int number, string date, int minutes)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(character);
		if (!body)
		{
			LF_Debug.Warn("Respawn", "woken body is gone before the owner could wake up");
			return;
		}

		// The toast needs the body to be the player's controlled entity, which the handover sets after OnBodyCreated.
		if (charged > 0)
			LF_Utils.NotifyEntity(body, "#LF-Hospital_BillToast", "#LF-Hospital_Title", 6.0, LF_MoneyFormat.FormatMoney(charged));

		body.LF_SendHospitalWake(loiterType, charged, number, date, minutes);
	}
}

//------------------------------------------------------------------------------------------------
//! Owner side of the hospital wake: the get-out-of-bed animation, the spawn sound, then the bill.
class LF_HospitalWake
{
	// lf-fixed: retry bound or retry delay of an engine-readiness wait, not a balance value
	static const int RETRY_MS = 500;
	// lf-fixed: retry bound or retry delay of an engine-readiness wait, not a balance value
	static const int MAX_START_TRIES = 6;
	// lf-fixed: retry bound or retry delay of an engine-readiness wait, not a balance value
	static const int MAX_WAIT_TRIES = 40;
	//! The spawn put the body this close (horizontal metres) to the origin of the bed it wakes in.
	static const float BED_SEARCH = 2.0;
	//! A body that got up farther than this from the stand spot was carried away by the get-up.
	static const float CARRIED_AWAY = 2.5;

	//! The floor spot beside the bed this owner's body woke in, read once at the wake start while the
	//! body is still at the bed. The get-up can carry the body away, so the step down never searches
	//! around where it ended up.
	protected static bool s_bHasBed;
	protected static vector s_vStand;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bHasBed = false;
	}

	//------------------------------------------------------------------------------------------------
	static void Begin(SCR_ChimeraCharacter character, int loiterType, int charged, int number, string date, int minutes)
	{
		if (!character)
			return;

		LF_Debug.Info("Respawn", string.Format("waking in hospital: bill %1 (#%2), animation %3", charged, number, loiterType));
		RememberBed(character);

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (world && world.GetMusicManager())
			world.GetMusicManager().Play("SOUND_ONSPAWN");

		if (loiterType > 0)
			TryStart(character, loiterType, 0, charged, number, date, minutes);
		else
			OpenBill(charged, number, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryStart(IEntity character, int loiterType, int attempt, int charged, int number, string date, int minutes)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller)
		{
			OpenBill(charged, number, date, minutes);
			return;
		}

		if (!controller.IsLoitering())
		{
			// The server spawned the body on the bed's sit transform (LF_BedWakePose): the animation
			// plays from where it stands.
			vector transform[4];
			controller.GetOwner().GetWorldTransform(transform);
			controller.StartLoitering(null, loiterType, false, RootMotion(), true, transform, false);
		}

		if (controller.IsLoitering())
		{
			LF_Debug.Info("Respawn", "lying in the hospital bed");
			GetGame().GetCallqueue().CallLater(WaitForStand, RETRY_MS, false, character, 0, charged, number, date, minutes);
			return;
		}

		if (attempt + 1 >= MAX_START_TRIES)
		{
			LF_Debug.Warn("Respawn", "the get-out-of-bed animation did not start; showing the bill");
			StepOffBed(character);
			OpenBill(charged, number, date, minutes);
			return;
		}

		GetGame().GetCallqueue().CallLater(TryStart, RETRY_MS, false, character, loiterType, attempt + 1, charged, number, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WaitForStand(IEntity character, int attempt, int charged, int number, string date, int minutes)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller || !controller.IsLoitering() || attempt + 1 >= MAX_WAIT_TRIES)
		{
			StepOffBed(character);
			OpenBill(charged, number, date, minutes);
			return;
		}

		// The bed loiter holds the body sitting on the bed until stopped; the stop plays the get-up.
		int lieTicks = LieTicks();
		if (lieTicks > 0 && attempt == lieTicks)
		{
			LF_Debug.Info("Respawn", "getting up from the hospital bed");
			controller.StopLoitering(false);
		}

		GetGame().GetCallqueue().CallLater(WaitForStand, RETRY_MS, false, character, attempt + 1, charged, number, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: the body was moved away from its bed mid-wake (the agent harness stand, #4611). The wake
	//! stops where it is: the bed loiter ends and the step down beside the bed never runs, or it would
	//! put the moved body back in the hospital seconds later. The bill card is not opened.
	static void End(IEntity character)
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		queue.Remove(TryStart);
		queue.Remove(WaitForStand);
		bool waking = s_bHasBed;
		s_bHasBed = false;
		SCR_CharacterControllerComponent controller = FindController(character);
		if (controller && controller.IsLoitering())
		{
			controller.StopLoitering(true);
			waking = true;
		}

		if (waking)
			LF_Debug.Info("Respawn", "the waking body was moved away from its bed; the wake ends there");
	}

	//------------------------------------------------------------------------------------------------
	//! The owner reads the same shipped config as the server for how the body wakes.
	protected static bool RootMotion()
	{
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		return !config || config.m_bWakeRootMotion;
	}

	//------------------------------------------------------------------------------------------------
	//! Half-second ticks before the bed loiter is stopped (the get-up); 0 lets it end by itself.
	protected static int LieTicks()
	{
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		if (!config || config.m_fWakeLieSeconds <= 0)
			return 0;

		return Math.Ceil(config.m_fWakeLieSeconds * 1000 / RETRY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_CharacterControllerComponent FindController(IEntity character)
	{
		if (!character)
			return null;

		return SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Owner, at the wake start: the hospital or first-spawn bed (#1965) the body was spawned at.
	protected static void RememberBed(IEntity body)
	{
		s_bHasBed = false;
		LF_HospitalBedComponent bed = LF_HospitalBedComponent.FindNearest(body.GetOrigin(), BED_SEARCH);
		if (bed)
		{
			s_vStand = bed.GetStandPosition(body);
		}
		else
		{
			LF_FirstSpawnBedComponent firstBed = LF_FirstSpawnBeds.FindNearest(body.GetOrigin(), BED_SEARCH);
			if (!firstBed)
			{
				LF_Debug.Warn("Respawn", string.Format("no bed within %1 m of the waking body at %2; it wakes where it stands", BED_SEARCH, body.GetOrigin()));
				return;
			}

			s_vStand = firstBed.GetStandPosition(body);
		}

		s_bHasBed = true;
		LF_Debug.Info("Respawn", string.Format("wakes beside the bed at %1, stands at %2", body.GetOrigin(), s_vStand));
	}

	//------------------------------------------------------------------------------------------------
	//! True when a body that got up at `body` is not standing on the floor beside its bed (off the
	//! floor height, or carried away by the get-up) and must be put on the stand spot.
	static bool NeedsStepDown(vector body, vector stand)
	{
		if (Math.AbsFloat(body[1] - stand[1]) > LF_BedWakePose.ON_FLOOR)
			return true;

		return vector.DistanceSqXZ(body, stand) > CARRIED_AWAY * CARRIED_AWAY;
	}

	//------------------------------------------------------------------------------------------------
	//! The get-up ends with the body standing at the bed edge on the floor. When it is not (off the
	//! floor height, or carried away), it is put on the remembered stand spot. The owner moves its
	//! own body (a server move is overwritten by it).
	protected static void StepOffBed(IEntity character)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(character);
		if (!body || !s_bHasBed)
			return;

		s_bHasBed = false;
		if (!NeedsStepDown(body.GetOrigin(), s_vStand))
		{
			LF_Debug.Info("Respawn", string.Format("got up on the floor beside the bed at %1 (floor %2)", body.GetOrigin(), s_vStand[1]));
			return;
		}

		vector transform[4];
		body.GetWorldTransform(transform);
		transform[3] = s_vStand;
		body.Teleport(transform);
		LF_Debug.Info("Respawn", string.Format("stepped from %1 to the floor beside the bed at %2", body.GetOrigin(), s_vStand));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenBill(int charged, int number, string date, int minutes)
	{
		if (charged <= 0)
			return;

		LF_HospitalBillCard.Open(number, charged, date, "", false, "", minutes, null);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: tell this body's owner they wake up in hospital. The amounts are display only; the
	//! bank was already charged on the server.
	void LF_SendHospitalWake(int loiterType, int charged, int number, string date, int minutes)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Utils.GetLocalCharacter() == this)
		{
			LF_HospitalWake.Begin(this, loiterType, charged, number, date, minutes);
			return;
		}

		Rpc(RpcDo_LF_HospitalWake, loiterType, charged, number, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_HospitalWake(int loiterType, int charged, int number, string date, int minutes)
	{
		LF_HospitalWake.Begin(this, loiterType, charged, number, date, minutes);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HospitalRespawn's statics (LF_StaticReset finds it by reflection). Also drops LF_NewLifeRule's wristbands and its tick.
class LF_HospitalRespawn_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HospitalRespawn";
	}

	override void Apply()
	{
		LF_HospitalRespawn.Reset();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HospitalWake's remembered bed (LF_StaticReset finds it by reflection).
class LF_HospitalWake_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HospitalWake";
	}

	override void Apply()
	{
		LF_HospitalWake.ResetStatics();
	}
}
