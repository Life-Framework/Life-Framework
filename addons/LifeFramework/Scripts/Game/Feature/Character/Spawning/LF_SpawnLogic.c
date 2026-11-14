[BaseContainerProps(category: "Respawn")]
class LF_SpawnLogic : SCR_SpawnLogic
{
	[Attribute(category: "New character defaults")]
	protected ref array<ResourceName> m_aDefaultCharacterPrefabs;

	[Attribute(category: "New character defaults")]
	protected ref array<ref LF_DefaultLoadoutItem> m_aDefaultCharacterItems;

	//! Per-role starter loadouts. The CIVILIAN entry is every new body's kit (it replaces
	//! m_aDefaultCharacterItems; clothing slots cannot stack). The POLICE entry is the starter
	//! police kit a first clock-in at the duty desk issues (LF_DutyLocker). No menu picks a role.
	[Attribute(category: "New character defaults")]
	protected ref array<ref LF_FactionLoadout> m_aFactionCharacterItems;

	//! Opening bank balance of a newly registered character (flagship default: the cheapest car on
	//! the lot, so a new player can drive on day one; docs/decisions.md). Paid once, at registration.
	[Attribute("1000", desc: "Bank balance a newly registered character starts with. Paid once, when the registration form is signed.", category: "New character defaults")]
	protected int m_iStartingBank;

	//------------------------------------------------------------------------------------------------
	int GetStartingBank()
	{
		return LF_DebugMoneyTopUp.StartingBank(m_bDebugMoneyTopUp, Math.Max(0, m_iStartingBank), m_iTopUpBankTarget);
	}

	//! Playtest only: nobody runs out of money. On spawn and every 60 s a bank under the low mark is
	//! filled to the target and carried cash under its low mark is filled to its target (ledger reason
	//! debug-topup). Off in the flagship game mode.
	[Attribute("0", desc: "Playtest: top up bank and carried cash for every player (spawn and every 60 s). Off in the flagship mode.", category: "Debug money top-up")]
	protected bool m_bDebugMoneyTopUp;

	[Attribute("100000", desc: "Bank balance under which the top-up fires.", category: "Debug money top-up")]
	protected int m_iTopUpBankLow;

	[Attribute("1000000", desc: "Bank balance the top-up fills to; also a new character's opening bank.", category: "Debug money top-up")]
	protected int m_iTopUpBankTarget;

	[Attribute("5000", desc: "Carried cash under which the top-up fires.", category: "Debug money top-up")]
	protected int m_iTopUpCashLow;

	[Attribute("20000", desc: "Carried cash the top-up fills to.", category: "Debug money top-up")]
	protected int m_iTopUpCashTarget;

	//! Playtest only: the spawn table's travel globe (LF_SpawnHub, #607) teleports players to any playtest
	//! area. The server refuses the request unless this is on. Off in the flagship game mode.
	[Attribute("0", desc: "Playtest: the spawn hub travel globe may teleport players to the playtest areas. Off in the flagship mode.", category: "Playtest hub")]
	protected bool m_bPlaytestHub;

	protected bool m_bTopUpTimer;

	//! Playtest only: the dispatch console's test calls (LF_DebugDispatchAction) place a police or
	//! medic call without a second player at a payphone. Off in the flagship game mode.
	[Attribute("0", desc: "Playtest: the dispatch console can place a test police or medic call for a solo tester. Off in the flagship mode.", category: "Debug test dispatch")]
	protected bool m_bDebugTestDispatch;

	//------------------------------------------------------------------------------------------------
	bool IsDebugTestDispatch()
	{
		return m_bDebugTestDispatch;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDebugMoneyTopUp()
	{
		return m_bDebugMoneyTopUp;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPlaytestHub()
	{
		return m_bPlaytestHub;
	}

	//------------------------------------------------------------------------------------------------
	//! Tops up one player now and arms the 60 s server timer (once).
	protected void TopUpPlayer(int playerId, IEntity character)
	{
		if (!m_bDebugMoneyTopUp)
			return;

		LF_DebugMoneyTopUp.Apply(LF_Utils.GetPlayerUID(playerId), character, m_iTopUpBankLow, m_iTopUpBankTarget, m_iTopUpCashLow, m_iTopUpCashTarget);
		if (!m_bTopUpTimer)
		{
			m_bTopUpTimer = true;
			GetGame().GetCallqueue().Remove(TopUpTick);
			GetGame().GetCallqueue().CallLater(TopUpTick, 60000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TopUpTick()
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character)
				LF_DebugMoneyTopUp.Apply(LF_Utils.GetPlayerUID(id), character, m_iTopUpBankLow, m_iTopUpBankTarget, m_iTopUpCashLow, m_iTopUpCashTarget);
		}
	}

	//! Playtest only: a new character owns two cars from the start, one in the garage and one at the
	//! impound lot, so both flows can be tried at once. Off in the flagship game mode.
	[Attribute("0", desc: "Playtest: a new character owns one car in the garage and one at the impound lot (from the lot flagged m_bTestCarLot).", category: "New character defaults")]
	protected bool m_bGrantTestCars;

	//! Playtest only: the "Send me to jail" board outside the jail works (LF_DebugJailMe). Off in the flagship mode.
	[Attribute("0", desc: "Playtest: the board outside the jail jails you for a short sentence so the jail can be tried solo.", category: "Debug jail")]
	protected bool m_bDebugJailMe;

	//------------------------------------------------------------------------------------------------
	bool IsDebugJailMe()
	{
		return m_bDebugJailMe;
	}

	//------------------------------------------------------------------------------------------------
	bool GrantsTestCars()
	{
		return m_bGrantTestCars;
	}

	//! The town clerk hands the ID card over in the arrival chain (M1-18, a quest with
	//! m_bRewardIdCard) and replaces a lost one for a fee (#902). On, every spawn carries one from
	//! the start and a lost one comes back free at the next spawn (a mode with no clerk).
	[Attribute("0", desc: "Give every spawned character their ID card (LF_IdCard), registered at the clerk or not, and replace a lost one free at the next spawn (a mode with no clerk). Off: the clerk hands the first card over and replaces a lost one for a fee.", category: "New character defaults")]
	protected bool m_bGrantIdAtSpawn;

	//! Registration retries per player. Capped so a player whose UID or account never resolves
	//! (or who left) does not keep a ~1 s retry alive for the whole session.
	protected static ref map<int, int> s_mRegisterRetries;
	static const int MAX_REGISTER_RETRIES = 60;

	//------------------------------------------------------------------------------------------------
	//! Account manager owns player data, not vanilla collections. Leaving the vanilla Player/Character
	//! collections unresolved keeps SCR_SpawnLogic.RequestPlayerData_S on its synchronous no-persistence
	//! path; without this, an ACTIVE persistence system reroutes player spawn through an async
	//! RequestLoad and DoSpawn_S never runs.
	override protected void SetupPersistenceCollections(SCR_RespawnSystemComponent owner)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the spawn sequence once the joining player has been audited.
	//! Vanilla's implementation stops after registering the controller with persistence, so the spawn
	//! kick-off has to be added here (the old persistence-framework base did exactly this).
	override void OnPlayerAuditSuccess_S(int playerId)
	{
		super.OnPlayerAuditSuccess_S(playerId);
		ExcuteInitialLoadOrSpawn_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Handles failed player registration attempts by scheduling a retry.
	protected void OnPlayerRegisterFailed(int playerId)
	{
		if (!s_mRegisterRetries)
			s_mRegisterRetries = new map<int, int>();

		int attempts = s_mRegisterRetries.Get(playerId) + 1;
		if (!ShouldRetryRegister(attempts))
		{
			s_mRegisterRetries.Remove(playerId);
			LF_Debug.Error("Spawn", string.Format("spawn abandoned for playerId %1: UID/account did not resolve after %2 retries", playerId, MAX_REGISTER_RETRIES));
			return;
		}

		s_mRegisterRetries.Set(playerId, attempts);
		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(OnPlayerAuditSuccess_S, delay, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! \param attempt 1-based retry number about to be scheduled.
	static bool ShouldRetryRegister(int attempt)
	{
		return attempt <= MAX_REGISTER_RETRIES;
	}

	//------------------------------------------------------------------------------------------------
	//! Death: leave the body where it fell and let the player respawn through the death
	//! screen. The corpse keeps everything it carried and is only removed by a server
	//! restart or cleanup - it is never deleted here. NotifyReadyForSpawn opens the
	//! death screen on the owning client; the player's Respawn button re-runs
	//! RespawnPlayer_S.
	//! \param playerId The player who died.
	//! \param playerEntity The dead body.
	//! \param killerEntity The killer, may be null.
	//! \param killer The instigator of the kill.
	override void OnPlayerKilled_S(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled_S(playerId, playerEntity, killerEntity, killer);

		// Forward the kill to the quest system so kill objectives progress
		// (additive seam; the quest manager no-ops when no quest is listening).
		LF_QuestManager questManager = LF_QuestManager.GetInstance();
		if (questManager)
			questManager.ReportKill(playerEntity, killerEntity);

		LF_Contraband.FailDiedFromEntity(playerEntity);

		// Death drops what you carry: the body keeps the items, the account loses the snapshot.
		string uid = LF_Utils.GetPlayerUID(playerId);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && !uid.IsEmpty())
		{
			LF_PlayerAccount account = accounts.GetAccount(uid);
			if (account && account.GetActiveCharacter())
			{
				LF_StorageSnapshot.OnCharacterDied(account.GetActiveCharacter().GetId());

				// Death on duty ends the shift; the stored civilian inventory comes back at respawn.
				LF_DutyLocker.OnDied(account);
			}
		}

		// You wake up in hospital: the next spawn of this player goes to a bed and pays the bill.
		if (!uid.IsEmpty())
		{
			LF_HospitalRespawn.MarkDied(uid);
			if (playerEntity)
				LF_NewLifeRule.RecordDeath(uid, playerEntity.GetOrigin());
		}

		SCR_RespawnComponent respawn = GetPlayerRespawnComponent_S(playerId);
		if (respawn)
			respawn.NotifyReadyForSpawn_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server entry for the death-screen respawn: resolves the player's account and creates
	//! their character. Called from the client's RpcAsk_LF_Respawn; the dead body is left
	//! untouched in the world.
	//! \param playerId The player to respawn.
	void RespawnPlayer_S(int playerId)
	{
		string playerUid = LF_Utils.GetAccountUID(playerId);
		if (playerUid.IsEmpty())
			return;

		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetAccount(playerUid);
		if (!account)
			return;

		// A playtest world starts clean after a death when nobody else is on (LF_WorldReset gates it).
		LF_WorldReset.OnRespawn(playerId);

		CreateCharacter(playerId, account);
	}

	//------------------------------------------------------------------------------------------------
	//! Implement the actual spawn behaviour.
	//! Resolves (or creates) the player's account synchronously, then creates their character from the
	//! account's active character.
	override protected void DoSpawn_S(int playerId)
	{
		// The account, not a character key (#331): GetOrCreate must never open an account under a key.
		string playerUid = LF_Utils.GetAccountUID(playerId);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn("Spawn", "persistent UID not available yet for playerId " + playerId + ", retrying");
			OnPlayerRegisterFailed(playerId);
			return;
		}

		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetOrCreate(playerUid);
		if (!account)
		{
			OnPlayerRegisterFailed(playerId);
			return;
		}

		if (s_mRegisterRetries)
			s_mRegisterRetries.Remove(playerId);

		CreateCharacter(playerId, account);
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the player's character from their account and hands control of it over to them.
	//! \param playerId The player who will control the character.
	//! \param account The player's account (already resolved synchronously).
	protected void CreateCharacter(int playerId, notnull LF_PlayerAccount account)
	{
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			LF_Debug.Warn("Spawn", "player " + playerId + " left before their character could be created - not spawning one");
			return;
		}

		LF_PlayerCharacter activeCharacter = account.GetActiveCharacter();
		if (!activeCharacter)
		{
			// No character yet: the character-creation flow owns first spawn (name and age on the
			// registration form) and calls SpawnPlayer_S once the account has one.
			LF_Debug.Info("Spawn", "player " + playerId + " has no character yet, waiting for the character-creation flow");
			return;
		}

		if (playerController.GetControlledEntity())
		{
			IEntity controlled = playerController.GetControlledEntity();
			CharacterControllerComponent characterController = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
			if (!characterController || characterController.GetLifeState() != ECharacterLifeState.DEAD)
			{
				#ifdef WORKBENCH
					// Workbench may hand the player an editor/play-from-camera entity before the
					// account flow creates the requested character. Do not leave that entity in
					// control, or the real character never receives ownership.
					SCR_ChimeraCharacter existingCharacter = SCR_ChimeraCharacter.Cast(controlled);
					ResourceName existingPrefab = LF_Utils.GetPrefabName(controlled);
					if (!existingCharacter || !LF_Utils.SamePrefab(existingPrefab, activeCharacter.GetPrefab()))
					{
						LF_Debug.Warn("Spawn", "replacing Workbench controlled entity with the account character");
						SCR_EntityHelper.DeleteEntityAndChildren(controlled);
					}
					else
				#endif
					{
				LF_Debug.Warn("Spawn", "player " + playerId + " already controls a character - not spawning another");
				return;
					}
			}
		}

		string characterPersistenceId = activeCharacter.GetId();

		ResourceName prefab = GetCreationPrefab(playerId, characterPersistenceId);
		if (!prefab)
		{
			LF_Debug.Error("Spawn", "could not resolve a character prefab for player " + playerId);
			return;
		}

		vector position, yawPitchRoll;
		if (!GetCreationPosition(playerId, characterPersistenceId, position, yawPitchRoll))
		{
			LF_Debug.Error("Spawn", "aborting spawn for player " + playerId + ": no spawn position could be resolved");
			return;
		}

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(yawPitchRoll, spawnParams.Transform);
		spawnParams.Transform[3] = position + "0 0.1 0"; // Anti lethal terrain clipping

		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!character)
		{
			LF_Debug.Error("Spawn", "failed to spawn player character from prefab: " + prefab);
			return;
		}

		OnCharacterCreated(playerId, characterPersistenceId, character);
		HandoverToPlayer(playerId, character);

		// The signed-up delivery job (kept across death and restarts) goes on the new body. After the
		// hand-over: the job component reads the player id from the body its player controls.
		LF_CivilianJob.ApplySignUpToCharacter(LF_Utils.GetPlayerUID(playerId), character);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the spawn position and orientation for a player character.
	//! \return false when no spawn point could be resolved. The caller must abort the spawn
	//!         rather than place the character at a default origin.
	protected bool GetCreationPosition(int playerId, string characterPersistenceId, out vector position, out vector yawPitchRoll)
	{
		position = "0 0 0";
		yawPitchRoll = "0 0 0";

		if (LF_HospitalRespawn.ResolveWake(playerId, LF_Utils.GetPlayerUID(playerId), position, yawPitchRoll))
			return true;

		SCR_SpawnPoint spawnPoint = ResolveSpawnPoint(playerId);
		if (!spawnPoint)
		{
			LF_Debug.Error("Spawn", "could not spawn character, no spawn point on the map");
			return false;
		}

		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the spawn point for the player's current role (CIVILIAN, or POLICE for an officer
	//! who reconnects on duty), falling back to any spawn point when the map has none for that
	//! role. Shared with the character-creation flow so both spawn paths land in the same area.
	//! \param playerId Player whose account role drives the choice.
	//! \return Spawn point, or null when the map has no spawn point at all.
	static SCR_SpawnPoint ResolveSpawnPoint(int playerId)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(playerId);
		if (account)
		{
			SCR_SpawnPoint factionPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction(typename.EnumToString(LF_Faction, account.GetFaction()));
			if (factionPoint)
				return factionPoint;
		}

		return SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The prefab of the player's active character, or null if none can be resolved.
	protected ResourceName GetCreationPrefab(int playerId, string characterPersistenceId)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(playerId);
		if (!account)
			return ResourceName.Empty;

		LF_PlayerCharacter activeCharacter = account.GetActiveCharacter();
		if (!activeCharacter)
			return ResourceName.Empty;

		return activeCharacter.GetPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! Called after a player character has been created and spawned into the world.
	//! A saved carried inventory replaces the starter kit; without one the character gets the
	//! configured default loadout items.
	protected void OnCharacterCreated(int playerId, string characterPersistenceId, IEntity character)
	{
		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(characterPersistenceId, LF_StorageSnapshot.SLOT_CARRIED);
		if (carried)
		{
			LF_StorageSnapshot.Apply(character, carried);
			LF_Debug.Info("Spawn", "restored carried inventory for player " + playerId + " instead of the starter kit");
		}
		else
		{
			ApplyStarterKit(playerId, character);
		}

		// After the inventory: a tag saved over a restart or a logout goes back on the goods (#479).
		LF_Stores.ReattachUnpaid(LF_Utils.GetPlayerUID(playerId), character);

		LF_ServiceRadio.TuneForAccount(LF_PlayerAccountManager.GetInstance().GetFromCache(playerId), character);
		BindAccountComponents(playerId, characterPersistenceId, character);

		// After the inventory so the printed bill has a pocket to land in.
		LF_HospitalRespawn.OnBodyCreated(playerId, LF_Utils.GetPlayerUID(playerId), character);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a character with no saved carried inventory the kit of its current role: CIVILIAN for
	//! every new body, POLICE only for an officer who reconnects on duty without a snapshot.
	protected void ApplyStarterKit(int playerId, IEntity character)
	{
		LF_Faction role = LF_Faction.CIVILIAN;
		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(playerId);
		if (account && account.IsOnDuty())
			role = account.GetFaction();

		GiveRoleKit(character, role);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a role's configured kit onto a character (clothing first, then direct items). The
	//! duty desk uses it for the starter police kit and for a civilian with no stored clothes.
	//! \return False when the character has no inventory.
	bool GiveRoleKit(IEntity character, LF_Faction role)
	{
		InventoryStorageManagerComponent storageManager = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storageManager)
		{
			LF_Debug.Error("Spawn", "role kit not given: the character has no inventory storage manager");
			return false;
		}

		LF_FactionLoadout factionLoadout = GetFactionLoadout(role);

		ref array<ref LF_DefaultLoadoutItem> items = m_aDefaultCharacterItems;
		if (factionLoadout)
		{
			items = factionLoadout.m_aItems;
			LF_Debug.Log("Spawn", "applied role kit " + typename.EnumToString(LF_Faction, role));
		}
		else
		{
			LF_Debug.Log("Spawn", "no kit configured for role " + typename.EnumToString(LF_Faction, role) + ", using generic default items");
		}

		foreach (LF_DefaultLoadoutItem loadoutItem : items)
		{
			if (loadoutItem.m_ePurpose != EStoragePurpose.PURPOSE_LOADOUT_PROXY)
			{
				Debug.Error(string.Format("Failed to add '%1' as default character item. Only clothing/backpack/vest etc. with purpose '%2' are allowed as top level entries.", loadoutItem.m_rPrefab, typename.EnumToString(EStoragePurpose, EStoragePurpose.PURPOSE_LOADOUT_PROXY)));
				continue;
			}

			// An item that belongs to a system turned off on this server is left out (#960).
			if (!LF_Systems.IsItemAllowed(loadoutItem.m_rPrefab))
			{
				LF_Debug.Info("Spawn", "loadout leaves out " + loadoutItem.m_rPrefab + ": its system is off");
				continue;
			}

			IEntity slotEntity = SpawnDefaultCharacterItem(storageManager, loadoutItem);
			if (!slotEntity)
				continue;

			if (!storageManager.TryInsertItem(slotEntity, loadoutItem.m_ePurpose))
				SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
		}

		// Direct items are inserted after the loadout slots so the freshly equipped clothing
		// storage exists for them: food/drink land in pockets, weapons land in hands. Inserting
		// them first leaves a bare character with no pockets, so they fail the best-slot search.
		if (factionLoadout)
			SpawnDirectCharacterItems(storageManager, factionLoadout.m_aDirectItems);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live spawn logic of the respawn system, or null outside a game mode.
	static LF_SpawnLogic GetInstance()
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (!respawnSystem)
			return null;

		return LF_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());
	}

	//------------------------------------------------------------------------------------------------
	protected void BindAccountComponents(int playerId, string characterPersistenceId, IEntity character)
	{
		// After the inventory (starter kit or restored), so the card has a pocket to land in. A
		// restored card comes back with its number and nothing printed; OnSpawn prints it from the
		// registry. A newcomer gets theirs from the town clerk (arrival chain). A card lost since
		// (dropped at death, stolen) stays lost: the clerk replaces it for a fee (#902).
		LF_IdCard.OnSpawn(character, LF_PlayerAccountManager.GetInstance().GetFromCache(playerId), m_bGrantIdAtSpawn, LF_QuestManager.GetInstance().HasEarnedIdCard(LF_Utils.GetPlayerUID(playerId)));

		// The account components need their ids bound to the entity; this runs on every spawn
		// path (first spawn, death respawn) so the bindings survive a fresh character entity.
		LF_CharacterATMComponent atmComponent = LF_CharacterATMComponent.Cast(character.FindComponent(LF_CharacterATMComponent));
		if (atmComponent)
			atmComponent.Init(LF_Utils.GetPlayerUID(playerId));

		LF_CharacterSurvivalComponent survivalComponent = LF_CharacterSurvivalComponent.Cast(character.FindComponent(LF_CharacterSurvivalComponent));
		if (survivalComponent)
			survivalComponent.Init(characterPersistenceId);

		// Level, licences, and turned-in quests live on the body; every new body gets them back.
		LF_Progression.ApplyToCharacter(LF_Utils.GetPlayerUID(playerId), character);

		// A newcomer (no card handed over yet) is pointed at the town clerk.
		LF_QuestTrackerComponent quests = LF_QuestTrackerComponent.Cast(character.FindComponent(LF_QuestTrackerComponent));
		if (quests)
			quests.GreetNewcomer();

		// A prisoner with time left (a rejoin or a restart mid-sentence) goes back to the cell.
		LF_Jail.OnPlayerSpawned(LF_Utils.GetPlayerUID(playerId), character);

		// Unpaid police tickets go with the player; a restart's blank copies are printed again.
		LF_FineTickets.OnPlayerSpawned(LF_Utils.GetPlayerUID(playerId), character);

		// Unpaid treatment bills go with the player too (#260).
		LF_TreatmentBills.OnPlayerSpawned(LF_Utils.GetPlayerUID(playerId), character);

		// Shots near an open crime scene leave casings (#268).
		LF_Evidence.WatchShooter(character);

		TopUpPlayer(playerId, character);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the player's active character and hands control over. Entry point for the
	//! character-creation flow, which cannot go through the platform-audit path: it never fires
	//! for the local player of an offline Workbench session.
	//! \param playerId Player to spawn.
	void SpawnPlayer_S(int playerId)
	{
		DoSpawn_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands network ownership and control of a spawned character to a player.
	//! The old persistence-framework base's HandoverToPlayer, minus its database callback.
	//! \param playerId The player taking control.
	//! \param character The character to hand over.
	protected void HandoverToPlayer(int playerId, IEntity character)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			LF_Debug.Error("Spawn", "cannot hand over character - no player controller for playerId " + playerId);
			return;
		}

		playerController.SetInitialMainEntity(character);

		SCR_BaseGameMode gamemode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gamemode)
			gamemode.OnPlayerEntityChanged_S(playerId, null, character);

		SCR_RespawnComponent respawn = GetPlayerRespawnComponent_S(playerId);
		if (respawn)
			respawn.NotifySpawn(character);

		// The splash placeholder stays on top until told otherwise. The audit chain that normally
		// clears it (HandlePlayFromCamera) never runs for the local player of an offline session,
		// so every handover path clears it here.
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}

	//------------------------------------------------------------------------------------------------
	//! Recursively spawn items configured in the spawn loadout to the character
	//! \param storageManager Storage manager of the character to give the items to
	//! \param loadoutItem Loadout configuration item
	//! \return parent entity of the items spawned by the configuration or null on failure
	protected IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, LF_DefaultLoadoutItem loadoutItem)
	{
		EntitySpawnParams spawnParams();
        spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();
		IEntity slotEntity = GetGame().SpawnEntityPrefabEx(loadoutItem.m_rPrefab, false, null, spawnParams);
		if (!slotEntity) return null;

		if (loadoutItem.m_aComponentDefaults)
		{
			foreach (LF_DefaultLoadoutItemComponent componentDefault : loadoutItem.m_aComponentDefaults)
			{
				componentDefault.ApplyTo(slotEntity);
			}
		}

		if (loadoutItem.m_aStoredItems)
		{
			array<Managed> outComponents();
			slotEntity.FindComponents(BaseInventoryStorageComponent, outComponents);

			foreach (LF_DefaultLoadoutItem storedItem : loadoutItem.m_aStoredItems)
			{
				for (int i = 0; i < storedItem.m_iAmount; i++)
				{
					IEntity spawnedItem = SpawnDefaultCharacterItem(storageManager, storedItem);

					foreach (Managed componentRef : outComponents)
					{
						BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(componentRef);
						if (storageComponent.GetPurpose() & storedItem.m_ePurpose)
						{
							if (!storageManager.TryInsertItemInStorage(spawnedItem, storageComponent)) continue;

							InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
							if (inventoryItemComponent && !inventoryItemComponent.GetParentSlot()) continue;

							break;
						}
					}
				}
			}
		}

		return slotEntity;
	}

	//------------------------------------------------------------------------------------------------
	//! \param faction Faction to look up.
	//! \return The configured loadout for the faction, or null when none is configured.
	LF_FactionLoadout GetFactionLoadout(LF_Faction faction)
	{
		if (!m_aFactionCharacterItems)
			return null;

		foreach (LF_FactionLoadout factionLoadout : m_aFactionCharacterItems)
		{
			if (factionLoadout && factionLoadout.m_eFaction == faction)
				return factionLoadout;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns items that are inserted by best-slot search (weapons, magazines, food, drink) rather
	//! than into a specific loadout slot. Items that fail to insert are deleted by the fail-safe.
	//! \param storageManager Storage manager of the character to give the items to.
	//! \param directItems Prefabs to spawn and insert.
	//! Public for the systems test (#960): a loadout leaves out the items of a system that is off.
	void SpawnDirectCharacterItems(InventoryStorageManagerComponent storageManager, array<ResourceName> directItems)
	{
		if (!storageManager || !directItems)
			return;

		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();

		foreach (ResourceName prefab : directItems)
		{
			if (prefab == ResourceName.Empty)
				continue;

			// The pager of a server with the Pager system off, and the like (#960).
			if (!LF_Systems.IsItemAllowed(prefab))
			{
				LF_Debug.Info("Spawn", "loadout leaves out " + prefab + ": its system is off");
				continue;
			}

			IEntity item = GetGame().SpawnEntityPrefabEx(prefab, false, null, spawnParams);
			if (!item)
			{
				LF_Debug.Error("Spawn", "could not spawn direct loadout item: " + prefab);
				continue;
			}

			if (!storageManager.TryInsertItem(item))
			{
				LF_Debug.Warn("Spawn", "could not insert direct loadout item " + prefab + " - deleting");
				SCR_EntityHelper.DeleteEntityAndChildren(item);
			}
		}
	}
}
