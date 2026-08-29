class EL_ShopMenu : ChimeraMenuBase
{
	protected ref EL_ShopComponent m_ShopComponent;
	protected ref PlayerController m_PlayerController;
	protected ref array<ref EL_ShopMenuItem> m_aMenuItems;

	//------------------------------------------------------------------------------------------------
	void SetShopComponent(EL_ShopComponent shopComponent)
	{
		m_ShopComponent = shopComponent;
		RefreshShopItems();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity m_PlayerEntity;

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
		if (m_PlayerController)
			m_PlayerEntity = m_PlayerController.GetControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	void SetPlayerEntity(IEntity playerEntity)
	{
		m_PlayerEntity = playerEntity;
		if (m_PlayerEntity)
		{
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_PlayerEntity);
			if (playerId > 0)
				m_PlayerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshShopItems()
	{
		m_aMenuItems = {};
		if (m_ShopComponent)
		{
			array<ref EL_ShopItem> items = m_ShopComponent.GetShopItems();
			foreach (EL_ShopItem item : items)
			{
				EL_ShopMenuItem menuItem = new EL_ShopMenuItem();
				menuItem.m_Item = item;
				menuItem.m_sDisplayName = item.GetDisplayName();
				menuItem.m_iPrice = item.GetPrice();
				menuItem.m_iMaxQuantity = item.GetMaxQuantity();
				m_aMenuItems.Insert(menuItem);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void BuyItem(EL_ShopItem item, int quantity)
	{
		if (m_ShopComponent && m_PlayerController)
		{
			IEntity buyer = m_PlayerController.GetControlledEntity();
			if (buyer)
			{
				m_ShopComponent.BuyItem(item, quantity, buyer);
				RefreshShopItems(); // Update UI if needed
			}
		}
	}
}

class EL_ShopMenuItem
{
	EL_ShopItem m_Item;
	string m_sDisplayName;
	int m_iPrice;
	int m_iMaxQuantity;
};