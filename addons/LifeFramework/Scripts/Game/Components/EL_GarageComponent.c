class EL_GarageComponentClass : ScriptComponentClass
{
};

class EL_GarageComponent : EPF_PersistentScriptedState
{
    [Attribute("0", UIWidgets.EditBox, "Garage ID", "")]
    protected int m_GarageID;

    [Attribute("1000", UIWidgets.EditBox, "Storage Fee", "")]
    protected int m_StorageFee;

    [Attribute("", UIWidgets.EditBox, "Owner UID", "")]
    protected string m_OwnerUID;

    [Attribute("10", UIWidgets.EditBox, "Max Vehicles", "")]
    protected int m_MaxVehicles;

    // Stored vehicles (array of vehicle data)
    protected ref array<ref EL_VehicleData> m_StoredVehicles;

    // Constructor
    void EL_GarageComponent()
    {
        m_StoredVehicles = new array<ref EL_VehicleData>();
        SetPersistentId("Garage_" + m_GarageID.ToString());
    }

    // Getters
    int GetGarageID() { return m_GarageID; }
    int GetStorageFee() { return m_StorageFee; }
    string GetOwnerUID() { return m_OwnerUID; }
    int GetMaxVehicles() { return m_MaxVehicles; }
    array<ref EL_VehicleData> GetStoredVehicles() { return m_StoredVehicles; }

    // Setters
    void SetOwnerUID(string uid) { m_OwnerUID = uid; }

    // Check if player owns this garage
    bool IsOwner(string playerUID)
    {
        return m_OwnerUID == playerUID;
    }

    // Store vehicle
    bool StoreVehicle(EL_VehicleData vehicleData, string playerUID)
    {
        if (!IsOwner(playerUID))
            return false;

        if (m_StoredVehicles.Count() >= m_MaxVehicles)
            return false;

        m_StoredVehicles.Insert(vehicleData);
        SaveGarageData();
        return true;
    }

    // Retrieve vehicle
    EL_VehicleData RetrieveVehicle(int index, string playerUID)
    {
        if (!IsOwner(playerUID) || index < 0 || index >= m_StoredVehicles.Count())
            return null;

        EL_VehicleData vehicle = m_StoredVehicles.Get(index);
        m_StoredVehicles.Remove(index);
        SaveGarageData();
        return vehicle;
    }

    // Save garage data
    void SaveGarageData()
    {
        // EPF handles persistence automatically
        Print("Garage data saved for ID: " + m_GarageID);
    }

    // Load garage data (EPF handles this)
    void LoadGarageData()
    {
        // EPF loads automatically
        Print("Garage data loaded for ID: " + m_GarageID);
    }
};

// Vehicle data class
class EL_VehicleData
{
    protected string m_VehicleType; // Prefab path or type identifier
    protected string m_DisplayName;
    protected int m_StorageCost;

    void EL_VehicleData(string vehicleType, string displayName, int storageCost = 0)
    {
        m_VehicleType = vehicleType;
        m_DisplayName = displayName;
        m_StorageCost = storageCost;
    }

    string GetVehicleType() { return m_VehicleType; }
    string GetDisplayName() { return m_DisplayName; }
    int GetStorageCost() { return m_StorageCost; }
};