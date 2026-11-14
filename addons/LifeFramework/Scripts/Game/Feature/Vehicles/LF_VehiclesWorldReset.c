//------------------------------------------------------------------------------------------------
//! Vehicles part of the world reset (Core/LF_WorldReset.c): every vehicle standing in the world
//! at game start goes back to where it stood. Vehicles that came later (bought, spawned) stay,
//! and the cash van keeps to its own route.

modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Vehicles());
	}
}

//------------------------------------------------------------------------------------------------
class LF_VehicleSpot : Managed
{
	EntityID m_Id;
	vector m_aTransform[4];
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Vehicles : LF_WorldResetStep
{
	protected ref array<ref LF_VehicleSpot> m_aSpots = {};

	override string GetName()
	{
		return "vehicles";
	}

	override void OnStart()
	{
		m_aSpots.Clear();
		BaseWorld world = GetGame().GetWorld();
		vector mins, maxs;
		world.GetBoundBox(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		// The cash van keeps to its route: its box moves with it.
		array<string> vans = {};
		array<IEntity> all = LF_WorldResetScan.All();
		foreach (IEntity entity : all)
		{
			LF_CashVanRouteComponent route = LF_CashVanRouteComponent.Cast(entity.FindComponent(LF_CashVanRouteComponent));
			if (route)
				vans.Insert(route.GetVanEntityName());
		}

		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(centre, vector.Distance(mins, maxs) * 0.5 + 100);
		foreach (IEntity vehicle : vehicles)
		{
			if (!vehicle || (!vehicle.GetName().IsEmpty() && vans.Find(vehicle.GetName()) >= 0))
				continue;

			LF_VehicleSpot spot = new LF_VehicleSpot();
			spot.m_Id = vehicle.GetID();
			vehicle.GetWorldTransform(spot.m_aTransform);
			m_aSpots.Insert(spot);
		}

		LF_Debug.Info(LF_WorldReset.FEATURE, string.Format("%1 vehicles remembered at their spots", m_aSpots.Count()));
	}

	override int Run()
	{
		int moved = 0;
		foreach (LF_VehicleSpot spot : m_aSpots)
		{
			IEntity vehicle = GetGame().GetWorld().FindEntityByID(spot.m_Id);
			if (!vehicle)
				continue;

			vector current[4];
			vehicle.GetWorldTransform(current);
			if (vector.DistanceSq(current[3], spot.m_aTransform[3]) < 0.25)
				continue;

			vehicle.SetTransform(spot.m_aTransform);
			Physics physics = vehicle.GetPhysics();
			if (physics)
			{
				physics.SetVelocity(vector.Zero);
				physics.SetAngularVelocity(vector.Zero);
			}

			moved++;
		}

		return moved;
	}
}
