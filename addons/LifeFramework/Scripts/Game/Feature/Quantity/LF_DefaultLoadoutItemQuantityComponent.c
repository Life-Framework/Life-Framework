[BaseContainerProps()]
class LF_DefaultLoadoutItemQuantityComponent : LF_DefaultLoadoutItemComponent
{
	[Attribute("1")]
	int m_iQuantity;

	override void ApplyTo(IEntity item)
	{
		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
		if (!quantity)
		{
			// Loadout config lists a quantity for a prefab without LF_QuantityComponent: the item keeps its default.
			LF_Debug.Error("Quantity", string.Format("default loadout quantity skipped: %1 has no LF_QuantityComponent", item));
			return;
		}

		quantity.SetQuantity(m_iQuantity);
	}
}
