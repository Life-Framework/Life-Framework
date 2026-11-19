// LF_ImplementHitch.c: hitch a reaper behind a UAZ469 or a Z7211 (#3284, harvester spike H1). The server spawns
// the implement prefab with the vehicle as its parent (EntitySpawnParams.Parent, the vanilla weapon-rack way), at
// the local offset of the vehicle's row in Configs/Harvest/LF_Hitches.conf. Neither vehicle mesh has a hitch
// pivot, so each offset is measured from the vehicle's bounds. The hierarchy replicates through the
// RplComponents, so other players and late joiners see it. Unhitching deletes the implement.

//------------------------------------------------------------------------------------------------
//! One vehicle that can tow: the prefab (or any ancestor of the vehicle's prefab) and where the implement sits.
[BaseContainerProps()]
class LF_HitchRow
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vehicle prefab; its variants inherit the row", "et")]
	ResourceName m_Vehicle;

	[Attribute("0 0 -3", UIWidgets.EditBox, "Implement origin in the vehicle's frame (metres; the vehicle faces +Z)")]
	vector m_vOffset;

	[Attribute("0 0 0", UIWidgets.EditBox, "Implement yaw pitch roll in the vehicle's frame (degrees)")]
	vector m_vAngles;
}

//------------------------------------------------------------------------------------------------
//! The hitch list. Core file Configs/Harvest/LF_Hitches.conf (base-game vehicles only: a DLC vehicle or a DLC
//! asset in the implement would lock non-owners out of the driver seat).
[BaseContainerProps(configRoot: true)]
class LF_HitchesConfig
{
	static const ResourceName CONFIG_PATH = "{0D81D5676775E291}Configs/Harvest/LF_Hitches.conf";

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The implement hitched by default (the reaper)", "et")]
	ResourceName m_ReaperPrefab;

	[Attribute("", UIWidgets.Object, "Vehicles that can tow an implement")]
	ref array<ref LF_HitchRow> m_aRows;

	//------------------------------------------------------------------------------------------------
	//! The core file, or null (logged): nothing can hitch then.
	static LF_HitchesConfig Load()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error("Harvest", "hitch config does not resolve, nothing can hitch: " + CONFIG_PATH);
			return null;
		}

		LF_HitchesConfig config = LF_HitchesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("Harvest", "hitch config has an unexpected root class: " + CONFIG_PATH);

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The row whose prefab is the vehicle's prefab or one of its ancestors, or null.
	LF_HitchRow FindRow(IEntity vehicle)
	{
		if (!vehicle || !m_aRows)
			return null;

		EntityPrefabData data = vehicle.GetPrefabData();
		if (!data)
			return null;

		BaseContainer prefab = data.GetPrefab();
		while (prefab)
		{
			string name = prefab.GetResourceName();
			foreach (LF_HitchRow row : m_aRows)
			{
				if (row && LF_Utils.SamePrefab(name, row.m_Vehicle))
					return row;
			}

			prefab = prefab.GetAncestor();
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Server API: Hitch, Unhitch, FindHitched. Everything is a static call on the authority.
class LF_ImplementHitch
{
	static const string FEATURE = "Harvest";

	//------------------------------------------------------------------------------------------------
	//! Server: spawn `implementPrefab` (the config's reaper when empty) hitched behind `vehicle`. Returns the
	//! implement, or null with `reason` set and logged: not the authority, a vehicle not on the hitch list, one
	//! that already tows something, or a prefab that does not spawn.
	static IEntity Hitch(IEntity vehicle, out string reason, ResourceName implementPrefab = "")
	{
		reason = "";
		if (!vehicle)
		{
			reason = "no vehicle";
			LF_Debug.Warn(FEATURE, "hitch refused: " + reason);
			return null;
		}

		if (!IsAuthority(vehicle))
		{
			reason = "not the server";
			LF_Debug.Warn(FEATURE, "hitch refused on " + Describe(vehicle) + ": " + reason);
			return null;
		}

		LF_HitchesConfig config = LF_HitchesConfig.Load();
		if (!config)
		{
			reason = "no hitch config";
			return null;
		}

		LF_HitchRow row = config.FindRow(vehicle);
		if (!row)
		{
			reason = "not on the hitch list";
			LF_Debug.Warn(FEATURE, "hitch refused on " + Describe(vehicle) + ": " + reason);
			return null;
		}

		if (FindHitched(vehicle))
		{
			reason = "already towing an implement";
			LF_Debug.Warn(FEATURE, "hitch refused on " + Describe(vehicle) + ": " + reason);
			return null;
		}

		if (implementPrefab.IsEmpty())
			implementPrefab = config.m_ReaperPrefab;

		Resource resource = Resource.Load(implementPrefab);
		if (!resource || !resource.IsValid())
		{
			reason = "implement prefab does not resolve: " + implementPrefab;
			LF_Debug.Error(FEATURE, "hitch failed on " + Describe(vehicle) + ": " + reason);
			return null;
		}

		EntitySpawnParams params();
		params.Parent = vehicle;
		params.TransformMode = ETransformMode.LOCAL;
		Math3D.AnglesToMatrix(row.m_vAngles, params.Transform);
		params.Transform[3] = row.m_vOffset;

		IEntity implement = GetGame().SpawnEntityPrefab(resource, vehicle.GetWorld(), params);
		if (!implement)
		{
			reason = "implement did not spawn: " + implementPrefab;
			LF_Debug.Error(FEATURE, "hitch failed on " + Describe(vehicle) + ": " + reason);
			return null;
		}

		if (!LF_ImplementComponent.Of(implement))
		{
			reason = "prefab has no LF_ImplementComponent: " + implementPrefab;
			LF_Debug.Error(FEATURE, "hitch failed on " + Describe(vehicle) + ": " + reason);
			RplComponent.DeleteRplEntity(implement, false);
			return null;
		}

		LF_Debug.Info(FEATURE, string.Format("hitched %1 (rpl %2) to %3 at local %4", implementPrefab, RplIdOf(implement), Describe(vehicle), row.m_vOffset));
		return implement;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: delete the implement the vehicle tows. False (logged) when there is none or this is not the server.
	static bool Unhitch(IEntity vehicle)
	{
		if (!vehicle || !IsAuthority(vehicle))
		{
			LF_Debug.Warn(FEATURE, "unhitch refused: no vehicle, or not the server");
			return false;
		}

		IEntity implement = FindHitched(vehicle);
		if (!implement)
		{
			LF_Debug.Warn(FEATURE, "unhitch refused on " + Describe(vehicle) + ": nothing hitched");
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("unhitched rpl %1 from %2; the implement is deleted", RplIdOf(implement), Describe(vehicle)));
		RplComponent.DeleteRplEntity(implement, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The implement a vehicle tows (a direct child with LF_ImplementComponent), or null. Any machine.
	static IEntity FindHitched(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		IEntity child = vehicle.GetChildren();
		while (child)
		{
			if (LF_ImplementComponent.Of(child))
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The hitch row for a vehicle, or null. For callers that only ask "can this tow" (a rack, a prompt).
	static LF_HitchRow RowFor(IEntity vehicle)
	{
		LF_HitchesConfig config = LF_HitchesConfig.Load();
		if (!config)
			return null;

		return config.FindRow(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! An entity without an RplComponent is local and its own authority (a test fixture).
	protected static bool IsAuthority(IEntity entity)
	{
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		return !rpl || rpl.IsMaster();
	}

	//------------------------------------------------------------------------------------------------
	//! "rpl <id>" of the entity's RplComponent, or "local".
	static string RplIdOf(IEntity entity)
	{
		if (!entity)
			return "none";

		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return "local";

		return rpl.Id().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle's prefab name and rpl id, for log lines.
	static string Describe(IEntity vehicle)
	{
		if (!vehicle)
			return "no vehicle";

		string name = "?";
		EntityPrefabData data = vehicle.GetPrefabData();
		if (data)
			name = data.GetPrefabName();

		return name + " (rpl " + RplIdOf(vehicle) + ")";
	}
}
