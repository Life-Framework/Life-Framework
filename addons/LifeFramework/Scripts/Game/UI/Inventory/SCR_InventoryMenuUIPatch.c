modded class SCR_InventoryMenuUI
{
	protected static SCR_InventoryMenuUI s_pELThisMenu;

	//------------------------------------------------------------------------------------------------
	static SCR_InventoryMenuUI LF_GetCurrentInstance()
	{
		return s_pELThisMenu;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		s_pELThisMenu = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		super.OnMenuClose();
		s_pELThisMenu = null;
	}
}
