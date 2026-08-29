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
	//! Whether a requested quantity is within the item's buy limits.
	static bool IsQuantityAllowed(int maxQuantity, int quantity)
	{
		if (quantity <= 0)
			return false;
		return quantity <= maxQuantity;
	}

	//------------------------------------------------------------------------------------------------
	//! Total price of a purchase. Zero for invalid requests.
	static int ComputeTotalPrice(int unitPrice, int quantity)
	{
		if (unitPrice < 0 || quantity <= 0)
			return 0;
		return unitPrice * quantity;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the shop actually sells this item. Only the shop's own catalog
	//! may be bought; an arbitrary item object is a forged offer.
	bool IsSoldByThisShop(EL_ShopItem item)
	{
		if (!m_aShopItems)
			return false;
		return m_aShopItems.Find(item) != -1;
	}

	//------------------------------------------------------------------------------------------------
	bool BuyItem(EL_ShopItem item, int quantity, IEntity buyer)
	{
		if (!Replication.IsServer())
			return false;
		if (!item || !IsQuantityAllowed(item.GetMaxQuantity(), quantity))
			return false;
		if (!IsSoldByThisShop(item))
		{
			EL_Debug.Log("Shop", string.Format("buy rejected (not sold here) item=%1 qty=%2", item.GetItemPrefab(), quantity));
			return false;
		}

		int totalPrice = ComputeTotalPrice(item.GetPrice(), quantity);

		// RemoveAmount reports what was actually removed; a partial removal is
		// not success and the removed part must be returned to the buyer.
		int removed = EL_MoneyUtils.RemoveAmount(buyer, totalPrice);
		if (removed != totalPrice)
		{
			if (removed > 0)
				EL_MoneyUtils.AddAmount(buyer, removed);
			EL_Debug.Log("Shop", string.Format("buy failed (insufficient funds) item=%1 qty=%2 price=%3 removed=%4", item.GetItemPrefab(), quantity, totalPrice, removed));
			EL_Utils.Notify("#EL-Not_Enough_Money", "#EL-Shop_PurchaseFailed", 3.0);
			return false;
		}

		// Add item to buyer's inventory
		ResourceName prefab = item.GetItemPrefab();
		if (EL_InventoryUtils.AddAmount(buyer, prefab, quantity))
		{
			EL_Debug.Log("Shop", string.Format("bought item=%1 qty=%2 price=%3", prefab, quantity, totalPrice));
			EL_Utils.Notify(string.Format("#EL-Item_Bought", item.GetDisplayName(), quantity), "#EL-Shop_PurchaseSuccessful", 3.0);
			return true;
		}
		else
		{
			// Refund the full price: the full price was removed
			EL_MoneyUtils.AddAmount(buyer, totalPrice);
			EL_Debug.Log("Shop", string.Format("buy failed (inventory full) item=%1 - refunded %2", prefab, totalPrice));
			EL_Utils.Notify("#EL-Inventory_Full", "#EL-Shop_PurchaseFailed", 3.0);
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