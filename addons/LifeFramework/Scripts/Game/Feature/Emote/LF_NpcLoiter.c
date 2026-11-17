// LF_NpcLoiter.c - town NPCs sit on benches and lean like players do (#1928, the last open item of #982).
// An NPC of a loiter role takes a free seat near where it stands (the same LF_SitAction places a player
// sits on), else leans or smokes, and gives the seat up when it dies, leaves or is deleted. Server only:
// the loiter and the seat's occupant replicate through the base game exactly as a player's sit does.

//! What a loitering NPC is doing.
enum LF_ENpcLoiter
{
	NONE,
	SEAT,
	LEAN,
	SMOKE
}

//------------------------------------------------------------------------------------------------
//! One loitering NPC: what it does, which seat entity it holds and when it started.
class LF_NpcLoiterState : Managed
{
	LF_ENpcLoiter m_eKind;
	EntityID m_SeatId = EntityID.INVALID;
	int m_iSinceMs;
}

//------------------------------------------------------------------------------------------------
class LF_NpcLoiterQuery
{
	ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	bool Add(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_NpcLoiter
{
	static const string FEATURE = "Emote";

	//! The roles that pass the time on a bench or a wall; the others stand at their post.
	protected static const string LOITER_ROLES = "|townsfolk|farmer|miner|logger|";
	//! How far from where it stands an NPC looks for a seat, in metres.
	static const float SEAT_RANGE = 5; // lf-fixed: the reach of one bench, not a server rule
	static const int WATCH_MS = 5000; // lf-fixed: a housekeeping tick, not tuning
	//! A loiter needs a moment to start; the watch does not read "not loitering" before this.
	static const int GRACE_MS = 4000; // lf-fixed: the time a loiter needs to begin

	protected static ref map<EntityID, ref LF_NpcLoiterState> s_mState;
	protected static bool s_bWatching;

	//------------------------------------------------------------------------------------------------
	//! Called from LF_NpcLoiter_StaticResetHook when a world ends.
	static void ResetStatics()
	{
		if (s_bWatching && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Watch);

		s_bWatching = false;
		s_mState = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: does this role pass the time on a bench.
	static bool IsLoiterRole(string role)
	{
		if (role.IsEmpty())
			return false;

		return LOITER_ROLES.Contains("|" + role + "|");
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: a free seat is taken; with none, the seed splits NPCs between leaning
	//! and smoking so a wall of them does not stand in one pose.
	static LF_ENpcLoiter Choose(bool seatFree, int seed)
	{
		if (seatFree)
			return LF_ENpcLoiter.SEAT;

		if (seed % 2 == 0)
			return LF_ENpcLoiter.LEAN;

		return LF_ENpcLoiter.SMOKE;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<EntityID, ref LF_NpcLoiterState> State()
	{
		if (!s_mState)
			s_mState = new map<EntityID, ref LF_NpcLoiterState>();

		return s_mState;
	}

	//------------------------------------------------------------------------------------------------
	//! What this NPC is doing now.
	static LF_ENpcLoiter KindOf(IEntity npc)
	{
		if (!npc || !s_mState)
			return LF_ENpcLoiter.NONE;

		LF_NpcLoiterState state = s_mState.Get(npc.GetID());
		if (!state)
			return LF_ENpcLoiter.NONE;

		return state.m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSeated(IEntity npc)
	{
		return KindOf(npc) == LF_ENpcLoiter.SEAT;
	}

	//------------------------------------------------------------------------------------------------
	//! The seat reads taken: someone (an NPC here, or a player through the base game occupancy) holds it.
	static bool IsTaken(LF_SitAction seat)
	{
		return seat && seat.IsOccupied();
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest free sit place within range of the NPC, or null. Found by component, so any prop a
	//! map or pack made a seat counts.
	static LF_SitAction FindFreeSeat(IEntity npc, float range = SEAT_RANGE)
	{
		if (!npc)
			return null;

		LF_NpcLoiterQuery query = new LF_NpcLoiterQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(npc.GetOrigin(), range, query.Add, null, EQueryEntitiesFlags.ALL);

		LF_SitAction best;
		float bestDistance = range;
		foreach (IEntity prop : query.m_aFound)
		{
			BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(prop.FindComponent(BaseActionsManagerComponent));
			if (!manager)
				continue;

			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				LF_SitAction sit = LF_SitAction.Cast(action);
				if (!sit || IsTaken(sit))
					continue;

				vector at[4];
				sit.LF_GetSeatTransform(at);
				float distance = vector.Distance(at[3], npc.GetOrigin());
				if (distance < bestDistance)
				{
					best = sit;
					bestDistance = distance;
				}
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Seat the NPC on this place. False (and a log line) when it cannot.
	static bool Sit(IEntity npc, LF_SitAction seat)
	{
		if (!npc || !seat || !Replication.IsServer())
			return false;

		if (IsSeated(npc))
			return true;

		// An NPC never takes a seat someone holds, even before that body's loiter has begun.
		if (IsTaken(seat) || !seat.LF_SeatBody(npc))
		{
			LF_Debug.Warn(FEATURE, string.Format("npc loiter: %1 could not take seat %2", npc.GetID(), seat.GetOwner().GetID()));
			return false;
		}

		Track(npc, LF_ENpcLoiter.SEAT, seat.GetOwner().GetID());
		LF_Debug.Info(FEATURE, string.Format("npc loiter: %1 sits on %2", npc.GetID(), seat.GetOwner().GetID()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Lean or smoke where it stands.
	static bool Stand(IEntity npc, LF_ENpcLoiter kind)
	{
		if (!npc || !Replication.IsServer())
			return false;

		if (kind != LF_ENpcLoiter.LEAN && kind != LF_ENpcLoiter.SMOKE)
			return false;

		if (KindOf(npc) != LF_ENpcLoiter.NONE)
			return true;

		SCR_CharacterControllerComponent controller = Controller(npc);
		if (!controller)
		{
			LF_Debug.Error(FEATURE, string.Format("npc loiter: %1 has no character controller", npc.GetID()));
			return false;
		}

		ELoiteringType type = ELoiteringType.SMOKING;
		if (kind == LF_ENpcLoiter.LEAN)
			type = ELoiteringType.LEAN_LEFT;

		vector none[4];
		controller.StartLoitering(null, type, false, true, false, none);
		Track(npc, kind, EntityID.INVALID);
		LF_Debug.Info(FEATURE, string.Format("npc loiter: %1 %2", npc.GetID(), typename.EnumToString(LF_ENpcLoiter, kind)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The NPC takes a free seat near it, else leans or smokes. Returns what it ended up doing.
	static LF_ENpcLoiter Settle(IEntity npc)
	{
		if (!npc || !Replication.IsServer() || KindOf(npc) != LF_ENpcLoiter.NONE)
			return KindOf(npc);

		LF_SitAction seat = FindFreeSeat(npc);
		if (seat && Sit(npc, seat))
			return LF_ENpcLoiter.SEAT;

		LF_ENpcLoiter kind = Choose(false, LF_Roles.SeedHash(npc.GetName() + npc.GetID().ToString()));
		if (Stand(npc, kind))
			return kind;

		return LF_ENpcLoiter.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The NPC gives up its loiter and its seat. Safe to call for one that holds nothing.
	static void Release(IEntity npc, string why = "released")
	{
		if (!npc || !s_mState)
			return;

		ReleaseId(npc.GetID(), npc, why);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Track(IEntity npc, LF_ENpcLoiter kind, EntityID seatId)
	{
		LF_NpcLoiterState state = new LF_NpcLoiterState();
		state.m_eKind = kind;
		state.m_SeatId = seatId;
		state.m_iSinceMs = System.GetTickCount();
		State().Set(npc.GetID(), state);

		if (!s_bWatching && GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
			s_bWatching = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_CharacterControllerComponent Controller(IEntity npc)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(npc);
		if (!character)
			return null;

		return SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
	}

	//------------------------------------------------------------------------------------------------
	//! npc may be null when the entity is already gone: the seat is then freed by id alone.
	protected static void ReleaseId(EntityID id, IEntity npc, string why)
	{
		LF_NpcLoiterState state = s_mState.Get(id);
		if (!state)
			return;

		s_mState.Remove(id);

		if (state.m_eKind == LF_ENpcLoiter.SEAT)
		{
			IEntity seatEntity = GetGame().GetWorld().FindEntityByID(state.m_SeatId);
			if (seatEntity && npc)
			{
				BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(seatEntity.FindComponent(BaseActionsManagerComponent));
				array<BaseUserAction> actions = {};
				if (manager)
					manager.GetActionsList(actions);

				foreach (BaseUserAction action : actions)
				{
					LF_SitAction sit = LF_SitAction.Cast(action);
					if (sit)
						sit.LF_StandBody(npc);
				}
			}
		}
		else if (npc)
		{
			SCR_CharacterControllerComponent controller = Controller(npc);
			if (controller)
				controller.StopLoitering(false);
		}

		LF_Debug.Info(FEATURE, string.Format("npc loiter: %1 stops (%2)", id.ToString(), why));
	}

	//------------------------------------------------------------------------------------------------
	//! Repeating: free the seat of an NPC that died or was deleted, or that stopped loitering by itself.
	static void Watch()
	{
		if (!s_mState || s_mState.IsEmpty())
			return;

		array<EntityID> ids = {};
		foreach (EntityID id, LF_NpcLoiterState state : s_mState)
		{
			ids.Insert(id);
		}

		int now = System.GetTickCount();
		foreach (EntityID id : ids)
		{
			IEntity npc = GetGame().GetWorld().FindEntityByID(id);
			if (!npc)
			{
				ReleaseId(id, null, "gone");
				continue;
			}

			SCR_CharacterControllerComponent controller = Controller(npc);
			if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			{
				ReleaseId(id, npc, "down");
				continue;
			}

			LF_NpcLoiterState current = s_mState.Get(id);
			if (!current || now - current.m_iSinceMs < GRACE_MS)
				continue;

			SCR_CharacterCommandHandlerComponent handler = SCR_CharacterCommandHandlerComponent.Cast(controller.GetAnimationComponent().GetCommandHandler());
			ELoiteringType type;
			if (!handler || !handler.IsLoitering(type))
				ReleaseId(id, npc, "stood up");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: how many NPCs hold a loiter now.
	static int GetCount()
	{
		if (!s_mState)
			return 0;

		return s_mState.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_NpcLoiter's statics (registered at script load; Core/LF_StaticReset.c).
class LF_NpcLoiter_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_NpcLoiter_StaticResetHook());

	override string Owner()
	{
		return "LF_NpcLoiter";
	}

	override void Apply()
	{
		LF_NpcLoiter.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! An NPC of a loiter role settles a few seconds after it spawns (the world has loaded by then) and
//! lets go when it is deleted.
modded class LF_NpcRoleComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer() || !LF_NpcLoiter.IsLoiterRole(m_sRole))
			return;

		// Spread the settle so a crowd does not all pick the same bench in one frame.
		int delay = 4000 + LF_Roles.SeedHash(owner.GetName() + owner.GetID().ToString()) % 4000;
		GetGame().GetCallqueue().CallLater(LF_Settle, delay, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_Settle()
	{
		IEntity owner = GetOwner();
		if (owner)
			LF_NpcLoiter.Settle(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(LF_Settle);

		LF_NpcLoiter.Release(owner, "deleted");
		super.OnDelete(owner);
	}
}
