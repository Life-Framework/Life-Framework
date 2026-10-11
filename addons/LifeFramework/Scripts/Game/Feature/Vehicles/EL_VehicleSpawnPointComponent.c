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
// Ported from LNGC_VehicleSpawnPointComponent. The pad is tied to a shop id. A restart
// spawns a vehicle here instead of at its last world position, the same rule as a
// character spawn point. Slots along the offset keep a second purchase off the first car.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Vehicles", description: "Fixed pad where a lot spawns and retrieves vehicles.")]
class EL_VehicleSpawnPointComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleSpawnPointComponent : ScriptComponent
{
	protected static const float OCCUPIED_RADIUS = 3.0;
	protected static const int SLOT_COUNT = 3;
	protected static const float SLOT_STEP = 8.0;

	[Attribute("", UIWidgets.EditBox, desc: "Shop id. Must match the lot.")]
	protected string m_sShopId;

	[Attribute("0 0 8", UIWidgets.Auto, desc: "Offset from this marker to the first pad.")]
	protected vector m_vOffset;

	protected static ref array<EL_VehicleSpawnPointComponent> s_aAll = {};
	protected bool m_bRegistered;

	//------------------------------------------------------------------------------------------------
	string GetShopId()
	{
		return m_sShopId;
	}

	//------------------------------------------------------------------------------------------------
	static int CountForShop(string shopId)
	{
		int count = 0;
		foreach (EL_VehicleSpawnPointComponent point : s_aAll)
		{
			if (point && point.GetShopId() == shopId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sShopId.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "spawn point has no shop id");
			return;
		}

		if (m_bRegistered)
			return;

		m_bRegistered = true;
		s_aAll.Insert(this);
		EL_Debug.Log("Vehicle", string.Format("spawn point registered shop=%1", m_sShopId));
	}

	//------------------------------------------------------------------------------------------------
	static bool FindFreeOrigin(string shopId, out vector origin)
	{
		origin = "0 0 0";
		if (shopId.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "spawn refused: shop id is empty");
			return false;
		}

		foreach (EL_VehicleSpawnPointComponent point : s_aAll)
		{
			if (!point || point.GetShopId() != shopId)
				continue;

			if (point.TryNextOrigin(origin))
				return true;
		}

		EL_Debug.Error("Vehicle", string.Format("spawn refused: no free pad for %1", shopId));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryNextOrigin(out vector origin)
	{
		origin = "0 0 0";
		IEntity owner = GetOwner();
		if (!owner)
		{
			EL_Debug.Error("Vehicle", "spawn refused: spawn point has no entity");
			return false;
		}

		vector base = owner.GetOrigin() + m_vOffset;
		for (int i = 0; i < SLOT_COUNT; i++)
		{
			vector slot = base + Vector(0, 0, i * SLOT_STEP);
			array<IEntity> nearby = EL_Utils.GetNearbyVehicles(slot, OCCUPIED_RADIUS);
			if (!nearby || nearby.Count() == 0)
			{
				origin = slot;
				return true;
			}
		}

		return false;
	}
}
