class LF_PersistenceComponentSerializer
{
	protected static void RegisterPersistencePayloads(notnull array<string> order)
	{
		order.Insert("reputation");
		order.Insert("itemkeys");
		// order.Insert("commented");
	}
}
