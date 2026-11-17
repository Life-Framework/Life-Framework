//------------------------------------------------------------------------------------------------
//! A logged-out character stays behind as a lootable body (#1973, maintainer rule #1794).
//!
//! On disconnect the server takes the character away from the leaving player just before the
//! base game would delete it (SCR_BaseGameMode.OnPlayerDisconnected deletes whatever the player
//! still controls), knocks it out for the window (LF_Knockout: it lies down, and vanilla lets
//! anyone open an unconscious body's inventory) and keeps it for a window: 5 minutes, 10 while
//! downed. Killing it is an ordinary death. The body ends at the end of the window, when its
//! player rejoins, or with the session (it is never saved).
//!
//! Where the rejoin lands is LF_DisconnectRules.RejoinFor (#1972); this file only keeps the body.
//!
//! No dupes: the account's carried snapshot (LF_StorageSnapshot, captured on disconnect) is what
//! the rejoin restores and what a restart loads. While the body exists it is the truth: every
//! item taken from or put on it re-captures the snapshot, and its death drops the snapshot
//! (death drops what you carry). So the rejoin gets exactly what the body still held, and a
//! restart in the middle of a robbery loads what was left.
//!
//! Records are keyed by the account uid and hold the body's EntityID, never an IEntity.
//------------------------------------------------------------------------------------------------
class LF_LogoutBodyRecord : Managed
{
	string m_sUid;
	string m_sCharacterId;
	EntityID m_BodyId = EntityID.INVALID;
	float m_fEndsAt;
	bool m_bDead;
}

//------------------------------------------------------------------------------------------------
//! The inventory callbacks need an instance; one listener per body, so an item change re-captures
//! only the body it happened on (#2404).
class LF_LogoutBodyListener : Managed
{
	string m_sUid;

	void OnInventoryChanged(IEntity item, BaseInventoryStorageComponent storage)
	{
		LF_LogoutBody.ScheduleRecapture(m_sUid);
	}
}

//------------------------------------------------------------------------------------------------
//! Owner config, section "logoutBody".
[BaseContainerProps(configRoot: true)]
class LF_LogoutBodyConfig
{
	static const string SECTION = "logoutBody";
	static const int WINDOW_SECONDS_MAX = 3600;	// lf-fixed: the schema limit, one hour

	[Attribute("300", UIWidgets.Auto, "Seconds a logged-out character stays in the world as a lootable body. 0 turns the body off (the character leaves with the player)", params: "0 3600")]
	int m_iWindowSeconds;

	[Attribute("600", UIWidgets.Auto, "Seconds the body stays when the player left downed and did not die (disconnect downedDies off). Never shorter than windowSeconds", params: "0 3600")]
	int m_iDownedWindowSeconds;

	protected static ref LF_LogoutBodyConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The shipped values; the members carry no initializers (#1077).
	static LF_LogoutBodyConfig CreateDefault()
	{
		LF_LogoutBodyConfig config = new LF_LogoutBodyConfig();
		config.m_iWindowSeconds = 300;
		config.m_iDownedWindowSeconds = 600;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped values with the owner's "logoutBody" section over them.
	static LF_LogoutBodyConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = CreateDefault();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
			s_Config.ApplyOwner(reader);

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
	}

	//------------------------------------------------------------------------------------------------
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"windowSeconds", "downedWindowSeconds"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("windowSeconds", m_iWindowSeconds, 0, WINDOW_SECONDS_MAX);
		reader.ReadInt("downedWindowSeconds", m_iDownedWindowSeconds, 0, WINDOW_SECONDS_MAX);
	}
}

//------------------------------------------------------------------------------------------------
class LF_LogoutBody
{
	static const string FEATURE = "LogoutBody";

	// lf-fixed: the knockout outlives the window so the body never stands up before it despawns
	static const int KNOCKOUT_MARGIN_SECONDS = 30;
	// lf-fixed: the sweep cadence, not gameplay tuning
	static const int SWEEP_MS = 1000;

	//! -1 until read: the owner's "logoutBody" section over the shipped 300 s and 600 s (EnsureConfig).
	protected static int s_iWindowSeconds = -1;
	protected static int s_iDownedWindowSeconds = -1;
	protected static ref map<string, ref LF_LogoutBodyRecord> s_mBodies;
	//! The account key of each spawned player: LF_GameModeRoleplay forgets the identity before the
	//! base game's disconnect runs, so Keep reads it from here.
	protected static ref map<int, string> s_mUidByPlayer;
	//! The inventory listener of each kept body, by account uid.
	protected static ref map<string, ref LF_LogoutBodyListener> s_mListeners;
	//! Bodies whose items changed since the last re-capture, by account uid.
	protected static ref array<string> s_aDirty;
	protected static bool s_bRecaptureQueued;
	protected static bool s_bSweeping;
	protected static int s_iKept;
	protected static int s_iDespawned;
	//! Snapshot captures since the last reset (a perf and regression counter).
	protected static int s_iCaptures;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(SweepQueued);
			GetGame().GetCallqueue().Remove(RecaptureQueued);
		}

		LF_LogoutBodyConfig.Reload();
		s_iWindowSeconds = -1;
		s_iDownedWindowSeconds = -1;
		s_mBodies = null;
		s_mUidByPlayer = null;
		s_mListeners = null;
		s_aDirty = null;
		s_bRecaptureQueued = false;
		s_bSweeping = false;
		s_iKept = 0;
		s_iDespawned = 0;
		s_iCaptures = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and server tuning: the window in seconds; 0 turns the body off (the base game deletes it).
	static void SetWindows(int seconds, int downedSeconds)
	{
		s_iWindowSeconds = Math.Max(0, seconds);
		s_iDownedWindowSeconds = Math.Max(0, downedSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! The windows from LF_LogoutBodyConfig; read once per session.
	static void EnsureConfig()
	{
		if (s_iWindowSeconds >= 0)
			return;

		LF_LogoutBodyConfig config = LF_LogoutBodyConfig.Get();
		s_iWindowSeconds = config.m_iWindowSeconds;
		s_iDownedWindowSeconds = config.m_iDownedWindowSeconds;
		LF_Debug.Info(FEATURE, string.Format("logout body window %1 s, downed %2 s (0 is off)", s_iWindowSeconds, s_iDownedWindowSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: how long a body stays. Downed (not a stun) gets the longer window.
	static int WindowFor(bool downed, int windowSeconds, int downedWindowSeconds)
	{
		if (downed)
			return Math.Max(windowSeconds, downedWindowSeconds);

		return windowSeconds;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_LogoutBodyRecord> Bodies()
	{
		if (!s_mBodies)
			s_mBodies = new map<string, ref LF_LogoutBodyRecord>();

		return s_mBodies;
	}

	//------------------------------------------------------------------------------------------------
	static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from SCR_BaseGameMode.OnPlayerDisconnected after the Life Framework cleanup and
	//! before the base game deletes the character. Keeps the character as a body.
	//! \return True when the body stays (the base game then deletes nothing).
	static bool Keep(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		EnsureConfig();
		if (s_iWindowSeconds <= 0)
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
		if (!character)
			return false;

		// A dead character is the base game's corpse; a downed logout that died (LF_BleedOut) is too.
		if (!LF_StorageSnapshot.IsAlive(character))
		{
			LF_Debug.Info(FEATURE, string.Format("player %1 left dead: no logout body", playerId));
			return false;
		}

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty() && s_mUidByPlayer && s_mUidByPlayer.Contains(playerId))
			uid = s_mUidByPlayer.Get(playerId);

		if (s_mUidByPlayer)
			s_mUidByPlayer.Remove(playerId);

		string characterId = CharacterIdOf(uid);
		if (uid.IsEmpty() || characterId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 left with no account character (uid '%2'): the base game deletes the body", playerId, uid));
			return false;
		}

		PlayerController controller = playerManager.GetPlayerController(playerId);
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 left with no controller: the base game deletes the body", playerId));
			return false;
		}

		bool downed = LF_Downed.IsDowned(character) && !LF_Stun.IsStunned(character);
		int seconds = WindowFor(downed, s_iWindowSeconds, s_iDownedWindowSeconds);

		// Lie it down while the player still controls it: the knockout only takes a player's character.
		if (!LF_Downed.IsDowned(character) && !LF_Knockout.Begin(character, seconds + KNOCKOUT_MARGIN_SECONDS, "", "logout"))
			LF_Debug.Warn(FEATURE, string.Format("logout body of %1 could not be laid down; it stands, and only an unconscious body can be looted", uid));

		CharacterControllerComponent movement = character.GetCharacterController();
		if (movement)
			movement.SetMovement(0, vector.Forward);

		controller.SetControlledEntity(null);
		if (playerManager.GetPlayerControlledEntity(playerId) == character)
		{
			LF_Debug.Error(FEATURE, string.Format("could not take the character from player %1: the base game deletes the body", playerId));
			return false;
		}

		return KeepBody(uid, characterId, character, Now() + seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: track body as uid's logout body until endsAt (world seconds). The test entry point.
	static bool KeepBody(string uid, string characterId, IEntity body, float endsAt)
	{
		if (uid.IsEmpty() || characterId.IsEmpty() || !body)
		{
			LF_Debug.Error(FEATURE, string.Format("logout body refused: uid '%1', character '%2', body %3", uid, characterId, body));
			return false;
		}

		// One body per account: an older one (should not exist) goes first.
		Release(uid, "replaced");

		LF_LogoutBodyRecord record = new LF_LogoutBodyRecord();
		record.m_sUid = uid;
		record.m_sCharacterId = characterId;
		record.m_BodyId = body.GetID();
		record.m_fEndsAt = endsAt;
		Bodies().Set(uid, record);
		Watch(body, true, uid);
		LF_LogoutBodyTag.Mark(body, true);

		// The disconnect captured the snapshot; capture again so it is the body's from here on.
		Recapture(record);
		s_iKept++;
		StartSweep();
		LF_Debug.Info(FEATURE, string.Format("%1 logged out: body stays until %2 s (now %3 s), Logged out", uid, endsAt, Now()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player is back (identity known). Their body leaves the world; the rejoin restores
	//! the snapshot, which already holds what the body still carried.
	static void OnPlayerReturned(int playerId)
	{
		if (!Replication.IsServer())
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
			return;

		if (!s_mUidByPlayer)
			s_mUidByPlayer = new map<int, string>();

		s_mUidByPlayer.Set(playerId, uid);
		if (s_mBodies && !s_mBodies.IsEmpty())
			Release(uid, "rejoined");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: end uid's body now. An alive body is captured one last time and deleted; a dead one
	//! is left to the base game's corpse cleanup (its items are loot, its snapshot already dropped).
	//! \return True when there was a body.
	static bool Release(string uid, string reason)
	{
		if (!s_mBodies || !s_mBodies.Contains(uid))
			return false;

		LF_LogoutBodyRecord record = s_mBodies.Get(uid);
		s_mBodies.Remove(uid);
		if (!record)
			return false;

		IEntity body = Resolve(record);
		if (!body)
		{
			LF_Debug.Info(FEATURE, string.Format("logout body of %1 was already gone (%2)", uid, reason));
			return true;
		}

		Watch(body, false, uid);
		LF_LogoutBodyTag.Mark(body, false);
		Recapture(record);
		if (record.m_bDead || !LF_StorageSnapshot.IsAlive(body))
		{
			LF_Debug.Info(FEATURE, string.Format("logout body of %1 ends dead (%2): left to the corpse cleanup", uid, reason));
			return true;
		}

		RplComponent.DeleteRplEntity(body, false);
		s_iDespawned++;
		LF_Debug.Info(FEATURE, string.Format("logout body of %1 despawned (%2)", uid, reason));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ends every body whose window closed by now, notes deaths and lost bodies.
	//! \return How many bodies ended.
	static int Sweep(float now)
	{
		if (!s_mBodies)
			return 0;

		array<string> ended = {};
		foreach (string uid, LF_LogoutBodyRecord record : s_mBodies)
		{
			if (!record)
			{
				ended.Insert(uid);
				continue;
			}

			IEntity body = Resolve(record);
			if (body && !record.m_bDead && !LF_StorageSnapshot.IsAlive(body))
			{
				record.m_bDead = true;
				LF_StorageSnapshot.OnCharacterDied(record.m_sCharacterId);
				LF_Debug.Info(FEATURE, string.Format("logout body of %1 was killed: what it carried is loot", uid));
			}

			if (!body || record.m_bDead || now >= record.m_fEndsAt)
				ended.Insert(uid);
		}

		foreach (string uid : ended)
			Release(uid, "window over");

		return ended.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static void SweepQueued()
	{
		Sweep(Now());
		if (!s_mBodies || s_mBodies.IsEmpty())
			StopSweep();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartSweep()
	{
		if (s_bSweeping || !GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(SweepQueued, SWEEP_MS, true);
		s_bSweeping = true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopSweep()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(SweepQueued);

		s_bSweeping = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Item moves fire mid-transfer; capture a frame later, once per changed body. Only uid's body is
	//! captured again, not every kept body (#2404).
	static void ScheduleRecapture(string uid)
	{
		if (uid.IsEmpty())
			return;

		if (!s_aDirty)
			s_aDirty = new array<string>();

		if (s_aDirty.Find(uid) < 0)
			s_aDirty.Insert(uid);

		if (s_bRecaptureQueued || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bRecaptureQueued = true;
		GetGame().GetCallqueue().CallLater(RecaptureQueued, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RecaptureQueued()
	{
		RecaptureDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the snapshots of the bodies whose items changed follow what those bodies carry now.
	//! \return How many bodies were captured.
	static int RecaptureDirty()
	{
		s_bRecaptureQueued = false;
		if (!s_aDirty || s_aDirty.IsEmpty())
			return 0;

		array<string> dirty = {};
		dirty.Copy(s_aDirty);
		s_aDirty.Clear();
		if (!s_mBodies)
			return 0;

		int captured;
		foreach (string uid : dirty)
		{
			LF_LogoutBodyRecord record = s_mBodies.Get(uid);
			if (!record)
				continue;

			Recapture(record);
			captured++;
		}

		return captured;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every body's snapshot follows what the body carries now.
	static void RecaptureAll()
	{
		if (!s_mBodies)
			return;

		foreach (string uid, LF_LogoutBodyRecord record : s_mBodies)
		{
			if (record)
				Recapture(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshot captures since the last reset.
	static int GetCaptureCount()
	{
		return s_iCaptures;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Recapture(notnull LF_LogoutBodyRecord record)
	{
		IEntity body = Resolve(record);
		if (!body || record.m_bDead)
			return;

		s_iCaptures++;
		// A dead body drops the snapshot inside CaptureCarried.
		if (!LF_StorageSnapshot.CaptureCarried(record.m_sCharacterId, body))
			record.m_bDead = !LF_StorageSnapshot.IsAlive(body);
	}

	//------------------------------------------------------------------------------------------------
	//! Hooks uid's own listener to the body's item added and removed events, or unhooks it.
	protected static void Watch(notnull IEntity body, bool on, string uid)
	{
		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!manager)
		{
			if (on)
				LF_Debug.Error(FEATURE, "logout body has no SCR_InventoryStorageManagerComponent: its loot will not reach the snapshot");

			return;
		}

		if (!s_mListeners)
			s_mListeners = new map<string, ref LF_LogoutBodyListener>();

		LF_LogoutBodyListener listener = s_mListeners.Get(uid);
		if (listener)
		{
			manager.m_OnItemAddedInvoker.Remove(listener.OnInventoryChanged);
			manager.m_OnItemRemovedInvoker.Remove(listener.OnInventoryChanged);
		}

		if (!on)
		{
			s_mListeners.Remove(uid);
			return;
		}

		if (!listener)
		{
			listener = new LF_LogoutBodyListener();
			listener.m_sUid = uid;
			s_mListeners.Set(uid, listener);
		}

		manager.m_OnItemAddedInvoker.Insert(listener.OnInventoryChanged);
		manager.m_OnItemRemovedInvoker.Insert(listener.OnInventoryChanged);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Resolve(notnull LF_LogoutBodyRecord record)
	{
		if (record.m_BodyId == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(record.m_BodyId);
	}

	//------------------------------------------------------------------------------------------------
	protected static string CharacterIdOf(string uid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts || uid.IsEmpty())
			return string.Empty;

		LF_PlayerAccount account = accounts.GetAccount(uid);
		if (!account || !account.GetActiveCharacter())
			return string.Empty;

		return account.GetActiveCharacter().GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Is this entity a logged-out player's body? (The "Logged out" tag.)
	static bool IsLoggedOut(IEntity entity)
	{
		if (!entity || !s_mBodies)
			return false;

		EntityID id = entity.GetID();
		foreach (string uid, LF_LogoutBodyRecord record : s_mBodies)
		{
			if (record && record.m_BodyId == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasBody(string uid)
	{
		return s_mBodies && s_mBodies.Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	static int GetKeptCount()
	{
		return s_iKept;
	}

	//------------------------------------------------------------------------------------------------
	static int GetDespawnedCount()
	{
		return s_iDespawned;
	}
}

//------------------------------------------------------------------------------------------------
//! The one place a disconnecting player's character can be kept: after the Life Framework cleanup
//! (LF_GameModeRoleplay calls super last) and before the base game deletes what the player still
//! controls. The rejoin ends the body once the player's identity is known.
modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (IsMaster())
			LF_LogoutBody.Keep(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess(int iPlayerID)
	{
		if (IsMaster())
			LF_LogoutBody.OnPlayerReturned(iPlayerID);

		super.OnPlayerAuditSuccess(iPlayerID);
	}

	//------------------------------------------------------------------------------------------------
	//! Listen servers and local play may skip the audit; the spawn is the last chance.
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		if (IsMaster())
			LF_LogoutBody.OnPlayerReturned(playerId);

		super.OnPlayerSpawned(playerId, controlledEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LogoutBody's statics (registered at script load; Core/LF_StaticReset.c).
//! A body never outlives its session, so nothing of it is saved.
class LF_LogoutBody_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_LogoutBody_StaticResetHook());

	override string Owner()
	{
		return "LF_LogoutBody";
	}

	override void Apply()
	{
		LF_LogoutBody.ResetStatics();
	}
}
