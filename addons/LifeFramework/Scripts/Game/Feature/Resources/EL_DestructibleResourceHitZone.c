class EL_DestructibleResourceHitZone : SCR_HitZone
{
	//------------------------------------------------------------------------------------------------
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		if (System.IsConsoleApp()) return;

		IEntity owner = GetOwner();
		EL_DestructibleResourceComponent destructibleResource = EL_Component<EL_DestructibleResourceComponent>.Find(owner);
		if (!destructibleResource)
			return;

		EL_DestructibleResourceComponentClass settings = EL_DestructibleResourceComponentClass.Cast(destructibleResource.GetComponentData(owner));
		if (!settings || !settings.m_rHitEffect)
		{
			EL_Debug.Warn("Resources", "hit effect spawn skipped: no configured hit effect");
			return;
		}

		EL_Debug.Log("Resources", string.Format("hit fx spawned at %1", damageContext.hitPosition));
		EL_Utils.SpawnEntityPrefab(settings.m_rHitEffect, damageContext.hitPosition, damageContext.hitNormal.VectorToAngles(), false);
	}

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		EL_DestructibleResourceComponent destructibleResource = EL_Component<EL_DestructibleResourceComponent>.Find(GetOwner());
		if (!destructibleResource)
			return 0.0;

		EL_DestructibleResourceComponentClass settings = EL_DestructibleResourceComponentClass.Cast(destructibleResource.GetComponentData(destructibleResource.GetOwner()));
		if (!settings || !settings.m_aTools)
			return 0.0;

		ResourceName currentTool = EL_Utils.GetPrefabName(damageContext.damageSource);
		foreach (EL_ResourceDestructionTool tool : settings.m_aTools)
		{
			if (tool && tool.m_rTool == currentTool)
				return tool.m_fHitDamage;
		}

		return 0.0;
	}
}
