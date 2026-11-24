//! Hit zone of a resource you work with a tool: a tree (axe) or an ore rock (pickaxe).
//! Only the tools listed on LF_DestructibleResourceComponent deal damage, compared by
//! prefab GUID; a prefab that inherits a listed tool (a tier of it) counts too. An ore rock (LF_OreNodeComponent) takes every accepted hit itself on
//! the server and the zone heals back, so the rock is never destroyed by health.
class LF_DestructibleResourceHitZone : SCR_HitZone
{
	//! The system a server switches tool damage on trees and rocks off with (#1075).
	static const string SYSTEM = "Resources";

	//------------------------------------------------------------------------------------------------
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		if (!LF_Systems.IsEnabled(SYSTEM))
			return;

		IEntity owner = GetOwner();
		LF_OreNodeComponent ore = LF_OreNodeComponent.Cast(owner.FindComponent(LF_OreNodeComponent));
		if (ore)
		{
			float dealt = ComputeEffectiveDamage(damageContext, false);
			IEntity miner;
			if (damageContext.instigator)
				miner = damageContext.instigator.GetInstigatorEntity();
			ore.OnToolHit(dealt, damageContext.hitPosition, damageContext.hitDirection, damageContext.damageSource, miner);
			SetHealthScaled(1);
			return;
		}

		LF_DestructibleResourceComponent destructibleResource = LF_Component<LF_DestructibleResourceComponent>.Find(owner);
		if (!destructibleResource)
			return;

		// Damage runs on the server: it broadcasts the effect to clients and plays it itself
		// when it renders (a listen host), so a dedicated server's players see chips (#3372).
		destructibleResource.OnToolHit(damageContext.hitPosition, damageContext.hitNormal);
	}

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		// Resources off: a tool deals nothing to a tree or a rock, so neither falls nor breaks (#1075).
		if (!LF_Systems.IsEnabled(SYSTEM))
			return 0.0;

		IEntity owner = GetOwner();
		LF_DestructibleResourceComponent destructibleResource = LF_Component<LF_DestructibleResourceComponent>.Find(owner);
		if (!destructibleResource)
			return 0.0;

		LF_OreNodeComponent ore = LF_OreNodeComponent.Cast(owner.FindComponent(LF_OreNodeComponent));
		if (ore && ore.IsDepleted())
			return 0.0;

		LF_DestructibleResourceComponentClass settings = LF_DestructibleResourceComponentClass.Cast(destructibleResource.GetComponentData(owner));
		if (!settings || !settings.m_aTools)
			return 0.0;

		float damage = HitDamageOf(settings, damageContext.damageSource);
		if (damage > 0)
			return damage;

		ResourceName currentTool = LF_Utils.GetPrefabName(damageContext.damageSource);
		if (damageContext.damageType == EDamageType.MELEE && !currentTool.IsEmpty())
			LF_Debug.Info("Resources", string.Format("%1 takes no damage from %2: not one of its tools", LF_Utils.GetPrefabName(owner), currentTool));

		return 0.0;
	}

	//------------------------------------------------------------------------------------------------
	//! The damage one hit with `tool` deals on a node with these settings; 0 when the tool is not one of
	//! its tools. A listed tool is the base of a family: a tier prefab that inherits it (worn, standard and
	//! fine pickaxe, #4188) counts as that tool, so nothing lists every tier (#4790).
	static float HitDamageOf(LF_DestructibleResourceComponentClass settings, IEntity tool)
	{
		if (!settings || !settings.m_aTools || !tool)
			return 0.0;

		foreach (LF_ResourceDestructionTool row : settings.m_aTools)
		{
			if (row && LF_Utils.EntityInherits(tool, row.m_rTool))
				return row.m_fHitDamage;
		}

		return 0.0;
	}
}
