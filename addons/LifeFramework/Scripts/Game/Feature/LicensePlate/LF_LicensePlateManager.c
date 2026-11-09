class LF_LicensePlateManagerClass: GenericEntityClass
{
}

class LF_LicensePlateManager: GenericEntity
{
	[Attribute(uiwidget: UIWidgets.Auto)]
	ref LF_LicensePlateGeneratorBase m_GeneratorBase;
	
	static LF_LicensePlateManager s_Instance;
	
	void LF_LicensePlateManager(IEntitySource src, IEntity parent)
	{
		s_Instance = this;
	}
}
