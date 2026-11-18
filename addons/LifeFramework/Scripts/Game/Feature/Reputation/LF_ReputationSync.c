//------------------------------------------------------------------------------------------------
//! The owner learns their own standing tiers (#2704, docs/architecture/reputation.md section 5).
//!
//! WIRE. On spawn and on each tier change the server packs the player's three tiers (town, police,
//! underworld) into one int and sends it to that player's own controller with an Owner RPC
//! (RpcDo_LF_StandingTiers). Never an RplProp: that would show every client everyone's underworld
//! tier. Points never leave the server, only tiers. A plain Rpc reaches a listen host's own
//! controller too (docs/architecture/rpc-routing.md), so there is no host branch.
//!
//! CLIENT. The last tiers received are kept in ClientTier for greyed offers (#2701). The underworld
//! tier is kept for that, but no screen draws it.
//!
//! TOAST. A town or police tier change also sends one toast in words through the notifications
//! feature (LF_NotificationManagerComponent.NotifyPlayer). An underworld change sends the tiers and
//! no toast.
//------------------------------------------------------------------------------------------------
class LF_ReputationSync
{
	//! Tiers are packed with this offset and base: tier + OFFSET in [0, BASE). The shipped ladder is -2..3.
	static const int TIER_OFFSET = 16;	// lf-fixed: wire encoding, not tuning
	static const int TIER_BASE = 32;	// lf-fixed: wire encoding, not tuning
	//! Set on every packed value, so 0 is never a valid packet.
	static const int PACKED_FLAG = 32768;	// lf-fixed: wire encoding (32^3), not tuning
	//! The journal keeps this many recent sends (tests, harness).
	static const int JOURNAL_MAX = 32;	// lf-fixed: a debug ring size

	static const string TOAST_TITLE = "#LF-Reputation_Toast_Title";

	//! Server: the last sends, newest last, as "uid|packed|toastKey" (toastKey empty on spawn and
	//! underworld). Recorded whether or not the player is online. Session-only.
	protected static ref array<string> s_aJournal;
	//! Client: the last packed tiers this client received; 0 until the first.
	protected static int s_iClientPacked;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aJournal = null;
		s_iClientPacked = 0;
		LF_ReputationSyncSubscriber.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The factions in wire order.
	static void FactionOrder(notnull array<string> factions)
	{
		factions.Clear();
		factions.Insert(LF_Reputation.FACTION_TOWN);
		factions.Insert(LF_Reputation.FACTION_POLICE);
		factions.Insert(LF_Reputation.FACTION_UNDERWORLD);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The faction's slot on the wire, or -1.
	static int Slot(string faction)
	{
		if (faction == LF_Reputation.FACTION_TOWN)
			return 0;
		if (faction == LF_Reputation.FACTION_POLICE)
			return 1;
		if (faction == LF_Reputation.FACTION_UNDERWORLD)
			return 2;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: three tiers into one int. A tier outside the wire range is clamped (and logged).
	static int Pack(int town, int police, int underworld)
	{
		return PACKED_FLAG + Digit(town) + Digit(police) * TIER_BASE + Digit(underworld) * TIER_BASE * TIER_BASE;
	}

	//------------------------------------------------------------------------------------------------
	protected static int Digit(int tier)
	{
		int digit = tier + TIER_OFFSET;
		if (digit < 0 || digit >= TIER_BASE)
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("tier %1 is outside the wire range; clamped", tier));
			digit = Math.ClampInt(digit, 0, TIER_BASE - 1);
		}

		return digit;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: one faction's tier from a packed value.
	//! \return False for a value that was never packed or an unknown faction (tier is then 0).
	static bool Unpack(int packed, string faction, out int tier)
	{
		tier = 0;
		int slot = Slot(faction);
		if (slot < 0 || packed < PACKED_FLAG || packed >= PACKED_FLAG * 2)
			return false;

		int rest = packed - PACKED_FLAG;
		for (int i = 0; i < slot; i++)
		{
			rest = rest / TIER_BASE;
		}

		tier = (rest % TIER_BASE) - TIER_OFFSET;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character's three tiers, packed.
	static int PackFor(string playerUid)
	{
		return Pack(LF_Reputation.Tier(playerUid, LF_Reputation.FACTION_TOWN), LF_Reputation.Tier(playerUid, LF_Reputation.FACTION_POLICE), LF_Reputation.Tier(playerUid, LF_Reputation.FACTION_UNDERWORLD));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast for a tier change, a `#` key in words; empty for the underworld, an unknown
	//! faction or no change.
	static string ToastKey(string faction, int tierBefore, int tier)
	{
		if (tier == tierBefore)
			return string.Empty;

		bool up = tier > tierBefore;
		if (faction == LF_Reputation.FACTION_TOWN)
		{
			if (up)
				return "#LF-Reputation_Toast_Town_Up";
			return "#LF-Reputation_Toast_Town_Down";
		}

		if (faction == LF_Reputation.FACTION_POLICE)
		{
			if (up)
				return "#LF-Reputation_Toast_Police_Up";
			return "#LF-Reputation_Toast_Police_Down";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the character's tiers to their own controller, and a toast when `toastKey` is set.
	//! An offline character is journaled and skipped; they get their tiers on the next spawn.
	static void Send(string playerUid, string toastKey, string reason)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("standing tiers not sent: empty player id (%1)", reason));
			return;
		}

		int packed = PackFor(playerUid);
		Journal(string.Format("%1|%2|%3", playerUid, packed, toastKey));

		int playerId = FindPlayerId(playerUid);
		if (playerId < 0)
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("standing tiers for %1 not sent: not online (%2)", playerUid, reason));
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("standing tiers for %1 not sent: player %2 has no controller (%3)", playerUid, playerId, reason));
			return;
		}

		controller.LF_SendStandingTiers(packed);
		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("standing tiers sent to player %1 (%2)", playerId, reason));

		if (!toastKey.IsEmpty())
			LF_NotificationManagerComponent.NotifyPlayer(playerId, TOAST_TITLE, toastKey);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Journal(string entry)
	{
		if (!s_aJournal)
			s_aJournal = {};

		s_aJournal.Insert(entry);
		while (s_aJournal.Count() > JOURNAL_MAX)
		{
			s_aJournal.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and harness: copies the send journal ("uid|packed|toastKey", newest last).
	static void GetJournal(notnull array<string> entries)
	{
		entries.Clear();
		if (s_aJournal)
			entries.Copy(s_aJournal);
	}

	//------------------------------------------------------------------------------------------------
	static void ClearJournal()
	{
		s_aJournal = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The server player id of a connected character, or -1.
	static int FindPlayerId(string playerUid)
	{
		if (playerUid.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return -1;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) == playerUid)
				return playerId;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: stores the tiers the server sent this player.
	static void ReceiveOnClient(int packed)
	{
		int town;
		if (!Unpack(packed, LF_Reputation.FACTION_TOWN, town))
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("standing tiers ignored: %1 is not a packed value", packed));
			return;
		}

		s_iClientPacked = packed;
		LF_Debug.Info(LF_Reputation.FEATURE, "standing tiers received");
	}

	//------------------------------------------------------------------------------------------------
	//! Client: this player's own tier with a faction, as the server last sent it. For greying offers;
	//! a screen never draws the underworld tier.
	//! \return False until the first send arrived, or for an unknown faction.
	static bool ClientTier(string faction, out int tier)
	{
		return Unpack(s_iClientPacked, faction, tier);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen for spawns and tier changes (idempotent, also after a bus reset).
	static void Install()
	{
		LF_ReputationSyncSubscriber.Install();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: stop listening.
	static void Uninstall()
	{
		LF_ReputationSyncSubscriber.Uninstall();
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a spawn sends the tiers; a tier change sends the tiers and, for the town and the
//! police, one toast.
class LF_ReputationSyncSubscriber : LF_EventSubscriber
{
	protected static ref LF_ReputationSyncSubscriber s_Subscriber;

	override string GetName()
	{
		return "ReputationSync";
	}

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnReputationChanged changed = LF_OnReputationChanged.Cast(busEvent);
		if (changed)
		{
			LF_ReputationSync.Send(changed.m_sPlayerUid, LF_ReputationSync.ToastKey(changed.m_sFaction, changed.m_iTierBefore, changed.m_iTier), "tier change " + changed.m_sFaction);
			return true;
		}

		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (spawned)
		{
			string uid = LF_Utils.GetPlayerUID(spawned.m_iPlayerId);
			if (uid.IsEmpty())
			{
				LF_Debug.Warn(LF_Reputation.FEATURE, string.Format("standing tiers not sent on spawn: player %1 has no record key yet", spawned.m_iPlayerId));
				return true;
			}

			LF_ReputationSync.Send(uid, string.Empty, "spawn");
		}

		return true;
	}

	static void Install()
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		Uninstall();
		s_Subscriber = new LF_ReputationSyncSubscriber();
		LF_EventBus.Subscribe(LF_OnReputationChanged, s_Subscriber);
		LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
	}

	static void Uninstall()
	{
		if (!s_Subscriber)
			return;

		LF_EventBus.Unsubscribe(LF_OnReputationChanged, s_Subscriber);
		LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
		s_Subscriber = null;
	}

	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The wire: the server sends the packed tiers to this controller's own client only.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: send this player their own tiers. A listen host's own controller runs the handler
	//! inside the Rpc call (docs/architecture/rpc-routing.md), so no host branch.
	void LF_SendStandingTiers(int packed)
	{
		Rpc(RpcDo_LF_StandingTiers, packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_StandingTiers(int packed)
	{
		LF_ReputationSync.ReceiveOnClient(packed);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_ReputationSync.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ReputationSync's statics (LF_StaticReset finds it by reflection).
class LF_ReputationSync_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReputationSync";
	}

	override void Apply()
	{
		LF_ReputationSync.ResetStatics();
	}
}
