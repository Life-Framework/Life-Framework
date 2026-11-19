[BaseContainerProps()]
class LF_ResourceDestructionTool
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Tool to destroy the resource", "et")]
	ResourceName m_rTool;
	
	[Attribute("20", UIWidgets.Auto, "Amount of damage to the resource on hit (x/100)", "1 100 1")]
	int m_fHitDamage;
}

class LF_DestructibleResourceComponentClass : SCR_DestructionMultiPhaseComponentClass
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Particle to spawn on hit", "et", category: "Damage FX")]
	ResourceName m_rHitEffect;
	
	[Attribute("", UIWidgets.Auto, "Usable tools to destroy the resource", category: "Tools")]
	ref array<ref LF_ResourceDestructionTool> m_aTools;
}

class LF_DestructibleResourceComponent : SCR_DestructionMultiPhaseComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server (or a listen host, or single player): one accepted tool hit on this resource. Tells
	//! every client to show the hit effect, and shows it here when this peer renders. Damage only
	//! runs on the server, so on a dedicated server nobody saw chips without the broadcast (#3372).
	void OnToolHit(vector hitPos, vector hitNormal)
	{
		// A client never broadcasts; it gets the effect from the server's RPC.
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl && !rpl.IsMaster())
			return;

		Rpc(RpcDo_HitFx, hitPos, hitNormal);
		if (!System.IsConsoleApp())
			SpawnHitFx(hitPos, hitNormal);
	}

	//------------------------------------------------------------------------------------------------
	//! Broadcast reaches clients only; the server and a listen host call SpawnHitFx themselves.
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_HitFx(vector hitPos, vector hitNormal)
	{
		SpawnHitFx(hitPos, hitNormal);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the configured hit effect at the hit point. False when none is configured.
	bool SpawnHitFx(vector hitPos, vector hitNormal)
	{
		IEntity owner = GetOwner();
		LF_DestructibleResourceComponentClass settings = LF_DestructibleResourceComponentClass.Cast(GetComponentData(owner));
		if (!settings || !settings.m_rHitEffect)
		{
			LF_Debug.Warn("Resources", "hit effect spawn skipped: no configured hit effect");
			return false;
		}

		LF_Debug.Log("Resources", string.Format("hit fx spawned at %1", hitPos));
		LF_Utils.SpawnEntityPrefab(settings.m_rHitEffect, hitPos, hitNormal.VectorToAngles(), false);
		return true;
	}
}
