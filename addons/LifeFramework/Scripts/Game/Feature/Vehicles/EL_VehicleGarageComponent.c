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
[ComponentEditorProps(category: "EveronLife/Feature/Vehicles", description: "Marks a garage. Any garage stores an owned vehicle parked at it and retrieves onto its own pad.")]
class EL_VehicleGarageComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleGarageComponent : ScriptComponent
{
	static const ResourceName GARAGE_SHELL = "{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et";
	static const float INTERACT_RADIUS = 15.0;

	[Attribute("", UIWidgets.EditBox, desc: "Pad id of the EL_VehicleSpawnPointComponent a retrieved vehicle is placed on, in front of this garage.")]
	protected string m_sPadId;
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
		if (m_sPadId.IsEmpty())
			EL_Debug.Error("Vehicle", "garage has no pad id: retrieve is refused there");
		else
			EL_Debug.Log("Vehicle", "garage registered pad=" + m_sPadId);
	}

	//------------------------------------------------------------------------------------------------
	string GetPadId()
	{
		return m_sPadId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The first garage whose radius holds origin, or null.
	static EL_VehicleGarageComponent FindAt(vector origin)
	{
		foreach (EL_VehicleGarageComponent garage : s_aAll)
		{
			if (!garage || !garage.GetOwner())
				continue;

			if (vector.Distance(origin, garage.GetOwner().GetOrigin()) <= INTERACT_RADIUS)
				return garage;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bRegistered)
		{
			s_aAll.RemoveItem(this);
			m_bRegistered = false;
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Game-mode start/end reset (LF_StaticReset).
	static void ResetStatics()
	{
		s_aAll.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPositionNearAnyGarage(vector origin)
	{
		return FindAt(origin) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Storing takes the vehicle off the map, so the vehicle has to be at the garage the player is
	//! using. Checking only the player let an owner store a car from anywhere.
	//! \return true when both the player and the vehicle stand within INTERACT_RADIUS of garageOrigin.
	static bool SharesGarage(vector garageOrigin, vector playerOrigin, vector vehicleOrigin)
	{
		if (vector.Distance(playerOrigin, garageOrigin) > INTERACT_RADIUS)
			return false;

		return vector.Distance(vehicleOrigin, garageOrigin) <= INTERACT_RADIUS;
	}
}
