//! Hit zone of a resource you work with a tool: a tree (axe) or an ore rock (pickaxe).
//! Only the tools listed on LF_DestructibleResourceComponent deal damage, compared by
//! prefab GUID. An ore rock (LF_OreNodeComponent) takes every accepted hit itself on
//! the server and the zone heals back, so the rock is never destroyed by health.
class LF_DestructibleResourceHitZone : SCR_HitZone
{
	//------------------------------------------------------------------------------------------------
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		IEntity owner = GetOwner();
		LF_OreNodeComponent ore = LF_OreNodeComponent.Cast(owner.FindComponent(LF_OreNodeComponent));
		if (ore)
		{
			float dealt = ComputeEffectiveDamage(damageContext, false);
			ore.OnToolHit(dealt, damageContext.hitPosition, damageContext.hitDirection, damageContext.damageSource);
			SetHealthScaled(1);
			return;
		}

		if (System.IsConsoleApp()) return;

		LF_DestructibleResourceComponent destructibleResource = LF_Component<LF_DestructibleResourceComponent>.Find(owner);
		if (!destructibleResource)
			return;

		LF_DestructibleResourceComponentClass settings = LF_DestructibleResourceComponentClass.Cast(destructibleResource.GetComponentData(owner));
		if (!settings || !settings.m_rHitEffect)
		{
			LF_Debug.Warn("Resources", "hit effect spawn skipped: no configured hit effect");
			return;
		}

		LF_Debug.Log("Resources", string.Format("hit fx spawned at %1", damageContext.hitPosition));
		LF_Utils.SpawnEntityPrefab(settings.m_rHitEffect, damageContext.hitPosition, damageContext.hitNormal.VectorToAngles(), false);
	}

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
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

		ResourceName currentTool = LF_Utils.GetPrefabName(damageContext.damageSource);
		foreach (LF_ResourceDestructionTool tool : settings.m_aTools)
		{
			if (tool && LF_Utils.SamePrefab(tool.m_rTool, currentTool))
				return tool.m_fHitDamage;
		}

		if (damageContext.damageType == EDamageType.MELEE && !currentTool.IsEmpty())
			LF_Debug.Info("Resources", string.Format("%1 takes no damage from %2: not one of its tools", LF_Utils.GetPrefabName(owner), currentTool));

		return 0.0;
	}
}
