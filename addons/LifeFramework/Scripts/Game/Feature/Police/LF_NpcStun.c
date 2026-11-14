//------------------------------------------------------------------------------------------------
//! The taser on a town NPC (#922). Town NPCs are LF_NpcBaseEntity, not characters: no damage
//! manager, so a dart that hits one reports to nobody (LF_Stun lives in the character damage
//! manager's hit handler and never sees it). The taser's shot is therefore read from the
//! shooter instead: every player body already has an OnProjectileShot handler on the server
//! (LF_Evidence.WatchShooter), which calls OnShot. When the shooter holds a stun weapon (a
//! prefab with LF_StunWeaponComponent) the server follows the shot's line for the weapon's reach,
//! and the NPC nearest to it, in sight, falls down for the weapon's stun time and gets back up.
//!
//! Server-driven. The state is the replicated flag on LF_NpcBaseEntity: each peer lays the body
//! down from it, so clients and late joiners see the same. A stunned NPC cannot be talked to
//! (BlocksTalk, used by LF_Dialogue and the NPC's actions). Players keep LF_Stun.
//! Guns do not hurt NPCs here; that is LF_NpcDamage's.
//------------------------------------------------------------------------------------------------
class LF_NpcStun
{
	static const string FEATURE = "Police";

	//! Degrees the body tips over backwards (negative pitch tips the head back and down).
	static const float FALL_PITCH = -88; // lf-fixed: a pose, not a balance value
	//! Metres the lying body sits above its feet's ground so its back does not sink in.
	static const float LIE_LIFT = 0.12; // lf-fixed: a pose, not a balance value
	//! How far from the line of fire, in metres, the centre of the chest may be and still be hit.
	static const float HIT_RADIUS = 0.6; // lf-fixed: roughly half a torso's width
	//! Height of the chest above the NPC's feet.
	static const float CHEST_HEIGHT = 1.2; // lf-fixed: the harness shoots at the same point

	protected static ref map<EntityID, int> s_mTokens;
	protected static int s_iStunCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mTokens = null;
		s_iStunCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<EntityID, int> Tokens()
	{
		if (!s_mTokens)
			s_mTokens = new map<EntityID, int>();

		return s_mTokens;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStunCount()
	{
		return s_iStunCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: which of `centres` (chest points) a shot from `from` along the unit `dir` hits,
	//! or -1. The nearest to the shooter among those within `radius` of the line and `range` of reach.
	static int PickTarget(vector from, vector dir, float range, float radius, array<vector> centres)
	{
		int best = -1;
		float bestAlong = range + 1;
		for (int i = 0; i < centres.Count(); i++)
		{
			vector rel = centres[i] - from;
			float along = vector.Dot(rel, dir);
			if (along < 0 || along > range)
				continue;

			float off = (rel - dir * along).Length();
			if (off > radius || along >= bestAlong)
				continue;

			best = i;
			bestAlong = along;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! True while an NPC cannot be talked to or served because it lies stunned.
	static bool BlocksTalk(IEntity npc)
	{
		LF_NpcBaseEntity entity = LF_NpcBaseEntity.Cast(npc);
		return entity && entity.LF_IsStunned();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stun an NPC for `seconds`. Refused (and logged) for something that is not a town NPC,
	//! one that is already down or dying, or no time.
	static bool Apply(IEntity npc, float seconds, string byUid = "")
	{
		if (!Replication.IsServer())
			return false;

		LF_NpcBaseEntity entity = LF_NpcBaseEntity.Cast(npc);
		if (!entity || seconds <= 0 || entity.LF_IsStunned() || LF_NpcDamage.IsDying(entity))
		{
			LF_Debug.Info(FEATURE, string.Format("npc stun refused (town npc %1, seconds %2)", entity != null, seconds));
			return false;
		}

		int token = Tokens().Get(entity.GetID()) + 1;
		Tokens().Set(entity.GetID(), token);
		entity.LF_SetStunned(true);
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Wake, seconds * 1000, false, entity, token);

		s_iStunCount++;
		LF_Debug.Info(FEATURE, string.Format("npc %1 is stunned for %2 s (by %3)", entity.GetID(), seconds, byUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the stun's time is over. A later stun's token makes an earlier wake do nothing.
	static void Wake(LF_NpcBaseEntity npc, int token)
	{
		if (!npc || !npc.LF_IsStunned() || Tokens().Get(npc.GetID()) != token)
			return;

		End(npc);
	}

	//------------------------------------------------------------------------------------------------
	static bool End(notnull LF_NpcBaseEntity npc)
	{
		if (!Replication.IsServer() || !npc.LF_IsStunned())
			return false;

		npc.LF_SetStunned(false);
		LF_Debug.Info(FEATURE, string.Format("npc %1 is up again", npc.GetID()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player body fired a shot (called from the body's OnProjectileShot handler).
	static void OnShot(IEntity shooter, IEntity projectile)
	{
		if (!Replication.IsServer() || !shooter)
			return;

		LF_StunWeaponComponent weapon = LF_Stun.HeldStunWeapon(shooter);
		if (!weapon)
			return;

		vector from = shooter.GetOrigin() + Vector(0, 1.5, 0);
		vector dir = vector.Zero;
		ChimeraCharacter chimera = ChimeraCharacter.Cast(shooter);
		if (chimera)
		{
			CharacterControllerComponent controller = chimera.GetCharacterController();
			if (controller && controller.GetAimingComponent())
				dir = controller.GetAimingComponent().GetAimingDirectionWorld();
		}

		if (projectile)
		{
			from = projectile.GetOrigin();
			if (dir.LengthSq() < 0.5)
				dir = projectile.GetTransformAxis(2);
		}

		bool hit = HitAlong(shooter, weapon, from, dir);
		LF_Debug.Info(FEATURE, string.Format("stun weapon fired from %1 along %2: npc stunned %3", from, dir, hit));
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: the same hit as OnShot, fired from the shooter's chest at `npc`. \return True when
	//! the NPC was stunned. The shooter must hold a stun weapon.
	static bool HarnessShot(IEntity shooter, IEntity npc)
	{
		if (!shooter || !npc)
			return false;

		LF_StunWeaponComponent weapon = LF_Stun.HeldStunWeapon(shooter);
		if (!weapon)
			return false;

		vector from = shooter.GetOrigin() + Vector(0, CHEST_HEIGHT, 0);
		vector dir = vector.Direction(from, npc.GetOrigin() + Vector(0, CHEST_HEIGHT, 0)).Normalized();
		return HitAlong(shooter, weapon, from, dir);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HitAlong(notnull IEntity shooter, notnull LF_StunWeaponComponent weapon, vector from, vector dir)
	{
		if (dir.LengthSq() < 0.5)
			return false;

		dir = dir.Normalized();
		float range = weapon.GetRange();
		array<IEntity> npcs = {};
		array<vector> centres = {};
		LF_NpcStunScan scan = new LF_NpcStunScan(npcs, centres);
		GetGame().GetWorld().QueryEntitiesBySphere(from + dir * (range * 0.5), range * 0.5 + 1, scan.Add, null, EQueryEntitiesFlags.ALL);

		int pick = PickTarget(from, dir, range, HIT_RADIUS, centres);
		if (pick < 0)
			return false;

		IEntity npc = npcs[pick];
		if (Blocked(from, centres[pick], shooter, npc))
		{
			LF_Debug.Info(FEATURE, "stun dart stopped by something solid before the npc");
			return false;
		}

		string name = LF_WitnessDialogue.NameOf(npc);
		if (!Apply(npc, weapon.GetStunSeconds(), LF_Utils.GetPlayerUID(shooter)))
			return false;

		LF_Utils.NotifyEntity(shooter, "#LF-Police_NpcStunned", "", 4.0, name);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Something solid on the line from the muzzle to the chest that is neither the shooter nor the NPC.
	protected static bool Blocked(vector from, vector to, IEntity shooter, IEntity npc)
	{
		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = to;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = shooter;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		return fraction < 1 && trace.TraceEnt && trace.TraceEnt != npc && trace.TraceEnt.GetParent() != npc;
	}
}

//------------------------------------------------------------------------------------------------
//! Collects the town NPCs (not already stunned) of a sphere query with their chest points.
class LF_NpcStunScan : Managed
{
	protected array<IEntity> m_aNpcs;
	protected array<vector> m_aCentres;

	//------------------------------------------------------------------------------------------------
	void LF_NpcStunScan(array<IEntity> npcs, array<vector> centres)
	{
		m_aNpcs = npcs;
		m_aCentres = centres;
	}

	//------------------------------------------------------------------------------------------------
	bool Add(IEntity entity)
	{
		LF_NpcBaseEntity npc = LF_NpcBaseEntity.Cast(entity);
		if (!npc || npc.LF_IsStunned())
			return true;

		m_aNpcs.Insert(npc);
		m_aCentres.Insert(npc.GetOrigin() + Vector(0, LF_NpcStun.CHEST_HEIGHT, 0));
		return true;
	}
}
