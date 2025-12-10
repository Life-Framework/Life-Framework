class EL_PropertyComponent : EPF_PersistentScriptedState
{
    [Attribute("0", UIWidgets.EditBox, "Property ID", "")]
    protected int m_PropertyID;

    [Attribute("10000", UIWidgets.EditBox, "Purchase Price", "")]
    protected int m_PurchasePrice;

    [Attribute("", UIWidgets.EditBox, "Owner UID", "")]
    protected string m_OwnerUID;

    [Attribute("false", UIWidgets.CheckBox, "Is Locked", "")]
    protected bool m_IsLocked;

    [Attribute("", UIWidgets.EditBox, "Property Name", "")]
    protected string m_PropertyName;

    // Furniture list (stored as array of prefab paths)
    protected ref array<string> m_FurnitureItems;

    // Constructor
    void EL_PropertyComponent()
    {
        m_FurnitureItems = new array<string>();
        SetPersistentId("Property_" + m_PropertyID.ToString());
    }

    // Getters
    int GetPropertyID() { return m_PropertyID; }
    int GetPurchasePrice() { return m_PurchasePrice; }
    string GetOwnerUID() { return m_OwnerUID; }
    bool IsLocked() { return m_IsLocked; }
    string GetPropertyName() { return m_PropertyName; }
    array<string> GetFurnitureItems() { return m_FurnitureItems; }

    // Setters
    void SetOwnerUID(string uid) { m_OwnerUID = uid; }
    void SetLocked(bool locked) { m_IsLocked = locked; }
    void AddFurniture(string prefab) { m_FurnitureItems.Insert(prefab); }
    void RemoveFurniture(string prefab) { m_FurnitureItems.RemoveItem(prefab); }

    // Check if player owns this property
    bool IsOwner(string playerUID)
    {
        return m_OwnerUID == playerUID;
    }

    // Purchase property
    bool Purchase(string playerUID)
    {
        if (m_OwnerUID != "")
            return false; // Already owned

        m_OwnerUID = playerUID;
        return true;
    }

    // Toggle lock
    void ToggleLock()
    {
        m_IsLocked = !m_IsLocked;
    }
}