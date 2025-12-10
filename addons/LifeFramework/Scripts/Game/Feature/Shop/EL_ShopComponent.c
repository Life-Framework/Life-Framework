[ComponentEditorProps(category: "EveronLife/Shop", description: "Component for shop entities that allow buying items")]
class EL_ShopComponentClass : ScriptComponentClass
{
}

class EL_ShopComponent : ScriptComponent
{
	[Attribute("Shop", UIWidgets.Auto, desc: "Name of the shop")]
	protected string m_sShopName;

	[Attribute("", UIWidgets.Auto, desc: "Description of the shop")]
	protected string m_sShopDescription;

	[Attribute("", UIWidgets.Object, desc: "List of items available in this shop")]
	protected ref array<ref EL_ShopItem> m_aShopItems;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_aShopItems)
			m_aShopItems = new array<ref EL_ShopItem>();
	}

	//------------------------------------------------------------------------------------------------
	string GetShopName()
	{
		return m_sShopName;
	}

	//------------------------------------------------------------------------------------------------
	string GetShopDescription()
	{
		return m_sShopDescription;
	}

	//------------------------------------------------------------------------------------------------
	array<ref EL_ShopItem> GetShopItems()
	{
		return m_aShopItems;
	}

	//------------------------------------------------------------------------------------------------
	bool BuyItem(EL_ShopItem item, int quantity, IEntity buyer)
	{
		if (!item || quantity <= 0 || quantity > item.GetMaxQuantity())
			return false;

		int totalPrice = item.GetPrice() * quantity;

		// Check if buyer has enough money
		if (!EL_MoneyUtils.RemoveAmount(buyer, totalPrice))
		{
			EL_Utils.Notify("#EL-Not_Enough_Money", "Purchase Failed", 3.0);
			return false;
		}

		// Add item to buyer's inventory
		ResourceName prefab = item.GetItemPrefab();
		if (EL_InventoryUtils.AddAmount(buyer, prefab, quantity))
		{
			EL_Utils.Notify(string.Format("#EL-Item_Bought", item.GetDisplayName(), quantity), "Purchase Successful", 3.0);
			return true;
		}
		else
		{
			// Refund money if inventory full
			EL_MoneyUtils.AddAmount(buyer, totalPrice);
			EL_Utils.Notify("#EL-Inventory_Full", "Purchase Failed", 3.0);
			return false;
		}
	}

	//------------------------------------------------------------------------------------------------
	void OpenShopMenu(IEntity playerEntity)
	{
		if (!playerEntity)
			return;
			
		PlayerController playerController = GetGame().GetPlayerController();
		if (playerController)
		{
			EL_ShopMenu menu = EL_ShopMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.Shop));
			if (menu)
			{
				menu.SetShopComponent(this);
				menu.SetPlayerEntity(playerEntity);
			}
		}
	}
};