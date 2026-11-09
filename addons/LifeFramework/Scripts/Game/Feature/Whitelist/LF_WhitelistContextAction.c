//------------------------------------------------------------------------------------------------
//! Game Master context actions: grant or revoke a whitelist on the hovered character.
[BaseContainerProps()]
class LF_EnableWhitelistContextAction : SCR_BaseContextAction
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LF_WhitelistType))]
	LF_WhitelistType m_eType;

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
		LF_WhitelistRespawnHandlerComponent whitelistComponent = LF_WhitelistRespawnHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(LF_WhitelistRespawnHandlerComponent));
		if (whitelistComponent)
			whitelistComponent.EnableWhitelistOnPlayer(LF_Utils.GetPlayerUID(hoveredEntity.GetOwner()), m_eType);
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_DisableWhitelistContextAction : SCR_BaseContextAction
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LF_WhitelistType))]
	LF_WhitelistType m_eType;

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
		LF_WhitelistRespawnHandlerComponent whitelistComponent = LF_WhitelistRespawnHandlerComponent.Cast(GetGame().GetGameMode().FindComponent(LF_WhitelistRespawnHandlerComponent));
		if (whitelistComponent)
			whitelistComponent.DisableWhitelistOnPlayer(LF_Utils.GetPlayerUID(hoveredEntity.GetOwner()), m_eType);
	}
}