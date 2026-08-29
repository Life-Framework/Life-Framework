//------------------------------------------------------------------------------------------------
//! Game Master context actions: grant or revoke a whitelist on the hovered character.
[BaseContainerProps()]
class EL_EnableWhitelistContextAction : SCR_BaseContextAction
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EL_WhitelistType))]
	EL_WhitelistType m_eType;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return hoveredEntity && hoveredEntity.GetEntityType() == EEditableEntityType.CHARACTER;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return hoveredEntity != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		EL_WhitelistRespawnHandlerComponent whitelistComponent = EL_WhitelistRespawnHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(EL_WhitelistRespawnHandlerComponent));
		if (whitelistComponent)
			whitelistComponent.EnableWhitelistOnPlayer(EL_Utils.GetPlayerUID(hoveredEntity.GetOwner()), m_eType);
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class EL_DisableWhitelistContextAction : SCR_BaseContextAction
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EL_WhitelistType))]
	EL_WhitelistType m_eType;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShown(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return hoveredEntity && hoveredEntity.GetEntityType() == EEditableEntityType.CHARACTER;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags)
	{
		return hoveredEntity != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(SCR_EditableEntityComponent hoveredEntity, notnull set<SCR_EditableEntityComponent> selectedEntities, vector cursorWorldPosition, int flags, int param = -1)
	{
		EL_WhitelistRespawnHandlerComponent whitelistComponent = EL_WhitelistRespawnHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(EL_WhitelistRespawnHandlerComponent));
		if (whitelistComponent)
			whitelistComponent.DisableWhitelistOnPlayer(EL_Utils.GetPlayerUID(hoveredEntity.GetOwner()), m_eType);
	}
}