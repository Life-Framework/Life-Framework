class LF_FixtureLazy
{
	protected static ref LF_FixtureLazy s_Instance;

	static LF_FixtureLazy GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_FixtureLazy();
		return s_Instance;
	}

	void Touch() {}
}

class LF_FixtureLookups
{
	void Run(IEntity owner, Widget root)
	{
		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(owner);
		if (quantity)
			quantity.SetQuantity(1);
		SCR_InventoryStorageManagerComponent storage = SCR_InventoryStorageManagerComponent.Cast(owner.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!GetGame().GetGameMode() || !GetGame().GetGameMode().IsMaster())
			return;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			workspace.CreateWidgets("GetGame().GetWorkspace().CreateWidgets");
		LF_FixtureLazy.GetInstance().Touch();
		int index = m_aItems.Find(owner);
	}
}
