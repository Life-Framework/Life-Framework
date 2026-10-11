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
}
