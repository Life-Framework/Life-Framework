//! What GetPlayerUID uses when the platform has no identity for a player.
enum LF_EIdentityFallback
{
	WAIT,
	PLAYER_NAME,
	SESSION_SLOT
}

//! One QueryEntitiesBySphere pass for LF_Utils.GetNearbyVehicles and GetNearbyCharacters. The hits go
//! into the caller's array, so no static outlives the call.
class LF_NearbyEntityCollector : Managed
{
	protected array<IEntity> m_aFound; // the caller owns it
	protected string m_sRequiredComponent;

	//------------------------------------------------------------------------------------------------
	void LF_NearbyEntityCollector(array<IEntity> found, string requiredComponent = "")
	{
		m_aFound = found;
		m_sRequiredComponent = requiredComponent;
	}

	//------------------------------------------------------------------------------------------------
	bool Insert(IEntity entity)
	{
		if (!entity)
			return true;

		m_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool FilterVehicle(IEntity entity)
	{
		if (!entity || !entity.IsInherited(Vehicle))
			return false;

		if (m_sRequiredComponent.IsEmpty())
			return true;

		typename componentType = m_sRequiredComponent.ToType();
		if (!componentType)
			return true;

		return entity.FindComponent(componentType) != null;
	}

	//------------------------------------------------------------------------------------------------
	bool FilterCharacter(IEntity entity)
	{
		return !!SCR_ChimeraCharacter.Cast(entity);
	}
}

class LF_Utils : Managed
{
	static const int MAX_PREFAB_ANCESTRY = 12; // lf-fixed: a loop guard on the ancestor walk, not a tunable

	//! Integer min, max and clamp for anything that can be large: unix seconds, millisecond stamps, money,
	//! XP, running counters. Enforce `Math.Max`, `Math.Min` and `Math.Clamp` work in floats, so above
	//! 2^24 (16,777,216) an int loses its low bits (unix 1800000480 comes back as 1800000512, #2787).
	//! `validate-int-time-math` flags a time-named argument handed to them.

	//------------------------------------------------------------------------------------------------
	static int MaxInt(int a, int b)
	{
		if (a > b) return a;
		return b;
	}

	//------------------------------------------------------------------------------------------------
	static int MinInt(int a, int b)
	{
		if (a < b) return a;
		return b;
	}

	//------------------------------------------------------------------------------------------------
	//! value limited to lo..hi without a float round trip. A reversed range (lo above hi) returns hi.
	static int ClampInt(int value, int lo, int hi)
	{
		if (value < lo) value = lo;
		if (value > hi) value = hi;
		return value;
	}

	//! Tests only: the body (entity id as text) that stands in for a player, and its uid.
	protected static string s_sTestUidBody;
	protected static string s_sTestUid;

	//------------------------------------------------------------------------------------------------
	//! Tests only: gives `body` (a test character no player controls) a player uid for GetPlayerUID,
	//! as LF_QuestTrackerComponent.SetUidForTests does for quests. A null body or an empty uid clears it.
	static void SetUidForTests(IEntity body, string uid)
	{
		s_sTestUidBody = "";
		s_sTestUid = "";
		if (!body || uid.IsEmpty())
			return;

		s_sTestUidBody = body.GetID().ToString();
		s_sTestUid = uid;
	}

	//------------------------------------------------------------------------------------------------
	static array<IEntity> GetNearbyVehicles(vector center, float radius, string requiredComponent = "")
	{
		array<IEntity> found = new array<IEntity>();
		LF_NearbyEntityCollector collector = new LF_NearbyEntityCollector(found, requiredComponent);
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, collector.Insert, collector.FilterVehicle);
		return found;
	}

	//------------------------------------------------------------------------------------------------
	static array<IEntity> GetNearbyCharacters(vector center, float radius)
	{
		array<IEntity> found = new array<IEntity>();
		LF_NearbyEntityCollector collector = new LF_NearbyEntityCollector(found);
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, collector.Insert, collector.FilterCharacter);
		return found;
	}

	//------------------------------------------------------------------------------------------------
	static void Notify(string message, string title = "", float duration = 3.0)
	{
		NoteToast(message, title, duration);
	}

	//------------------------------------------------------------------------------------------------
	//! A note on this machine in the cream note style (first-2-hours review C11: the vanilla hint was a dark
	//! panel with an orange heading beside the cream quest notes). Falls back to the vanilla hint when the
	//! game mode has no notification manager (a world without it).
	static void NoteToast(string message, string title = "", float duration = 3.0, LF_ENotificationType type = LF_ENotificationType.INFO)
	{
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (manager)
		{
			manager.ShowNotificationToast(title, message, duration, type);
			return;
		}

		SCR_HintManagerComponent.ShowCustomHint(LF_Terms.Shown(message), LF_Terms.Shown(title), duration);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: toast for the player controlling `player`, delivered to that player only
	//! (owner RPC on their controller; a direct call for a listen host). Notify above shows a
	//! hint on the machine that calls it, which on a dedicated server is nobody.
	//! A client call is a no-op: the server copy of the same action sends the toast.
	//! \param messageArg Fills %1 in a #key message on the receiving client.
	//! \param slot A toast with a slot replaces the one still showing in that slot (LF_NotificationManagerComponent.SLOT_STEP).
	static void NotifyEntity(IEntity player, string message, string title = "", float duration = 3.0, string messageArg = "", string slot = "")
	{
		if (!Replication.IsServer() || !player)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
		{
			LF_Debug.Log("Notify", string.Format("notify skipped: %1 is not a player (%2)", player, message));
			return;
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, title, message, duration, LF_ENotificationType.INFO, messageArg, slot);
	}

	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraCharacter GetLocalCharacter()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return SCR_ChimeraCharacter.Cast(controller.GetControlledEntity());
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterId(SCR_ChimeraCharacter character)
	{
		if (!character)
			return "";

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";

		int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
		if (playerId > 0)
		{
			// Get the actual Steam UID, not just the player ID
			string uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
			if (!uid.IsEmpty())
				return uid;
		}

		return "";
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterId(IEntity entity)
	{
		return GetCharacterId(SCR_ChimeraCharacter.Cast(entity));
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterName(SCR_ChimeraCharacter character)
	{
		if (!character)
			return "";

		CharacterIdentityComponent identityComp = CharacterIdentityComponent.Cast(character.FindComponent(CharacterIdentityComponent));
		if (identityComp)
		{
			Identity identity = identityComp.GetIdentity();
			if (identity && identity.GetFullName())
				return identity.GetFullName();
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
			if (playerId > 0)
			{
				string playerName = playerManager.GetPlayerName(playerId);
				if (playerName && !playerName.IsEmpty())
					return playerName;
			}
		}

		return "";
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterName(IEntity entity)
	{
		return GetCharacterName(SCR_ChimeraCharacter.Cast(entity));
	}

//------------------------------------------------------------------------------------------------
	static bool IsCharacterLocal(SCR_ChimeraCharacter character)
	{
		if (!character)
			return false;

		RplComponent rplComponent = RplComponent.Cast(character.FindComponent(RplComponent));
		if (!rplComponent)
			return false;

		return rplComponent.IsOwner();
	}

//------------------------------------------------------------------------------------------------
	static bool IsCharacterLocal(IEntity entity)
	{
		return IsCharacterLocal(SCR_ChimeraCharacter.Cast(entity));
	}

	//------------------------------------------------------------------------------------------------
	//! Recursively change color on entity and all children using material parameters
	static void ChangeColorRecursive(IEntity entity, Color color)
	{
		if (!entity)
			return;
        
		// Try to set color via signals manager (for vehicles with paintable surfaces)
		SignalsManagerComponent signalsManager = SignalsManagerComponent.Cast(entity.FindComponent(SignalsManagerComponent));
		if (signalsManager)
		{
			// Common vehicle color signals
			int signalR = signalsManager.AddOrFindSignal("VehicleColorR");
			int signalG = signalsManager.AddOrFindSignal("VehicleColorG");
			int signalB = signalsManager.AddOrFindSignal("VehicleColorB");
            
			if (signalR >= 0)
				signalsManager.SetSignalValue(signalR, color.R() / 255.0);
			if (signalG >= 0)
				signalsManager.SetSignalValue(signalG, color.G() / 255.0);
			if (signalB >= 0)
				signalsManager.SetSignalValue(signalB, color.B() / 255.0);
		}
        
		// Apply to children recursively
		IEntity child = entity.GetChildren();
		while (child)
		{
			ChangeColorRecursive(child, color);
			child = child.GetSibling();
		}
	}
    
	//------------------------------------------------------------------------------------------------
	//! The key the player's character-scoped records are saved under (bank, progression, wanted
	//! reports, tickets, vehicles, ...): the record key of the account's active character (#331).
	//! Slot 1 of an account is keyed by the account uid, so with one character per account this is
	//! the uid, as it always was. Account-level checks (whitelists, bans, staff tools) use
	//! GetAccountUID. A client reads the key the server replicated onto the player's character.
	//! \param playerId Index of the player inside player manager
	//! \return the record key, or "" while the identity has not resolved
	static string GetPlayerUID(int playerId)
	{
		string accountUid = GetAccountUID(playerId);
		if (accountUid.IsEmpty())
			return accountUid;

		if (!Replication.IsServer())
		{
			string replicated = ReplicatedCharacterKey(playerId);
			if (!replicated.IsEmpty())
				return replicated;

			return accountUid;
		}

		return LF_PlayerAccountManager.ResolveCharacterKey(accountUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReplicatedCharacterKey(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
		if (!character)
			return "";

		return character.LF_GetCharacterKey();
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the platform-stable identity id of a player (the player's Steam/Bohemia UID): the
	//! account key. Whitelists, bans and staff tools use it; character records use GetPlayerUID.
	//! \param playerId Index of the player inside player manager
	//! \return the uid as string
	static string GetAccountUID(int playerId)
	{
		if (playerId <= 0)
			return "";

		// The backend can answer with "", or transiently with the NULL UUID ("00000000-..."), and on
		// a non-dedicated host vanilla turns "" into a name-derived "00bbbddd-" stand-in. None of
		// them is the player's own identity, which can still arrive seconds later on a listen host:
		// LF_PlayerIdentity decides whether to wait or what stands in for it.
		string identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);

		bool sessionDefine = false;
#ifdef LF_SESSION_IDENTITY
		sessionDefine = true;
#endif
		// A session slot goes to a connected player only: a late lookup for a player id that already left
		// (a callback queued before the disconnect) must not claim a slot nobody releases.
		if (sessionDefine && Replication.IsServer() && !LF_PlayerIdentity.HoldsSlot(playerId) && !GetGame().GetPlayerManager().IsPlayerConnected(playerId))
			return "";

		bool noBackend = System.IsCLIParam("noBackend");
#ifdef WORKBENCH
		// Workbench play has no online service: vanilla's peer-tool polyfill is all it gets.
		noBackend = true;
#endif
		// A client never learns platform identities (vanilla answers NULL there): it keeps the name
		// stand-in, and GetPlayerUID reads the key the server replicated instead.
		if (!Replication.IsServer())
			noBackend = true;

		return LF_PlayerIdentity.Resolve(playerId, identity, RplSession.Mode() == RplMode.Dedicated, sessionDefine, noBackend, System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! What stands in for a missing platform identity. Pure, so it is LOGIC-tier tested
	//! (LF_Test_IdentityFallback).
	//! - Dedicated with LF_SESSION_IDENTITY (agent-box/session.mjs): every client is the same
	//!   Steam account with the same name, so the player id keys it, for one server run.
	//! - -noBackend (tools\cli host start) or Workbench play: no identity will ever arrive. Each
	//!   friend joins with their own Steam name, so the name keys the account and survives a
	//!   restart. Two players with the same name share one account.
	//! - Dedicated with a backend: wait for the identity (the creation flow and the spawn retry on
	//!   an empty key). A dedicated server never keys a player by a name anyone can take.
	//! - Listen host with a backend: the identity arrives late (about 15 s on playtest build 4), so
	//!   wait for it; after LF_PlayerIdentity.LISTEN_WAIT_MS with no answer the online service is
	//!   down, and the name keys the account as in offline play.
	//! \param waitedMs How long this player has been waiting for a real identity.
	static LF_EIdentityFallback ChooseIdentityFallback(bool dedicated, bool sessionDefine, bool noBackend, int waitedMs)
	{
		if (dedicated && sessionDefine)
			return LF_EIdentityFallback.SESSION_SLOT;

		if (noBackend)
			return LF_EIdentityFallback.PLAYER_NAME;

		if (dedicated || waitedMs < LF_PlayerIdentity.LISTEN_WAIT_MS)
			return LF_EIdentityFallback.WAIT;

		return LF_EIdentityFallback.PLAYER_NAME;
	}

	//------------------------------------------------------------------------------------------------
	//! Identity for a multi-client harness session (LF_SESSION_IDENTITY): unique per player id,
	//! stable only within one server run.
	//! \param playerId Index of the player inside player manager
	//! \return a UUID-shaped identity, never empty
	static string BuildSessionIdentity(int playerId)
	{
		string digits = playerId.ToString(12, true);
		return "00bbbddd-5e55-0000-0000-" + digits;
	}

	//------------------------------------------------------------------------------------------------
	//! Derives a stable per-player identity from the player name when no backend identity
	//! exists (offline sessions). Mirrors vanilla's peer-tool polyfill so the result matches
	//! what vanilla would produce for the same name.
	//! \param playerId Index of the player inside player manager
	//! \return a stable identity string, never empty
	static string BuildLocalIdentity(int playerId)
	{
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (playerName.IsEmpty())
			playerName = "LocalPlayer" + playerId;

		const int splitLength = Math.Max(1, playerName.Length() / 3);
		const string split1 = Math.AbsInt(playerName.Substring(0, splitLength).Hash()).ToString(8, true);
		const string split2 = Math.AbsInt(playerName.Substring(splitLength, splitLength).Hash()).ToString(8, true);
		const int doubleSplit = splitLength * 2;
		const string split3 = Math.AbsInt(playerName.Substring(doubleSplit, playerName.Length() - doubleSplit).Hash()).ToString(8, true);

		string uid = string.Format("00bbbddd-%1-%2-%3-%4%5", split1.Substring(0, 4), split1.Substring(4, 4), split2.Substring(0, 4), split2.Substring(4, 4), split3);
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! The account uid of the player controlling this entity (see GetAccountUID(int)).
	static string GetAccountUID(IEntity player)
	{
		if (!player)
			return "";

#ifdef LF_HARNESS
		string filmStandIn = LF_FilmIdentity.Get(player);
		if (!filmStandIn.IsEmpty())
			return filmStandIn;
#endif

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";

		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
			return "";

		return GetAccountUID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The record key of the active character of the player controlling this entity (see
	//! GetPlayerUID(int)).
	//! \param player Instance of the player
	//! \return the key as string
	static string GetPlayerUID(IEntity player)
	{
		if (!player)
			return "";

		if (!s_sTestUidBody.IsEmpty() && player.GetID().ToString() == s_sTestUidBody)
			return s_sTestUid;

#ifdef LF_HARNESS
		// Film actors (LF_FilmDirector) have no player; the harness build gives them a stand-in uid.
		string standIn = LF_FilmIdentity.Get(player);
		if (!standIn.IsEmpty())
			return standIn;
#endif

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";

		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
			return "";

		return GetPlayerUID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server check for a request that needs a living talker: the character's life state is ALIVE.
	//! A dead or downed body, or no body, is not (a client could ask from a corpse).
	static bool IsAliveState(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return false;

		// The damage manager is destroyed the moment the character dies; the controller's life state
		// follows a frame later. Either one means not alive.
		DamageManagerComponent damage = DamageManagerComponent.Cast(chimera.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return false;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return false;

		return controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}

	//------------------------------------------------------------------------------------------------
	//! Compatibility alias: some code uses GetPlayerUid (lowercase d)
	static string GetPlayerUid(IEntity player)
	{
		return GetPlayerUID(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Get player Steam UID (not character ID) - for account lookups
	static string GetPlayerSteamUID(IEntity player)
	{
		if (!player)
			return "";
            
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";
        
		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId > 0)
		{
			string uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
			if (!uid.IsEmpty())
				return uid;
		}
        
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Get player Steam UID from player ID - for account lookups
	static string GetPlayerSteamUID(int playerId)
	{
		if (playerId <= 0)
			return "";
            
		string uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Get player name (wrapper for GetCharacterName)
	static string GetPlayerName(IEntity player)
	{
		return GetCharacterName(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a prefab
	//! \param prefab ResournceName of the prefab to be spawned
	//! \param origin Position(origin) where to spawn the entity
	//! \param orientation Angles(yaw, pitch, rolle in degrees) to apply to the entity
	//! \param global If true the entity is spawned in the global (networked) space
	//! \param parent Optional parent entity to attach the spawned entity to
	//! \return the spawned entity or null on failure
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true, IEntity parent = null)
	{
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;
		spawnParams.Parent = parent;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error("Utils", string.Format("spawn failed: prefab does not resolve: %1", prefab));
			return null;
		}

		// A spawn transform replaces the prefab's own, scale included: a 0.55 money bag came out as a 1.4 m sack.
		// The scale goes into the matrix (EntitySpawnParams.Scale alone left it at 1).
		float scale = PrefabScale(resource);
		for (int axis = 0; axis < 3; axis++)
			spawnParams.Transform[axis] = spawnParams.Transform[axis] * scale;

		if (!global)
			return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), spawnParams);

		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	//! The root scale a prefab declares (its own `scale` or an inherited one), 1 when it sets none.
	static float PrefabScale(Resource resource)
	{
		if (!resource || !resource.IsValid())
			return 1;

		BaseContainer source = resource.GetResource().ToBaseContainer();
		float scale = 1;
		if (!source || !source.Get("scale", scale) || scale <= 0)
			return 1;

		return scale;
	}

	//------------------------------------------------------------------------------------------------
	//! Remaps every material on the entity's current mesh to the given material
	//! \param entity Entity whose mesh materials are replaced
	//! \param newMaterial ResourceName of the material to remap to
	static void ChangeEntityMaterial(IEntity entity, ResourceName newMaterial)
	{
		if (!entity || !newMaterial)
			return;

		VObject mesh = entity.GetVObject();
		if (!mesh)
			return;

		string remap;
		string materials[256];
		int numMats = mesh.GetMaterials(materials);
		if (numMats == 0)
			return;

		for (int i = 0; i < numMats; i++)
		{
			remap += string.Format("$remap '%1' '%2';", materials[i], newMaterial);
		}

		entity.SetObject(mesh, remap);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the MeshObject component of a prefab source (no instance needed)
	//! \param prefab Prefab to inspect
	//! \return the MeshObject BaseContainer or null when the prefab has none
	static BaseContainer GetPrefabMeshComponent(ResourceName prefab)
	{
		BaseContainer meshComponent;
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		IEntitySource prefabSource = resource.GetResource().ToEntitySource();
		if (!prefabSource)
			return null;

		int count = prefabSource.GetComponentCount();

		for (int i = 0; i < count; i++)
		{
			IEntityComponentSource comp = prefabSource.GetComponent(i);

			if (comp.GetClassName() == "MeshObject")
			{
				meshComponent = comp;
				break;
			}
		}
		return meshComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the prefab the entity uses
	//! \param entity Instance of which to get the prefab name
	//! \return the resource name of the prefab or empty string if no prefab was used or entity is invalid
	static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity) return ResourceName.Empty;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData) return ResourceName.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(prefabData.GetPrefab());
	}

	//------------------------------------------------------------------------------------------------
	//! The GUID of "{GUID}path", upper-cased. A name without a GUID (a bare path) comes back whole,
	//! upper-cased: its path is the only key it has.
	static string PrefabGuid(string resourceName)
	{
		string text = resourceName;
		int close = GuidEnd(text);
		if (close > 1)
			text = text.Substring(1, close - 1);

		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Same prefab: two resource names that name one resource. A resource keeps its GUID when it
	//! moves to another folder or addon, but GetPrefabName() then reports the new path
	//! (docs/architecture/pack-move-spike.md section 3). So the GUID decides, and a name saved or
	//! configured before the move still matches the live entity after it. Only when a side has no
	//! GUID does the path decide, ignoring case. An empty name is never the same as anything.
	static bool SamePrefab(string a, string b)
	{
		if (a.IsEmpty() || b.IsEmpty())
			return false;

		if (GuidEnd(a) > 1 && GuidEnd(b) > 1)
			return PrefabGuid(a) == PrefabGuid(b);

		return PrefabPathKey(a) == PrefabPathKey(b);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab `name` is `ancestor` or a prefab that inherits it. A tier of a tool (worn,
	//! standard and fine pickaxe, #4188) is its base tool for every rule that asks "which tool is this",
	//! so a node, a gather and a rod row name the base and every tier counts (#4790). An empty name
	//! or ancestor is never the same as anything; a prefab that does not load inherits nothing.
	static bool PrefabInherits(string name, string ancestor)
	{
		if (name.IsEmpty() || ancestor.IsEmpty())
			return false;

		if (SamePrefab(name, ancestor))
			return true;

		Resource resource = Resource.Load(name);
		if (!resource || !resource.IsValid())
			return false;

		return ContainerInherits(resource.GetResource().ToBaseContainer(), ancestor);
	}

	//------------------------------------------------------------------------------------------------
	//! \ref PrefabInherits(string, string), for a live entity: its own prefab and every ancestor of it.
	static bool EntityInherits(IEntity entity, string ancestor)
	{
		if (!entity || ancestor.IsEmpty())
			return false;

		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return false;

		return ContainerInherits(data.GetPrefab(), ancestor);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ContainerInherits(BaseContainer prefab, string ancestor)
	{
		int depth = 0;
		while (prefab && depth < MAX_PREFAB_ANCESTRY)
		{
			if (SamePrefab(prefab.GetResourceName(), ancestor))
				return true;

			prefab = prefab.GetAncestor();
			depth++;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The index of the "}" closing a leading "{GUID}", or -1 when the name has none.
	protected static int GuidEnd(string resourceName)
	{
		if (!resourceName.StartsWith("{"))
			return -1;

		return resourceName.IndexOf("}");
	}

	//------------------------------------------------------------------------------------------------
	//! The path of "{GUID}path" (the whole name when it has no GUID), upper-cased.
	protected static string PrefabPathKey(string resourceName)
	{
		string text = resourceName;
		int close = GuidEnd(text);
		if (close > 0)
			text = text.Substring(close + 1, text.Length() - close - 1);

		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAnyInherited(typename type, notnull array<typename> from)
	{
		if (type)
		{
			foreach (typename candiate : from)
			{
				if (type.IsInherited(candiate))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInstanceAnyInherited(Managed instance, notnull array<typename> from)
	{
		if (!instance)
			return false;

		typename type = instance.Type();
		return IsAnyInherited(type, from);
	}
};

//------------------------------------------------------------------------------------------------
//! World-end reset of the test uid stand-in in LF_Utils (LF_StaticReset finds it by reflection).
class LF_Utils_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Utils";
	}

	override void Apply()
	{
		LF_Utils.SetUidForTests(null, "");
	}
}
