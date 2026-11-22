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
class LF_VehicleSpawnPointComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_VehicleSpawnPointComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Pad id. The lot, a garage, or an impound desk names the pads it spawns on.")]
	protected string m_sShopId;

	[Attribute("0 0 8", UIWidgets.Auto, desc: "Offset from this marker to the first pad.")]
	protected vector m_vOffset;

	[Attribute("3", UIWidgets.Auto, desc: "Number of pads along the step.")]
	protected int m_iSlots;

	[Attribute("0 0 8", UIWidgets.Auto, desc: "World offset from one pad to the next.")]
	protected vector m_vSlotStep;

	[Attribute("0 0 0", UIWidgets.Auto, desc: "Angles (yaw pitch roll, degrees) of a vehicle placed on these pads.")]
	protected vector m_vAngles;

	//! Metres from a vehicle's origin to the far end of its body (half an M923 truck, rounded up).
	static const float PAD_VEHICLE_REACH = 5;

	protected static ref array<LF_VehicleSpawnPointComponent> s_aAll = {};
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
		foreach (LF_VehicleSpawnPointComponent point : s_aAll)
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
			LF_Debug.Error("Vehicle", "spawn point has no shop id");
			return;
		}

		if (m_bRegistered)
			return;

		m_bRegistered = true;
		s_aAll.Insert(this);
		LF_Debug.Log("Vehicle", string.Format("spawn point registered shop=%1", m_sShopId));
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
	static bool FindFreeOrigin(string shopId, out vector origin)
	{
		vector angles;
		return FindFreePad(shopId, origin, angles);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] angles Yaw pitch roll for a vehicle placed on the free pad.
	static bool FindFreePad(string shopId, out vector origin, out vector angles)
	{
		origin = "0 0 0";
		angles = "0 0 0";
		if (shopId.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "spawn refused: shop id is empty");
			return false;
		}

		foreach (LF_VehicleSpawnPointComponent point : s_aAll)
		{
			if (!point || point.GetShopId() != shopId)
				continue;

			if (point.TryNextOrigin(origin))
			{
				angles = point.m_vAngles;
				return true;
			}
		}

		LF_Debug.Error("Vehicle", string.Format("spawn refused: no free pad for %1", shopId));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle the sphere query returns takes the pad only when its origin is close enough for its
	//! body to reach the pad. The query tests bounds, and a vehicle teleported while asleep (the cash
	//! van between its stops) keeps bounds spanning its whole route: 36 m and 211 m away, it filled
	//! the island lot's pads and no second car could be bought.
	static bool IsNearPad(vector vehicleOrigin, vector slot, float radius)
	{
		return vector.Distance(vehicleOrigin, slot) <= radius + PAD_VEHICLE_REACH;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryNextOrigin(out vector origin)
	{
		origin = "0 0 0";
		IEntity owner = GetOwner();
		if (!owner)
		{
			LF_Debug.Error("Vehicle", "spawn refused: spawn point has no entity");
			return false;
		}

		vector base = owner.GetOrigin() + m_vOffset;
		for (int i = 0; i < m_iSlots; i++)
		{
			float steps = i;
			vector slot = base + m_vSlotStep * steps;
			float radius = LF_GarageConfig.PadOccupiedRadius();
			array<IEntity> nearby = LF_Utils.GetNearbyVehicles(slot, radius);
			IEntity blocker;
			if (nearby)
			{
				foreach (IEntity vehicle : nearby)
				{
					if (vehicle && IsNearPad(vehicle.GetOrigin(), slot, radius))
					{
						blocker = vehicle;
						break;
					}
				}
			}

			if (!blocker)
			{
				origin = slot;
				return true;
			}

			LF_Debug.Log("Vehicle", string.Format("pad %1 slot %2 at %3 is taken by %4 at %5", m_sShopId, i, slot, LF_Utils.GetPrefabName(blocker), blocker.GetOrigin()));
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleSpawnPointComponent's statics (LF_StaticReset finds it by reflection).
class LF_VehicleSpawnPointComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleSpawnPointComponent";
	}

	override void Apply()
	{
		LF_VehicleSpawnPointComponent.ResetStatics();
	}
}
