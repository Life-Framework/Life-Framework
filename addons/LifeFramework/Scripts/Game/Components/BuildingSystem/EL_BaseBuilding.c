[EntityEditorProps(category: "EveronLife/Building", description: "Base class for placeable building entities")]
class EL_BaseBuildingClass : GenericEntityClass
{
}

enum EBuildingType
{
	CROP
}

class EL_BaseBuilding : GenericEntity
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EBuildingType), desc: "Placeable Type", category: "Building")]
	EBuildingType m_eBuildingType;
}