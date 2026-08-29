class EL_NetworkUtils : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \param entity Entity to get the replication id from
	//! \return The RplId of the entity or RplId.Invalid() if the entity has no RplComponent
	static RplId GetRplId(IEntity entity)
	{
		if (entity)
		{
			RplComponent replication = RplComponent.Cast(entity.FindComponent(RplComponent));
			if (replication) return replication.Id();
		}

		return RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Finds an entity by its replication id
	//! \param rplId Replication id to search for
	//! \return the entity found or null if not found or invalid replication id
	static IEntity FindEntityByRplId(RplId rplId)
	{
		IEntity entity = null;

		if (rplId.IsValid())
		{
			RplComponent entityRpl = RplComponent.Cast(Replication.FindItem(rplId));
			if (entityRpl) entity = entityRpl.GetEntity();
		}

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! \param entity Entity to check ownership of
	//! \return true if the local machine is the owner of the given entity
	static bool IsOwner(IEntity entity)
	{
		if (!entity)
			return false;

		RplComponent replication = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!replication)
			return false;

		return replication.IsOwner();
	}
};