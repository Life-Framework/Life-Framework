[BaseContainerProps()]
class LF_DefaultLoadoutItem
{
	[Attribute()]
	ResourceName m_rPrefab;

	[Attribute(defvalue: "1")]
	int m_iAmount;

	[Attribute(defvalue: EStoragePurpose.PURPOSE_DEPOSIT.ToString(), uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EStoragePurpose))]
	EStoragePurpose m_ePurpose;

	[Attribute()]
	ref array<ref LF_DefaultLoadoutItemComponent> m_aComponentDefaults;

	[Attribute()]
	ref array<ref LF_DefaultLoadoutItem> m_aStoredItems;
}

[BaseContainerProps()]
class LF_DefaultLoadoutItemComponent
{
	void ApplyTo(IEntity item);
}
