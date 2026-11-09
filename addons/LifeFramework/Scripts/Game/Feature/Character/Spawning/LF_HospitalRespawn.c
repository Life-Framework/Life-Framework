// LF_HospitalRespawn.c — you wake up in hospital (decision 2026-11-08, M1-26).
//
// Server-authoritative. A death marks the player's uid with a pending wake. The next spawn of
// that player (the death-screen Respawn, or a reconnect while dead) takes the pending wake once:
// the body is placed at a free hospital bed, the bill is charged to the bank (into debt when the
// account cannot cover it and LF_RespawnConfig allows it), and a printed paper bill goes into the
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
	}

	//------------------------------------------------------------------------------------------------
	//! Charges a hospital bill to an account.
	//! \param amount Bill amount; zero or less charges nothing.
	//! \param debtAllowed When false the bill takes only what the account holds.
	//! \return The amount charged.
	static int ChargeBill(LF_BankAccount account, int amount, bool debtAllowed)
	{
		if (!account || amount <= 0)
			return 0;

		return account.Charge(amount, debtAllowed, "hospital-bill");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the pending wake for a uid and charges the bill to their bank account.
	//! \return The amount charged, or -1 when there was no death to bill for.
	static int BillOnWake(string uid, int amount, bool debtAllowed)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Respawn", "bill refused: empty uid");
			return -1;
		}

		if (!TakePendingWake(uid))
			return -1;

		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		int before = account.GetBalance();
		int charged = ChargeBill(account, amount, debtAllowed);
		LF_Debug.Info("Respawn", string.Format("hospital bill %1 charged to %2: bank %3 -> %4", charged, uid, before, account.GetBalance()));
		if (account.IsInDebt())
			LF_Debug.Info("Respawn", string.Format("%1 is in debt: %2", uid, account.GetBalance()));

		return charged;
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
		beds[index].GetWakeTransform(transform);
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
		bool debtAllowed = false;
		int minutes = 0;
		int loiterType = 0;
		if (config)
		{
			amount = config.m_iBillAmount;
			debtAllowed = config.m_bDebtAllowed;
			minutes = config.m_iNewLifeMinutes;
			loiterType = config.m_iWakeLoiterType;
		}

		int charged = BillOnWake(uid, amount, debtAllowed);
		if (charged < 0)
			return;

		int balance = LF_ATMManager.GetOrCreate(uid).GetBalance();
		string date = LF_HospitalBill.InGameDate();
		LF_HospitalBill.Grant(character, charged, balance, date, minutes);

		if (!inBed)
			loiterType = 0;

		GetGame().GetCallqueue().CallLater(NotifyOwner, WAKE_NOTIFY_DELAY_MS, false, character, loiterType, charged, balance, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyOwner(IEntity character, int loiterType, int charged, int balance, string date, int minutes)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(character);
		if (!body)
		{
			LF_Debug.Warn("Respawn", "woken body is gone before the owner could wake up");
			return;
		}

		// The toast needs the body to be the player's controlled entity, which the handover sets after OnBodyCreated.
		LF_Utils.NotifyEntity(body, "#LF-Hospital_BillToast", "#LF-Hospital_Title", 6.0, LF_MoneyFormat.FormatMoney(charged));
		body.LF_SendHospitalWake(loiterType, charged, balance, date, minutes);
	}
}

//------------------------------------------------------------------------------------------------
//! Owner side of the hospital wake: the get-out-of-bed animation, the spawn sound, then the bill.
class LF_HospitalWake
{
	static const int RETRY_MS = 500;
	static const int MAX_START_TRIES = 6;
	static const int MAX_WAIT_TRIES = 40;

	//------------------------------------------------------------------------------------------------
	static void Begin(SCR_ChimeraCharacter character, int loiterType, int charged, int balance, string date, int minutes)
	{
		if (!character)
			return;

		LF_Debug.Info("Respawn", string.Format("waking in hospital: bill %1, balance %2, animation %3", charged, balance, loiterType));

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (world && world.GetMusicManager())
			world.GetMusicManager().Play("SOUND_ONSPAWN");

		if (loiterType > 0)
			TryStart(character, loiterType, 0, charged, balance, date, minutes);
		else
			OpenBill(charged, balance, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TryStart(IEntity character, int loiterType, int attempt, int charged, int balance, string date, int minutes)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller)
		{
			OpenBill(charged, balance, date, minutes);
			return;
		}

		if (!controller.IsLoitering())
		{
			vector transform[4];
			controller.GetOwner().GetWorldTransform(transform);
			controller.StartLoitering(null, loiterType, false, RootMotion(), true, transform, false);
		}

		if (controller.IsLoitering())
		{
			LF_Debug.Info("Respawn", "lying in the hospital bed");
			GetGame().GetCallqueue().CallLater(WaitForStand, RETRY_MS, false, character, 0, charged, balance, date, minutes);
			return;
		}

		if (attempt + 1 >= MAX_START_TRIES)
		{
			LF_Debug.Warn("Respawn", "the get-out-of-bed animation did not start; showing the bill");
			StepOffBed(character);
			OpenBill(charged, balance, date, minutes);
			return;
		}

		GetGame().GetCallqueue().CallLater(TryStart, RETRY_MS, false, character, loiterType, attempt + 1, charged, balance, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WaitForStand(IEntity character, int attempt, int charged, int balance, string date, int minutes)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller || !controller.IsLoitering() || attempt + 1 >= MAX_WAIT_TRIES)
		{
			StepOffBed(character);
			OpenBill(charged, balance, date, minutes);
			return;
		}

		// The bed loiter holds the body lying down until stopped; the stop plays the get-up.
		int lieTicks = LieTicks();
		if (lieTicks > 0 && attempt == lieTicks)
		{
			LF_Debug.Info("Respawn", "getting up from the hospital bed");
			controller.StopLoitering(false);
		}

		GetGame().GetCallqueue().CallLater(WaitForStand, RETRY_MS, false, character, attempt + 1, charged, balance, date, minutes);
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
	//! The bed wake ends with the body standing on the mattress; step it down to the bed's stand
	//! point on the floor. The owner moves its own body (a server move is overwritten by it).
	protected static void StepOffBed(IEntity character)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(character);
		if (!body)
			return;

		LF_HospitalBedComponent bed = LF_HospitalBedComponent.FindNearest(body.GetOrigin(), 1.5);
		if (!bed)
			return;

		vector transform[4];
		body.GetWorldTransform(transform);
		transform[3] = bed.GetStandPosition();
		body.Teleport(transform);
		LF_Debug.Info("Respawn", string.Format("stepped off the hospital bed to %1", transform[3]));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OpenBill(int charged, int balance, string date, int minutes)
	{
		LF_HospitalBillCard.Open(charged, balance, date, minutes, null);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: tell this body's owner they wake up in hospital. The amounts are display only; the
	//! bank was already charged on the server.
	void LF_SendHospitalWake(int loiterType, int charged, int balance, string date, int minutes)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Utils.GetLocalCharacter() == this)
		{
			LF_HospitalWake.Begin(this, loiterType, charged, balance, date, minutes);
			return;
		}

		Rpc(RpcDo_LF_HospitalWake, loiterType, charged, balance, date, minutes);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_HospitalWake(int loiterType, int charged, int balance, string date, int minutes)
	{
		LF_HospitalWake.Begin(this, loiterType, charged, balance, date, minutes);
	}
}
