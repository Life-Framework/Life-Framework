[ComponentEditorProps(category: "LifeFramework/Shop", description: "Furniture shop component")]
class EL_FurnitureShopComponentClass : ScriptComponentClass
{
}

class EL_FurnitureShopComponent : ScriptComponent
{
    [Attribute("", UIWidgets.EditBox, "Shop Name", "")]
    protected string m_ShopName;

    // Furniture items (initialized inline to avoid constructor overloads)
    protected ref array<ref EL_FurnitureItem> m_FurnitureItems = new array<ref EL_FurnitureItem>();

    // Initialize defaults in lifecycle hook
    override void EOnInit(IEntity owner)
    {
        super.EOnInit(owner);

        // Populate default furniture items if empty
        if (m_FurnitureItems.Count() == 0)
        {
            m_FurnitureItems.Insert(EL_FurnitureItem.CreateChair());
            m_FurnitureItems.Insert(EL_FurnitureItem.CreateTable());
            m_FurnitureItems.Insert(EL_FurnitureItem.CreateBed());
        }
    }

    // Add furniture item
    void AddFurnitureItem(EL_FurnitureItem item)
    {
        m_FurnitureItems.Insert(item);
    }

    // Get furniture items
    array<ref EL_FurnitureItem> GetFurnitureItems()
    {
        return m_FurnitureItems;
    }

    // Get shop name
    string GetShopName() { return m_ShopName; }

    // Buy furniture
    bool BuyFurniture(string itemName, string playerUID)
    {
        foreach (EL_FurnitureItem item : m_FurnitureItems)
        {
            if (item.GetName() == itemName)
            {
                // TODO: Check player money and deduct
                Print("Furniture " + itemName + " purchased by " + playerUID);
                return true;
            }
        }
        return false;
    }
}