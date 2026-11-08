// Author: LNGCGenetic
// Copyright (c) LNGCGenetic
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following
// conditions:
// The above copyright notice and this permission notice shall be included in all copies
// or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
// Ported from LNGC_GarageComponent. One shared garage network: any garage retrieves any
// of the player's stored vehicles. Cash stays MoneyStack. No LNGC_ types.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Vehicles", description: "Marks a garage. Any garage can store or retrieve an owned vehicle.")]
class EL_VehicleGarageComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleGarageComponent : ScriptComponent
{
	static const ResourceName GARAGE_SHELL = "{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et";
	protected static const float INTERACT_RADIUS = 15.0;
	protected static ref array<EL_VehicleGarageComponent> s_aAll = {};
	protected bool m_bRegistered;

	//------------------------------------------------------------------------------------------------
	static array<EL_VehicleGarageComponent> GetAll()
	{
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_bRegistered)
			return;

		m_bRegistered = true;
		s_aAll.Insert(this);
		EL_Debug.Log("Vehicle", "garage registered");
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPositionNearAnyGarage(vector origin)
	{
		foreach (EL_VehicleGarageComponent garage : s_aAll)
		{
			if (!garage || !garage.GetOwner())
				continue;

			if (vector.Distance(origin, garage.GetOwner().GetOrigin()) <= INTERACT_RADIUS)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when both the player and the vehicle stand within INTERACT_RADIUS of garageOrigin.
	static bool SharesGarage(vector garageOrigin, vector playerOrigin, vector vehicleOrigin)
	{
		if (vector.Distance(playerOrigin, garageOrigin) > INTERACT_RADIUS)
			return false;

		return vector.Distance(vehicleOrigin, garageOrigin) <= INTERACT_RADIUS;
	}

	//------------------------------------------------------------------------------------------------
	//! Storing takes the vehicle off the map, so the vehicle has to be at the garage the player is
	//! using. Checking only the player let an owner store a car from anywhere, including one in
	//! a police impound.
	//! \return true when one garage has both the player and the vehicle within its radius.
	static bool IsVehicleAtPlayersGarage(vector playerOrigin, vector vehicleOrigin)
	{
		foreach (EL_VehicleGarageComponent garage : s_aAll)
		{
			if (!garage || !garage.GetOwner())
				continue;

			if (SharesGarage(garage.GetOwner().GetOrigin(), playerOrigin, vehicleOrigin))
				return true;
		}

		return false;
	}
}
