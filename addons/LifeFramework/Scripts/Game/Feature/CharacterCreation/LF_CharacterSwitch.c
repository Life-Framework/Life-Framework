//------------------------------------------------------------------------------------------------
//! Changing character mid-session (#331 made real for players): with more than one character per
//! account (LF_CharacterConfig), a player asks the town clerk to "pull another file". The clerk
//! grants the ask (a short-lived token), the player's screen opens the registry sheet in switch mode
//! (LF_CharacterSelectSheet: the current resident, the others, "register a new resident" while a slot
//! is free, and "stay as ..."), and the pick comes back through the ordinary pick RPC.
//!
//! NOTHING CHANGES UNTIL THE PICK. The body stays where it stands while the sheet is open. On a pick
//! of another character the server checks again (Refuse), then stores the current character the way
//! a disconnect does (carried inventory snapshot, unpaid store tags, last position, playtime, the
//! rejoin mark, an open phone call), takes the body away from the player and deletes it, and spawns
//! the picked character (or opens form R-89 for a new one) through the arrival flow.
//!
//! NO ESCAPE. A switch is refused, logged as [LFDebug:CharacterSwitch] and toasted with the reason
//! while the character is dead, downed, cuffed or escorted, jailed, wanted, on police or medic duty,
//! in a fight (player-on-player damage within LF_CharacterConfig.SwitchCombatSeconds), carrying or
//! hauling contraband, on a delivery job run, in a race, or in a vehicle. Debt stays on the character
//! it belongs to (bills and bank are keyed by the character), so a switch never clears it.
//!
//! SEPARATION. Every character-scoped record is keyed by the character's record key
//! (LF_Utils.GetPlayerUID): bank, garage, houses and plots, organisation membership, licences,
//! wanted level and record, reputation, map marks. The police whitelist and roster are per account
//! (an approved officer may clock in as any of their characters); duty is per character: a switch is
//! refused while on duty, so only the character who clocked in carries the duty kit.
//------------------------------------------------------------------------------------------------
class LF_CharacterSwitchState
{
	bool m_bMultiAllowed;
	bool m_bHasCharacter;
	bool m_bAlive;
	bool m_bDowned;
	bool m_bCuffed;
	bool m_bJailed;
	int m_iWanted;
	bool m_bOnDuty;
	bool m_bInCombat;
	bool m_bContraband;
	bool m_bJobRun;
	bool m_bRacing;
	bool m_bInVehicle;
}

//------------------------------------------------------------------------------------------------
class LF_CharacterSwitch
{
	static const string FEATURE = "CharacterSwitch";
	//! The clerk's first errand: the character is registered once it is handed in.
	static const string ARRIVAL_QUEST = "arrival_register";

	//------------------------------------------------------------------------------------------------
	//! Pure: whether the clerk offers "I need another of my files". The server must allow more than one
	//! character, and the player must have a second file already or be settled in (registered): a brand
	//! new arrival is not offered a character change as their first move (first-2-hours review A10).
	static bool SwitchOffered(int maxPerAccount, bool hasActiveCharacter, int fileCount, bool registered)
	{
		if (maxPerAccount <= 1 || !hasActiveCharacter)
			return false;

		return fileCount > 1 || registered;
	}
	//! How long the clerk's yes stays good for the sheet the player's screen asks for.
	// lf-fixed: a network round trip after the talk closes, not a server choice
	static const float GRANT_SECONDS = 30;

	//! Character key -> world seconds of the last player-on-player damage it dealt or took.
	protected static ref map<string, float> s_mCombatAt;
	//! Player id -> world seconds the clerk granted a switch.
	protected static ref map<int, float> s_mGrantedAt;
	protected static int s_iSwitches;

	//! How long an open switch sheet stays good on the server, and how far from where it opened the
	//! pick may come. Without them a client that never sends the cancel keeps the offer and picks
	//! later, far from the clerk, as a safe vanish with no logout body (#1794).
	// lf-fixed: anti-abuse bounds for one sheet at the clerk's desk, not server tuning
	static const float OFFER_SECONDS = 180;
	// lf-fixed: the clerk's desk, not server tuning
	static const float OFFER_RADIUS = 10;
	//! Player id -> world seconds and body position the switch sheet was sent at.
	protected static ref map<int, float> s_mOfferAt;
	protected static ref map<int, vector> s_mOfferPos;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mCombatAt = null;
		s_mGrantedAt = null;
		s_mOfferAt = null;
		s_mOfferPos = null;
		s_iSwitches = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the switch sheet went to this player, standing at `at`.
	static void NoteOffer(int playerId, float now, vector at)
	{
		if (!s_mOfferAt)
			s_mOfferAt = new map<int, float>();

		if (!s_mOfferPos)
			s_mOfferPos = new map<int, vector>();

		s_mOfferAt.Set(playerId, now);
		s_mOfferPos.Set(playerId, at);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a pick from the switch sheet counts only within OFFER_SECONDS of the offer and within
	//! OFFER_RADIUS of where the player stood when it was sent.
	static bool OfferHolds(int playerId, float now, vector at)
	{
		if (!s_mOfferAt || !s_mOfferAt.Contains(playerId) || !s_mOfferPos)
			return false;

		float offeredAt = s_mOfferAt.Get(playerId);
		if (now < offeredAt || now - offeredAt > OFFER_SECONDS)
			return false;

		return vector.Distance(s_mOfferPos.Get(playerId), at) <= OFFER_RADIUS;
	}

	//------------------------------------------------------------------------------------------------
	static void ForgetOffer(int playerId)
	{
		if (s_mOfferAt)
			s_mOfferAt.Remove(playerId);

		if (s_mOfferPos)
			s_mOfferPos.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why this player may not change character now, as the localization key they are told, or
	//! "" to allow. The order is the order a player most needs to hear.
	static string Refuse(notnull LF_CharacterSwitchState state)
	{
		if (!state.m_bMultiAllowed)
			return "#LF-CharSwitch_Off";

		if (!state.m_bHasCharacter || !state.m_bAlive)
			return "#LF-CharSel_Refused";

		if (state.m_bJailed)
			return "#LF-CharSwitch_Jailed";

		if (state.m_bCuffed)
			return "#LF-CharSwitch_Cuffed";

		if (state.m_bDowned)
			return "#LF-CharSwitch_Downed";

		if (state.m_iWanted > 0)
			return "#LF-CharSwitch_Wanted";

		if (state.m_bOnDuty)
			return "#LF-CharSwitch_OnDuty";

		if (state.m_bInCombat)
			return "#LF-CharSwitch_Combat";

		if (state.m_bContraband)
			return "#LF-CharSwitch_Contraband";

		if (state.m_bJobRun)
			return "#LF-CharSwitch_JobRun";

		if (state.m_bRacing)
			return "#LF-CharSwitch_Racing";

		if (state.m_bInVehicle)
			return "#LF-CharSwitch_Vehicle";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a fight is on while the last player-on-player hit is younger than the window.
	static bool IsInCombat(float lastHitAt, float now, float windowSeconds)
	{
		if (lastHitAt < 0 || windowSeconds <= 0)
			return false;

		return now - lastHitAt < windowSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the state of the player with this body (the body may be null: a test, a dead player).
	static LF_CharacterSwitchState ReadState(int playerId, IEntity body)
	{
		LF_CharacterSwitchState state = new LF_CharacterSwitchState();
		state.m_bMultiAllowed = LF_CharacterConfig.MaxPerAccount() > 1;

		LF_PlayerAccount account;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		string accountUid = LF_Utils.GetAccountUID(playerId);
		if (accounts && !accountUid.IsEmpty())
			account = accounts.GetAccount(accountUid);

		string key = LF_Utils.GetPlayerUID(playerId);
		return ReadStateFor(state, account, key, body);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fills state for this account, character key and body. The seam the tests drive.
	static LF_CharacterSwitchState ReadStateFor(notnull LF_CharacterSwitchState state, LF_PlayerAccount account, string key, IEntity body)
	{
		string characterId;
		if (account && account.GetActiveCharacter())
		{
			state.m_bHasCharacter = true;
			characterId = account.GetActiveCharacter().GetId();
			state.m_iWanted = account.GetWantedLevel();
			state.m_bOnDuty = account.IsOnDuty();
		}

		state.m_bAlive = body && LF_StorageSnapshot.IsAlive(body);
		if (body)
		{
			state.m_bDowned = LF_Downed.IsDowned(body);
			state.m_bCuffed = LF_Restraint.IsRestrained(body);
			state.m_bContraband = LF_Contraband.CountCarried(body) > 0;
			ChimeraCharacter character = ChimeraCharacter.Cast(body);
			state.m_bInVehicle = character && character.IsInVehicle();
		}

		if (!key.IsEmpty())
		{
			state.m_bCuffed = state.m_bCuffed || LF_Restraint.HasHold(key) || LF_JailEscort.IsEscorted(key);
			state.m_bJailed = LF_Jail.IsServing(key);
			state.m_bJobRun = LF_CivilianJob.HasActiveRun(key);
			state.m_bRacing = !LF_RaceRunner.CourseOf(key).IsEmpty();
			state.m_bInCombat = IsInCombat(LastHitAt(key), Now(), LF_CharacterConfig.SwitchCombatSeconds());
		}

		if (!characterId.IsEmpty())
			state.m_bContraband = state.m_bContraband || LF_Contraband.HasRun(characterId);

		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the refusal for this player as they stand now, or "".
	static string RefuseNow(int playerId)
	{
		IEntity body;
		if (GetGame() && GetGame().GetPlayerManager())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		return Refuse(ReadState(playerId, body));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: logs and toasts a refusal. Never silent.
	static void TellRefused(int playerId, string refusal, string where)
	{
		LF_Debug.Warn(FEATURE, string.Format("switch refused for player %1 (%2) at %3: %4", playerId, LF_Utils.GetPlayerUID(playerId), where, refusal));
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-CharSwitch_Title", refusal, 6.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player character dealt or took player-caused damage at `at` (world seconds).
	static void NoteHit(string key, float at)
	{
		if (key.IsEmpty())
			return;

		if (!s_mCombatAt)
			s_mCombatAt = new map<string, float>();

		s_mCombatAt.Set(key, at);
	}

	//------------------------------------------------------------------------------------------------
	//! \return World seconds of this character's last player-on-player hit, or -1.
	static float LastHitAt(string key)
	{
		if (!s_mCombatAt || !s_mCombatAt.Contains(key))
			return -1;

		return s_mCombatAt.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the damage hook: both sides of a hit between two players are in a fight.
	static void OnDamage(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!Replication.IsServer() || !victim || damageContext.damageValue <= 0)
			return;

		Instigator instigator = damageContext.instigator;
		if (!instigator)
			return;

		int attackerId = instigator.GetInstigatorPlayerID();
		PlayerManager players = GetGame().GetPlayerManager();
		if (attackerId <= 0 || !players)
			return;

		int victimId = players.GetPlayerIdFromControlledEntity(victim);
		if (victimId <= 0 || victimId == attackerId)
			return;

		float now = Now();
		NoteHit(LF_Utils.GetPlayerUID(victimId), now);
		NoteHit(LF_Utils.GetPlayerUID(attackerId), now);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the clerk said yes; the sheet the player's screen asks for within GRANT_SECONDS opens.
	static void Grant(int playerId, float now)
	{
		if (!s_mGrantedAt)
			s_mGrantedAt = new map<int, float>();

		s_mGrantedAt.Set(playerId, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: uses up the clerk's yes. \return True when one was given within GRANT_SECONDS of now.
	static bool ConsumeGrant(int playerId, float now)
	{
		if (!s_mGrantedAt || !s_mGrantedAt.Contains(playerId))
			return false;

		float at = s_mGrantedAt.Get(playerId);
		s_mGrantedAt.Remove(playerId);
		return now >= at && now - at <= GRANT_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player left; their id is reused by the next arrival.
	static void Forget(int playerId)
	{
		if (s_mGrantedAt)
			s_mGrantedAt.Remove(playerId);

		ForgetOffer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stores the active character as a disconnect does and removes its body, so the player
	//! controls nothing and the arrival flow can spawn another character. Call only after Refuse said "".
	//! \return False (logged) when the player has no living body to put away.
	static bool StoreAndRemoveBody(int playerId)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		PlayerController controller = players.GetPlayerController(playerId);
		IEntity body = players.GetPlayerControlledEntity(playerId);
		if (!controller || !body || !LF_StorageSnapshot.IsAlive(body))
		{
			LF_Debug.Error(FEATURE, string.Format("switch of player %1 stopped: no controller or no living body to put away", playerId));
			return false;
		}

		string key = LF_Utils.GetPlayerUID(playerId);
		int nowUnix = System.GetUnixTime();

		// What a disconnect stores, in the same order (LF_GameModeRoleplay.OnPlayerDisconnected).
		LF_Playtime.Settle(playerId, nowUnix);
		LF_PhoneCalls.OnPlayerDisconnected(playerId);
		bool captured = LF_StorageSnapshot.CapturePlayer(playerId);
		LF_Stores.ParkCarrier(key, body);
		LF_LastPosition.CapturePlayer(playerId, nowUnix);
		// The stored character's next spawn is a rejoin where it stood.
		LF_RejoinSpot.ForgetConnection(key);
		// Session state keyed by the player id that must not follow them into another life.
		LF_Dialogue.Forget(playerId);
		LF_TownBoardMapKnown.Forget(playerId);

		controller.SetControlledEntity(null);
		if (players.GetPlayerControlledEntity(playerId) == body)
		{
			LF_Debug.Error(FEATURE, string.Format("switch of player %1 stopped: the body could not be taken from them", playerId));
			return false;
		}

		RplComponent.DeleteRplEntity(body, false);
		s_iSwitches++;
		LF_Debug.Info(FEATURE, string.Format("player %1 put away character %2 (carried inventory captured %3)", playerId, key, captured));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the clerk offers "I need another file" when the server allows more than one character.
class LF_DialogueHook_CharacterSwitchOffered : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_PlayerAccount account = LF_PoliceUtils.GetAccountForEntity(player);
		if (!account)
			return false;

		bool settled;
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		if (tracker)
			settled = tracker.HasTurnedIn(LF_CharacterSwitch.ARRIVAL_QUEST);

		return LF_CharacterSwitch.SwitchOffered(LF_CharacterConfig.MaxPerAccount(), account.GetActiveCharacter() != null, account.GetCharacterCount(), settled);
	}
}

//------------------------------------------------------------------------------------------------
//! Condition: the clerk's "About my papers" leads somewhere: a lost card to replace, or another file
//! to pull. The papers node holds both, so the greeting keeps to its four replies.
class LF_DialogueHook_PapersOffered : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		LF_DialogueHook lost = LF_DialogueHook.Find("IdCardMissing");
		if (lost && lost.Allows(player, npc, param))
			return true;

		LF_DialogueHook other = LF_DialogueHook.Find("CharacterSwitchOffered");
		return other && other.Allows(player, npc, param);
	}
}

//------------------------------------------------------------------------------------------------
//! Action: the clerk pulls the register. Refused with the reason in the clerk's answer (and a toast);
//! allowed, the clerk grants the ask and the player's screen asks for the sheet once the talk closes.
// lf-spoken: #LF-CharSwitch_ClerkNo #LF-CharSwitch_ClerkYes
class LF_DialogueHook_CharacterSwitch : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		int playerId = LF_PlayerInteraction.PlayerIdOf(player);
		if (playerId <= 0)
		{
			LF_Debug.Warn(LF_CharacterSwitch.FEATURE, "clerk switch refused: the talker is no player");
			return;
		}

		string refusal = LF_CharacterSwitch.RefuseNow(playerId);
		if (!refusal.IsEmpty())
		{
			LF_CharacterSwitch.TellRefused(playerId, refusal, "the clerk");
			spoken.Insert("#LF-CharSwitch_ClerkNo");
			return;
		}

		LF_CharacterSwitch.Grant(playerId, LF_CharacterSwitch.Now());
		spoken.Insert("#LF-CharSwitch_ClerkYes");
		LF_Debug.Info(LF_CharacterSwitch.FEATURE, string.Format("clerk granted player %1 a character switch", playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the talk closed; ask the server for the sheet. A refused Run granted nothing, so the
	//! server answers that ask with nothing but a log line.
	override void AfterScene(IEntity player, IEntity npc, string param)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskCharacterSwitch();
	}
}

//------------------------------------------------------------------------------------------------
//! A hit between two players starts the fight window on both.
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		bool hijacked = super.HijackDamageHandling(damageContext);
		if (!hijacked)
			LF_CharacterSwitch.OnDamage(GetOwner(), damageContext);

		return hijacked;
	}
}

//------------------------------------------------------------------------------------------------
//! The switch channel on the player's own controller.
//!   owner -> server : LF_AskCharacterSwitch (after the clerk), LF_AskCancelCharacterSwitch
//!   server -> owner : LF_ShowCharacterSwitch (the sheet in switch mode)
//! The pick itself is the ordinary RpcAsk_LF_SelectCharacter.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: a switch took the body away. The names this character learned leave with it; the next
	//! spawn's name sync teaches the new character its own.
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (!to && from && LF_IsLocalController())
			LF_KnownNames.ForgetAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the clerk said yes; open the switch sheet.
	void LF_AskCharacterSwitch()
	{
		if (Replication.IsServer())
			RpcAsk_LF_CharacterSwitch();
		else
			Rpc(RpcAsk_LF_CharacterSwitch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CharacterSwitch()
	{
		LF_CharacterCreationManager manager = LF_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OfferSwitch(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the player put the switch sheet away or chose to stay.
	void LF_AskCancelCharacterSwitch()
	{
		if (Replication.IsServer())
			RpcAsk_LF_CancelCharacterSwitch();
		else
			Rpc(RpcAsk_LF_CancelCharacterSwitch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CancelCharacterSwitch()
	{
		LF_CharacterCreationManager manager = LF_CharacterCreationManager.GetInstance();
		if (manager)
			manager.CancelSwitch(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the sheet in switch mode on this player's machine.
	//! \param currentSlot 0-based slot of the character being played.
	void LF_ShowCharacterSwitch(string names, bool canRegister, int currentSlot)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowCharacterSwitch(names, canRegister, currentSlot);
		else
			Rpc(RpcDo_LF_ShowCharacterSwitch, names, canRegister, currentSlot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowCharacterSwitch(string names, bool canRegister, int currentSlot)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_CharacterSwitch.FEATURE, "switch sheet not opened: no workspace on this machine");
			return;
		}

		if (!LF_CharacterSelectSheet.Open(this, names, canRegister, currentSlot))
			LF_Debug.Error(LF_CharacterSwitch.FEATURE, "failed to open the switch sheet");
	}
}

//------------------------------------------------------------------------------------------------
//! A leaver's clerk grant goes with the connection (player ids are reused).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
			LF_CharacterSwitch.Forget(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CharacterSwitch's statics (LF_StaticReset finds it by reflection).
class LF_CharacterSwitch_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CharacterSwitch";
	}

	override void Apply()
	{
		LF_CharacterSwitch.ResetStatics();
	}
}
