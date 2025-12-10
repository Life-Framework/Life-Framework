class EL_FurnitureItem
{
    protected string m_Name;
    protected int m_Price;
    protected string m_PrefabPath;
    protected string m_Description;

    // Constructor
    void EL_FurnitureItem(string name, int price, string prefabPath, string description = "")
    {
        m_Name = name;
        m_Price = price;
        m_PrefabPath = prefabPath;
        m_Description = description;
    }

    // Getters
    string GetName() { return m_Name; }
    int GetPrice() { return m_Price; }
    string GetPrefabPath() { return m_PrefabPath; }
    string GetDescription() { return m_Description; }

    // Setters
    void SetPrice(int price) { m_Price = price; }
    void SetDescription(string description) { m_Description = description; }

    // Static factory methods for common items
    static EL_FurnitureItem CreateChair()
    {
        return new EL_FurnitureItem("Chair", 500, "{CHAIR_PREFAB_GUID}Prefabs/Items/Furniture/Chair.et", "A comfortable chair");
    }

    static EL_FurnitureItem CreateTable()
    {
        return new EL_FurnitureItem("Table", 1000, "{TABLE_PREFAB_GUID}Prefabs/Items/Furniture/Table.et", "A wooden table");
    }

    static EL_FurnitureItem CreateBed()
    {
        return new EL_FurnitureItem("Bed", 2000, "{BED_PREFAB_GUID}Prefabs/Items/Furniture/Bed.et", "A cozy bed");
    }
};