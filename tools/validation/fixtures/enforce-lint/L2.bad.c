class LF_FixtureNullable
{
	protected static LF_FixtureNullable s_Instance;

	static LF_FixtureNullable GetInstance()
	{
		return s_Instance;
	}

	void Touch() {}
}

class LF_FixtureLookups
{
	void Run(IEntity owner, Widget root)
	{
		EL_Component<EL_QuantityComponent>.Find(owner).SetQuantity(1);
		SCR_InventoryStorageManagerComponent.Cast(owner.FindComponent(SCR_InventoryStorageManagerComponent)).GetOwner();
		root.FindAnyWidget("Row").SetVisible(true);
		GetGame().GetWorkspace().CreateWidgets("x");
		GetGame().GetGameMode().FindComponent(SCR_BaseGameModeComponent);
		GetGame().GetCameraManager().CurrentCamera();
		LF_FixtureNullable.GetInstance().Touch();
	}
}
