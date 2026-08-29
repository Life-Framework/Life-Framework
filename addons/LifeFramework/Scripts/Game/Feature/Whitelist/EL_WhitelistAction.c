//------------------------------------------------------------------------------------------------
//! Base class for user actions gated by a whitelist flag. Subclasses configure
//! m_eWhitelistOnlyType; without a flag set the action performs for everyone.
class EL_WhitelistAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.Flags, "Action can only be used with this whitelist", "", ParamEnumArray.FromEnum(EL_WhitelistType))]
	protected EL_WhitelistType m_eWhitelistOnlyType;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (m_eWhitelistOnlyType == 0)
			return true;

		EL_PlayerWhitelistComponent whitelistComponent = EL_PlayerWhitelistComponent.Cast(user.FindComponent(EL_PlayerWhitelistComponent));
		if (!whitelistComponent || !whitelistComponent.HasWhitelist(m_eWhitelistOnlyType))
		{
			SetCannotPerformReason("#EL-Whitelist_NotAllowed");
			return false;
		}
		return true;
	}
}