[EntityEditorProps(category: "EveronLife/Building", description: "Base class for placeable building entities")]
class LF_BaseBuildingClass : GenericEntityClass
{
}

enum EBuildingType
{
	CROP
}

class LF_BaseBuilding : GenericEntity
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EBuildingType), desc: "Placeable Type", category: "Building")]
	EBuildingType m_eBuildingType;
}