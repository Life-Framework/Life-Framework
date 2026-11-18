//------------------------------------------------------------------------------------------------
//! Why a cast was refused (#2930). NONE is a cast that is now out.
enum LF_EFishingRefusal
{
	NONE,
	OFF,			//! the fishing config did not load
	NO_CHARACTER,	//! no living body behind the request
	NO_ROD,			//! the rod is not in the hands, or the rod is not in the config
	LEVEL,			//! the FISHER level is below the rod's
	RATE,			//! more casts in a minute than the config allows
	ALREADY_OUT,	//! a cast of this player is already out
	RANGE,			//! the target is farther than the rod casts
	NOT_WATER,		//! the target is not OCEAN or POND water
	NO_BAIT			//! no bait item in the inventory
}

//------------------------------------------------------------------------------------------------
//! What a strike did (#2930).
enum LF_EFishingStrike
{
	IGNORED,	//! no open cast, a stale cast id, or no server bite yet: nothing changes, nothing is spent
	CAUGHT,		//! inside the window: one fish given, the bait spent
	LATE,		//! after the window: no fish, the bait is gone
	EMPTY,		//! inside the window but nothing could be given (config problem, logged); the bait is gone
	CANCELLED	//! the rod, the bait or the spot under the player is gone: the cast ends, nothing spent
}

//------------------------------------------------------------------------------------------------
//! What the owner's machine is told (RpcDo_LF_FishingEvent). The float and its sound hang off these.
enum LF_EFishingEvent
{
	CAST_OK,	//! the line is out; the castId is the one a strike must carry
	BITE,		//! the float dips: "Strike!"
	CAUGHT,
	MISSED,		//! late or empty: the bait is gone
	ENDED		//! the cast ended with nothing spent (moved away, rod put away)
}

//------------------------------------------------------------------------------------------------
//! One open cast, server state only: never saved and never sent.
class LF_FishingCast : Managed
{
	int m_iCastId;
	string m_sKey;
	ref LF_FishingRodDef m_Rod;
	ref LF_FishingBaitDef m_Bait;
	vector m_vOrigin;			//! where the player stood when casting
	vector m_vTarget;
	string m_sWater;			//! OCEAN or POND
	string m_sSpotTable;		//! the table of the spot covering the target, empty for open water
	EntityID m_SpotId;
	EntityID m_CharacterId;		//! the body the cast was made from
	int m_iPlayerId;			//! the player who cast: every event goes here, even after that body died
	float m_fCastAt;
	bool m_bBitten;				//! the server's bite has happened
	float m_fBittenAt;			//! seconds, set by the bite and never by the client
}

//------------------------------------------------------------------------------------------------
//! Fishing, server side (#2930, docs/architecture/fishing.md section 3): the cast, the bite timer and
//! the strike. Every step that pays is decided here: the client sends a target and a strike and
//! nothing else, and the bite time, the window, the fish and the bait are the server's. All times are
//! seconds on the caller's clock (`LF_JobGuards.Now()` in play, a number in a test). State is one open
//! cast per player and the cast times of the last minute; it is session state and never saved.
//------------------------------------------------------------------------------------------------
class LF_FishingSession
{
	static const string TOOL_ID = "rod";
	static const string XP_ACTION = "fish_catch";
	static const string JOB = "FISHER";
	static const float MOVE_ENDS_CAST_METRES = 5;	// lf-fixed: design section 3, the cast ends when the player walks off
	static const float MAX_ASK_METRES = 200;		// lf-fixed: a cast request farther than this from the body is dropped before any query
	static const float LIMIT_METRES = 100000;		// lf-fixed: no map is larger; anything beyond is a forged payload
	static const float EXPIRE_MARGIN_MS = 250;		// lf-fixed: the expiry runs just after the last moment a strike lands

	protected static ref map<string, ref LF_FishingCast> s_mCasts;
	protected static ref map<string, ref array<float>> s_mCastTimes;
	protected static int s_iNextCastId;

	//------------------------------------------------------------------------------------------------
	//! The key a player's cast is kept under: the player id, else (a body no player drives) the entity id.
	static string KeyOf(IEntity character)
	{
		if (!character)
			return "";

		string uid = LF_Utils.GetPlayerUID(character);
		if (!uid.IsEmpty())
			return uid;

		return "body_" + character.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The open cast of this key, or null.
	static LF_FishingCast Open(string key)
	{
		if (!s_mCasts || key.IsEmpty())
			return null;

		return s_mCasts.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every part of a client's target is a real number inside the world's size
	//! (a forged NaN or a huge value must not reach the maths or the water query).
	static bool ValidTarget(vector target)
	{
		for (int i = 0; i < 3; i++)
		{
			if (target[i] != target[i] || Math.AbsFloat(target[i]) > LIMIT_METRES)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "OCEAN", "POND" or "" for the water at `target` (flat position; the terrain height under
	//! it is read here, never taken from the client).
	static string WaterKindAt(vector target)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "";

		vector probe = target;
		probe[1] = world.GetSurfaceY(target[0], target[2]);
		vector surface;
		vector transformWS[4];
		vector extents;
		EWaterSurfaceType kind;
		if (!ChimeraWorldUtils.TryGetWaterSurface(world, probe, surface, kind, transformWS, extents))
			return "";

		if (kind == EWaterSurfaceType.WST_OCEAN)
			return LF_FishingConfig.WATER_OCEAN;
		if (kind == EWaterSurfaceType.WST_POND)
			return LF_FishingConfig.WATER_POND;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a cast at `target`, the water read from the world.
	static LF_EFishingRefusal CastAt(IEntity character, vector target, float now)
	{
		return CastOn(character, target, WaterKindAt(target), now);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a cast at `target` onto `water` ("OCEAN", "POND" or "" for land). The tests call this to
	//! put a cast on water the debug world does not have; `Cast` reads the water and calls it. Checks, in
	//! order: living body, rod in the hands and in the config, level, rate, no cast out, range, water,
	//! bait. A refusal logs, toasts and leaves no state. On success the bite timer is set.
	static LF_EFishingRefusal CastOn(IEntity character, vector target, string water, float now)
	{
		LF_EFishingRefusal why = TryCast(character, target, water, now);
		if (why != LF_EFishingRefusal.NONE)
		{
			LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("cast refused for %1: %2", KeyOf(character), typename.EnumToString(LF_EFishingRefusal, why)));
			Toast(PlayerIdOf(character), RefusalText(why));
		}

		return why;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EFishingRefusal TryCast(IEntity character, vector target, string water, float now)
	{
		LF_FishingConfig config = LF_FishingConfig.Get();
		if (!config)
			return LF_EFishingRefusal.OFF;

		if (!character || !LF_Utils.IsAliveState(character))
			return LF_EFishingRefusal.NO_CHARACTER;

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || chimera.IsInVehicle())
			return LF_EFishingRefusal.NO_CHARACTER;

		string key = KeyOf(character);
		IEntity rodItem = LF_HandTool.HeldTool(character, TOOL_ID);
		if (!rodItem)
			return LF_EFishingRefusal.NO_ROD;

		LF_FishingRodDef rod = config.FindRod(LF_Utils.GetPrefabName(rodItem));
		if (!rod)
		{
			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("the held rod %1 is not in the fishing config", LF_Utils.GetPrefabName(rodItem)));
			return LF_EFishingRefusal.NO_ROD;
		}

		int level = LF_JobLevels.LevelOf(key, JOB);
		if (!LF_FishingRules.RodUsable(rod, level))
			return LF_EFishingRefusal.LEVEL;

		if (!s_mCastTimes)
			s_mCastTimes = new map<string, ref array<float>>();

		array<float> times = s_mCastTimes.Get(key);
		if (!times)
		{
			times = {};
			s_mCastTimes.Set(key, times);
		}

		if (!LF_FishingRules.TryCast(config, times, now))
			return LF_EFishingRefusal.RATE;

		if (Open(key))
			return LF_EFishingRefusal.ALREADY_OUT;

		float reach = Math.Sqrt(vector.DistanceSqXZ(character.GetOrigin(), target));
		if (!LF_FishingRules.InCastRange(rod, reach))
			return LF_EFishingRefusal.RANGE;

		if (!LF_FishingConfig.IsWaterKind(water))
			return LF_EFishingRefusal.NOT_WATER;

		LF_FishingBaitDef bait;
		if (!FindBait(character, config, bait))
			return LF_EFishingRefusal.NO_BAIT;

		LF_FishingSpotComponent spot = LF_FishingSpotComponent.FindSpotFor(target);
		bool spotEmpty = spot && spot.StockLeft(now) <= 0;
		float seconds = LF_FishingRules.BiteSeconds(config, rod, bait, spotEmpty, Math.RandomFloat01());

		LF_FishingCast cast = new LF_FishingCast();
		s_iNextCastId++;
		cast.m_iCastId = s_iNextCastId;
		cast.m_sKey = key;
		cast.m_CharacterId = character.GetID();
		cast.m_iPlayerId = PlayerIdOf(character);
		cast.m_Rod = rod;
		cast.m_Bait = bait;
		cast.m_vOrigin = character.GetOrigin();
		cast.m_vTarget = target;
		cast.m_sWater = water;
		cast.m_fCastAt = now;
		if (spot)
		{
			cast.m_sSpotTable = spot.GetTable();
			cast.m_SpotId = spot.GetOwner().GetID();
		}

		if (!s_mCasts)
			s_mCasts = new map<string, ref LF_FishingCast>();

		s_mCasts.Set(key, cast);
		GetGame().GetCallqueue().CallLater(OnBiteTimer, seconds * 1000, false, key, cast.m_iCastId);
		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("%1 cast %2 onto %3 water (%4 m, %5): bite in %6 s", key, cast.m_iCastId, water, reach, SpotLabel(cast), seconds));
		Toast(cast.m_iPlayerId, "#LF-Fishing_CastOut");
		Event(cast.m_iPlayerId, LF_EFishingEvent.CAST_OK, cast.m_iCastId);
		return LF_EFishingRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The first carried bait item of any bait in the config.
	protected static bool FindBait(IEntity character, LF_FishingConfig config, out LF_FishingBaitDef bait)
	{
		bait = null;
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
			return false;

		foreach (LF_FishingBaitDef def : config.m_aBaits)
		{
			if (!def)
				continue;

			if (!LF_InventoryUtils.FindItemsByPrefab(inventory, def.m_Prefab).IsEmpty())
			{
				bait = def;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one item of `bait` out of the inventory. \return False when there is none left.
	protected static bool SpendBait(IEntity character, LF_FishingBaitDef bait)
	{
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory || !bait)
			return false;

		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(inventory, bait.m_Prefab);
		if (items.IsEmpty())
			return false;

		return inventory.TryDeleteItem(items[0]);
	}

	//------------------------------------------------------------------------------------------------
	protected static string SpotLabel(LF_FishingCast cast)
	{
		if (cast.m_sSpotTable.IsEmpty())
			return "open water";

		return "spot table " + cast.m_sSpotTable;
	}

	//------------------------------------------------------------------------------------------------
	//! Timer: the bite of a cast. A cast that was ended or replaced since is not bitten.
	static void OnBiteTimer(string key, int castId)
	{
		Bite(key, castId, LF_JobGuards.Now());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the bite of cast `castId` at `now`. The cast must still be this one, not yet bitten, and
	//! its player must still hold the rod near where they cast; else it ends with nothing spent.
	//! \return True when the bite happened and the owner was told "Strike!".
	static bool Bite(string key, int castId, float now)
	{
		LF_FishingCast cast = Open(key);
		if (!cast || cast.m_iCastId != castId || cast.m_bBitten)
			return false;

		IEntity character = FindCharacter(cast);
		if (!StillFishing(character, cast))
		{
			Cancel(key, character, "walked off or put the rod away");
			return false;
		}

		cast.m_bBitten = true;
		cast.m_fBittenAt = now;
		LF_FishingConfig config = LF_FishingConfig.Get();
		float expireMs = 500;
		if (config)
			expireMs = (config.m_fStrikeWindowSeconds + config.m_fStrikeSlackSeconds) * 1000 + EXPIRE_MARGIN_MS;

		GetGame().GetCallqueue().CallLater(OnExpireTimer, expireMs, false, key, castId);
		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("%1 cast %2 bites", key, castId));
		Toast(RecipientOf(cast, character), "#LF-Fishing_Bite");
		Event(RecipientOf(cast, character), LF_EFishingEvent.BITE, castId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Timer: a bite nobody struck. The cast ends and the bait is gone.
	static void OnExpireTimer(string key, int castId)
	{
		LF_FishingCast cast = Open(key);
		if (!cast || cast.m_iCastId != castId || !cast.m_bBitten)
			return;

		IEntity character = FindCharacter(cast);
		End(key);
		int playerId = RecipientOf(cast, character);
		if (!character)
		{
			Event(playerId, LF_EFishingEvent.ENDED, castId);
			return;
		}

		SpendBait(character, cast.m_Bait);
		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("%1 cast %2 bite went unstruck: the bait is gone", key, castId));
		Toast(playerId, "#LF-Fishing_Missed");
		Event(playerId, LF_EFishingEvent.MISSED, castId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the owner's strike for cast `castId` at `now`. Ignored (nothing spent) with no open cast,
	//! another cast id, or before the server's own bite: the client's timing is never trusted. After the
	//! bite, the window plus the latency slack decides: inside it the fish is rolled, given and booked;
	//! after it the bait is gone and nothing is given.
	static LF_EFishingStrike Strike(IEntity character, int castId, float now)
	{
		if (!character)
			return LF_EFishingStrike.IGNORED;

		string key = KeyOf(character);
		LF_FishingCast cast = Open(key);
		if (!cast || cast.m_iCastId != castId || !cast.m_bBitten)
		{
			LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("strike ignored for %1: no bite to strike (cast %2)", key, castId));
			return LF_EFishingStrike.IGNORED;
		}

		LF_FishingConfig config = LF_FishingConfig.Get();
		if (!config)
		{
			Cancel(key, character, "the fishing config is gone");
			return LF_EFishingStrike.CANCELLED;
		}

		if (!StillFishing(character, cast) || !HasBait(character, cast.m_Bait))
		{
			Cancel(key, character, "lost the rod or the bait");
			return LF_EFishingStrike.CANCELLED;
		}

		End(key);
		if (LF_FishingRules.Strike(config, now - cast.m_fBittenAt) != LF_EFishStrike.HIT)
		{
			SpendBait(character, cast.m_Bait);
			LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("%1 struck late on cast %2: the bait is gone", key, castId));
			Toast(RecipientOf(cast, character), "#LF-Fishing_Missed");
			Event(RecipientOf(cast, character), LF_EFishingEvent.MISSED, castId);
			return LF_EFishingStrike.LATE;
		}

		SpendBait(character, cast.m_Bait);
		return Land(character, cast, config, now);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasBait(IEntity character, LF_FishingBaitDef bait)
	{
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory || !bait)
			return false;

		return !LF_InventoryUtils.FindItemsByPrefab(inventory, bait.m_Prefab).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! A strike that landed: pick the fish, give it, thin the spot, book the XP.
	protected static LF_EFishingStrike Land(IEntity character, LF_FishingCast cast, LF_FishingConfig config, float now)
	{
		string key = cast.m_sKey;
		LF_FishingSpotComponent spot;
		int stockLeft = 0;
		if (!cast.m_sSpotTable.IsEmpty())
		{
			spot = LF_FishingSpotComponent.Of(GetGame().GetWorld().FindEntityByID(cast.m_SpotId));
			if (spot)
				stockLeft = spot.StockLeft(now);
		}

		int level = LF_JobLevels.LevelOf(key, JOB);
		string spotTable = "";
		if (spot)
			spotTable = cast.m_sSpotTable;

		LF_FishCatch pick = LF_FishingRules.PickCatch(config, spotTable, stockLeft, cast.m_sWater, CurrentHour(), level, cast.m_Bait, Math.RandomFloat01(), Math.RandomFloat01());
		if (!pick || pick.m_Prefab.IsEmpty())
		{
			LF_Debug.Error(LF_FishingConfig.FEATURE, string.Format("%1 struck in time but nothing can be given (no catch, or a species with no prefab for that size)", key));
			Toast(RecipientOf(cast, character), "#LF-Fishing_Empty");
			Event(RecipientOf(cast, character), LF_EFishingEvent.MISSED, cast.m_iCastId);
			return LF_EFishingStrike.EMPTY;
		}

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		int added = LF_InventoryUtils.AddAmount(inventory, pick.m_Prefab, 1, true);
		if (added < 1)
		{
			LF_Debug.Error(LF_FishingConfig.FEATURE, string.Format("%1 caught %2 but it could not be given or dropped", key, pick.m_Prefab));
			Toast(RecipientOf(cast, character), "#LF-Fishing_Empty");
			Event(RecipientOf(cast, character), LF_EFishingEvent.MISSED, cast.m_iCastId);
			return LF_EFishingStrike.EMPTY;
		}

		if (spot && !pick.m_bOpenWater)
			spot.TakeFish(now);

		LF_JobLevels.Award(key, XP_ACTION);
		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("%1 caught %2 (%3, size %4, table %5)", key, pick.m_Prefab, pick.m_Species.m_sId, pick.m_iSize, pick.m_sTable));
		Toast(RecipientOf(cast, character), "#LF-Fishing_Caught");
		Event(RecipientOf(cast, character), LF_EFishingEvent.CAUGHT, cast.m_iCastId);
		return LF_EFishingStrike.CAUGHT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the player still holds the rod and has not walked off from where they cast.
	protected static bool StillFishing(IEntity character, LF_FishingCast cast)
	{
		if (!character || !LF_Utils.IsAliveState(character))
			return false;

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || chimera.IsInVehicle())
			return false;

		if (!LF_HandTool.Holds(character, TOOL_ID))
			return false;

		return vector.DistanceSq(character.GetOrigin(), cast.m_vOrigin) <= MOVE_ENDS_CAST_METRES * MOVE_ENDS_CAST_METRES;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a cast with nothing spent and tells the owner.
	protected static void Cancel(string key, IEntity character, string reason)
	{
		LF_FishingCast cast = Open(key);
		End(key);
		LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("%1 cast ended: %2", key, reason));
		if (cast)
			Event(RecipientOf(cast, character), LF_EFishingEvent.ENDED, cast.m_iCastId);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the open cast of this key (its timers find nothing and do nothing).
	static void End(string key)
	{
		if (s_mCasts && !key.IsEmpty())
			s_mCasts.Remove(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a player's cast and cast times when they leave, and every cast at world end.
	static void ResetStatics()
	{
		s_mCasts = null;
		s_mCastTimes = null;
		s_iNextCastId = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The hour of day, 0 to 23, on the world clock (noon when there is none).
	static int CurrentHour()
	{
		return Math.ClampInt(LF_Atmosphere.HourNow(), 0, 23);
	}

	//------------------------------------------------------------------------------------------------
	//! The body of this cast: the entity it was cast from, or null once that is gone.
	protected static IEntity FindCharacter(LF_FishingCast cast)
	{
		if (!cast || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(cast.m_CharacterId);
	}

	//------------------------------------------------------------------------------------------------
	static int PlayerIdOf(IEntity character)
	{
		if (!character)
			return 0;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return 0;

		return players.GetPlayerIdFromControlledEntity(character);
	}

	//------------------------------------------------------------------------------------------------
	//! The player told about `cast`: the one who cast it. A body that died or was swapped since maps to
	//! no player, and an event sent through it was lost: the client kept its line out for good (fire
	//! only struck, every strike ignored) after a death mid-cast.
	static int RecipientOf(LF_FishingCast cast, IEntity character)
	{
		if (cast && cast.m_iPlayerId > 0)
			return cast.m_iPlayerId;

		return PlayerIdOf(character);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Toast(int playerId, string text)
	{
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Fishing_Title", text, 4.0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Event(int playerId, LF_EFishingEvent kind, int castId)
	{
		if (playerId <= 0)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (controller)
			controller.LF_FishingEvent(kind, castId);
	}

	//------------------------------------------------------------------------------------------------
	protected static string RefusalText(LF_EFishingRefusal why)
	{
		switch (why)
		{
			case LF_EFishingRefusal.NO_ROD:
				return "#LF-Fishing_NoRod";
			case LF_EFishingRefusal.LEVEL:
				return "#LF-Fishing_Level";
			case LF_EFishingRefusal.RATE:
				return "#LF-Fishing_Rate";
			case LF_EFishingRefusal.ALREADY_OUT:
				return "#LF-Fishing_AlreadyOut";
			case LF_EFishingRefusal.RANGE:
				return "#LF-Fishing_Range";
			case LF_EFishingRefusal.NOT_WATER:
				return "#LF-Fishing_NotWater";
			case LF_EFishingRefusal.NO_BAIT:
				return "#LF-Fishing_NoBait";
		}

		return "#LF-Fishing_Cannot";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FishingSession's statics (LF_StaticReset finds it by reflection).
class LF_FishingSession_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FishingSession";
	}

	override void Apply()
	{
		LF_FishingSession.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for fishing, on the PlayerController (the same seat as the interaction
//! menu's). The server takes the angler from this controller's controlled entity, never from the
//! client, and `LF_FishingSession` re-checks everything.
//!   owner -> server : LF_AskFishingCast(target), LF_AskFishingStrike(castId)
//!   server -> owner : LF_FishingEvent(event, castId)
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: cast the line at a flat world position.
	void LF_AskFishingCast(vector target)
	{
		if (Replication.IsServer())
			RpcAsk_LF_FishingCast(target);
		else
			Rpc(RpcAsk_LF_FishingCast, target);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FishingCast(vector target)
	{
		// A target that is not a number must not reach the maths (a forged payload).
		if (!LF_FishingSession.ValidTarget(target))
		{
			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("cast from player %1 dropped: target is not finite", GetPlayerId()));
			return;
		}

		IEntity angler = GetControlledEntity();
		if (!angler || vector.DistanceSq(angler.GetOrigin(), target) > LF_FishingSession.MAX_ASK_METRES * LF_FishingSession.MAX_ASK_METRES)
		{
			LF_Debug.Warn(LF_FishingConfig.FEATURE, string.Format("cast from player %1 dropped: no body, or the target is far from it", GetPlayerId()));
			return;
		}

		LF_FishingSession.CastAt(angler, target, LF_JobGuards.Now());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: strike the open cast `castId`.
	void LF_AskFishingStrike(int castId)
	{
		if (Replication.IsServer())
			RpcAsk_LF_FishingStrike(castId);
		else
			Rpc(RpcAsk_LF_FishingStrike, castId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_FishingStrike(int castId)
	{
		LF_FishingSession.Strike(GetControlledEntity(), castId, LF_JobGuards.Now());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell this player's machine what happened to their cast.
	void LF_FishingEvent(int kind, int castId)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_FishingEvent(kind, castId);
		else
			Rpc(RpcDo_LF_FishingEvent, kind, castId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_FishingEvent(int kind, int castId)
	{
		LF_FishingRodComponent.OnServerEvent(kind, castId);
	}
}
