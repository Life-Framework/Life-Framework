class EL_GarageManager : ScriptedUserAction
{
	protected static EL_GarageManager s_Instance;
	protected ref map<int, EL_GarageComponent> m_mGarages = new map<int, EL_GarageComponent>();

	//------------------------------------------------------------------------------------------------
	static EL_GarageManager GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void EL_GarageManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_GarageManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Register a garage component
	void RegisterGarage(EL_GarageComponent garage)
	{
		if (garage)
		{
			m_mGarages.Set(garage.GetGarageID(), garage);
			Print("Garage registered: " + garage.GetGarageID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Unregister a garage component
	void UnregisterGarage(int garageId)
	{
		m_mGarages.Remove(garageId);
		Print("Garage unregistered: " + garageId);
	}

	//------------------------------------------------------------------------------------------------
	//! Get garage by ID
	EL_GarageComponent GetGarage(int garageId)
	{
		return m_mGarages.Get(garageId);
	}

	//------------------------------------------------------------------------------------------------
	//! Get garages by owner
	array<EL_GarageComponent> GetGaragesByOwner(string playerUID)
	{
		array<EL_GarageComponent> ownedGarages = new array<EL_GarageComponent>();
		for (int i = 0; i < m_mGarages.Count(); i++)
		{
			int id = m_mGarages.GetKey(i);
			EL_GarageComponent garage = m_mGarages.GetElement(i);
			if (garage && garage.GetOwnerUID() == playerUID)
			{
				ownedGarages.Insert(garage);
			}
		}
		return ownedGarages;
	}

	//------------------------------------------------------------------------------------------------
	//! Check if garage exists
	bool GarageExists(int garageId)
	{
		return m_mGarages.Contains(garageId);
	}

	//------------------------------------------------------------------------------------------------
	//! Get total garage count
	int GetGarageCount()
	{
		return m_mGarages.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Save all garages (EPF handles persistence automatically)
	void SaveAllGarages()
	{
		// EPF_PersistentScriptedState handles saving automatically
		Print("Garages persistence handled by EPF");
	}
};