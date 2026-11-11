//------------------------------------------------------------------------------------------------
//! Which town NPCs can be shot (#610). A town NPC is a scripted entity with no character damage
//! manager, so a bullet found nothing to hurt and every NPC was invulnerable. Only an NPC prefab
//! that carries an LF_NpcHitZone takes damage:
//!   - Npc_Civilian_01.et (ambient civilians) has one: it can be shot dead.
//!   - Npc_Default_01.et and its children (shop clerks, the fence, the police sergeant, quest
//!     givers) have none: they are service points a player needs, so they stay protected.
//! Killing a civilian is murder (LF_Murder) and a crime for witnesses to see (LF_Witness).
class LF_NpcDamage
{
	static const string FEATURE = "NpcDamage";
	static const int REMOVE_DELAY_MS = 1500;

	protected static ref array<EntityID> s_aDying;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aDying = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The damage rule: an NPC takes damage exactly when its prefab gave it a hit zone.
	static bool IsKillable(IEntity npc)
	{
		if (!npc)
			return false;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(npc.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
			return false;

		return LF_NpcHitZone.Cast(damage.GetDefaultHitZone()) != null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDying(IEntity npc)
	{
		return npc && s_aDying && s_aDying.Contains(npc.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a killable NPC's hit zone ran out of health. Records the murder for a player killer,
	//! then removes the body (a town NPC cannot ragdoll). Once per NPC.
	static bool OnKilled(IEntity npc, Instigator instigator)
	{
		if (!npc || !Replication.IsServer())
			return false;

		if (!s_aDying)
			s_aDying = {};

		if (s_aDying.Contains(npc.GetID()))
			return false;

		s_aDying.Insert(npc.GetID());

		IEntity killer;
		int killerId;
		if (instigator)
		{
			killer = instigator.GetInstigatorEntity();
			killerId = instigator.GetInstigatorPlayerID();
		}

		LF_Debug.Info(FEATURE, string.Format("civilian %1 killed by player %2", LF_WitnessDialogue.NameOf(npc), killerId));
		LF_Murder.OnCharacterKilled(npc, killer, 0, killerId);
		GetGame().GetCallqueue().CallLater(Remove, REMOVE_DELAY_MS, false, npc);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove(IEntity npc)
	{
		if (!npc)
			return;

		if (s_aDying)
			s_aDying.RemoveItem(npc.GetID());

		SCR_EntityHelper.DeleteEntityAndChildren(npc);
	}
}

//------------------------------------------------------------------------------------------------
//! The hit zone that makes a town NPC killable. When its health reaches zero the NPC is dead.
class LF_NpcHitZone : SCR_HitZone
{
	//------------------------------------------------------------------------------------------------
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		if (GetHealthScaled() > 0.001 && GetDamageState() != EDamageState.DESTROYED)
			return;

		LF_NpcDamage.OnKilled(GetOwner(), damageContext.instigator);
	}
}
