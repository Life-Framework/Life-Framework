//------------------------------------------------------------------------------------------------
//! Inspect on an identity card in the inventory (#902): the inventory closes and the card sheet
//! opens with what the server reads on that card. Every other item inspects as vanilla does.
modded class SCR_InventoryMenuUI
{
	//------------------------------------------------------------------------------------------------
	override protected bool CanInspectItemInInventory(notnull IEntity item, notnull InventoryItemComponent iic)
	{
		if (item.FindComponent(LF_IdCardComponent))
			return true;

		return super.CanInspectItemInInventory(item, iic);
	}

	//------------------------------------------------------------------------------------------------
	override void Action_Inspect()
	{
		IEntity card = LF_FocusedIdCard();
		if (!card)
		{
			super.Action_Inspect();
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "inspect not sent: no player controller");
			return;
		}

		controller.LF_InspectIdCard(LF_NetworkUtils.GetRplId(card));
		LF_Debug.Info(LF_PhotoIdCard.FEATURE, "inspect: asked the server for the card");
		Action_CloseInventory();
	}

	//------------------------------------------------------------------------------------------------
	//! The identity card under the cursor, or null.
	protected IEntity LF_FocusedIdCard()
	{
		if (!m_pFocusedSlotUI)
			return null;

		InventoryItemComponent item = m_pFocusedSlotUI.GetInventoryItemComponent();
		if (!item || !item.GetOwner() || !item.GetOwner().FindComponent(LF_IdCardComponent))
			return null;

		return item.GetOwner();
	}
}
