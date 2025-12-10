class EL_PropertyManager : ScriptedUserAction
{
	protected static EL_PropertyManager s_Instance;
	protected ref map<int, EL_PropertyComponent> m_mProperties = new map<int, EL_PropertyComponent>();

	//------------------------------------------------------------------------------------------------
	static EL_PropertyManager GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void EL_PropertyManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_PropertyManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Register a property component
	void RegisterProperty(EL_PropertyComponent property)
	{
		if (property)
		{
			m_mProperties.Set(property.GetPropertyID(), property);
			Print("Property registered: " + property.GetPropertyID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Unregister a property component
	void UnregisterProperty(int propertyId)
	{
		m_mProperties.Remove(propertyId);
		Print("Property unregistered: " + propertyId);
	}

	//------------------------------------------------------------------------------------------------
	//! Get property by ID
	EL_PropertyComponent GetProperty(int propertyId)
	{
		return m_mProperties.Get(propertyId);
	}

	//------------------------------------------------------------------------------------------------
	//! Get all properties owned by a player
	array<EL_PropertyComponent> GetPropertiesByOwner(string playerUID)
	{
		array<EL_PropertyComponent> ownedProperties = new array<EL_PropertyComponent>();
		for (int i = 0; i < m_mProperties.Count(); i++)
		{
			int id = m_mProperties.GetKey(i);
			EL_PropertyComponent prop = m_mProperties.GetElement(i);
			if (prop && prop.GetOwnerUID() == playerUID)
			{
				ownedProperties.Insert(prop);
			}
		}
		return ownedProperties;
	}

	//------------------------------------------------------------------------------------------------
	//! Check if property exists and is available for purchase
	bool IsPropertyAvailable(int propertyId)
	{
		EL_PropertyComponent prop = GetProperty(propertyId);
		return prop && prop.GetOwnerUID() == "";
	}

	//------------------------------------------------------------------------------------------------
	//! Attempt to purchase property
	bool PurchaseProperty(int propertyId, string playerUID)
	{
		EL_PropertyComponent prop = GetProperty(propertyId);
		if (!prop || prop.GetOwnerUID() != "")
			return false;

		// Here we could add money check, but for now assume it's handled in action
		prop.SetOwnerUID(playerUID);
		Print("Property " + propertyId + " purchased by " + playerUID);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Get total property count
	int GetPropertyCount()
	{
		return m_mProperties.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Save all properties (EPF handles persistence automatically via components)
	void SaveAllProperties()
	{
		// EPF_PersistentScriptedState handles saving automatically
		Print("Properties persistence handled by EPF");
	}
};