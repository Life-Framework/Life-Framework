[BaseContainerProps()]
class EL_DefaultLoadoutItemQuantityComponent : EL_DefaultLoadoutItemComponent
{
	[Attribute("1")]
	int m_iQuantity;

	override void ApplyTo(IEntity item)
	{
		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(item);
		if (!quantity)
		{
			// Loadout config lists a quantity for a prefab without EL_QuantityComponent: the item keeps its default.
			EL_Debug.Error("Quantity", string.Format("default loadout quantity skipped: %1 has no EL_QuantityComponent", item));
			return;
		}

		quantity.SetQuantity(m_iQuantity);
	}
}
