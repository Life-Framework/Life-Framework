//------------------------------------------------------------------------------------------------
//! Pack overlays of the fishing config (#3451): a pack adds, replaces, patches or removes species and
//! tables by id, as every other section does (LF_ConfigOverlay, docs/config/override-model.md). The
//! core file stays the shipped neutral set; the pack's coves are tables of their own, in
//! Configs/Fishing/*.conf of class LF_FishingConfig listed in the mission header. Rods, baits and
//! the global numbers stay core. LF_FishingConfig.Get applies the layers before it sanitizes.
class LF_FishingLayers
{
	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of the fishing config, in load order, over `config`.
	//! \return Rows changed.
	static int Apply(notnull LF_FishingConfig config)
	{
		if (!config.m_aSpecies)
			config.m_aSpecies = {};
		if (!config.m_aTables)
			config.m_aTables = {};

		LF_FishSpeciesList species = new LF_FishSpeciesList(config);
		LF_FishTableList tables = new LF_FishTableList(config);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_FishingConfig.CONFIG_PATH, LF_FishingConfig, "fishing", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		int total;
		foreach (int i, Resource resource : resources)
		{
			BaseContainer root = resource.GetResource().ToBaseContainer();
			int changed = LF_ConfigOverlay.ApplyContainerList(species, root, "m_aSpecies", names[i], claims);
			changed += LF_ConfigOverlay.ApplyContainerList(tables, root, "m_aTables", names[i], claims);
			total += changed;
			LF_Debug.Info(LF_FishingConfig.FEATURE, string.Format("pack overlay %1 changed %2 fishing rows", names[i], changed));
		}

		return total;
	}
}

//------------------------------------------------------------------------------------------------
//! The fishing config's `m_aSpecies` list for the shared merge, keyed by species id.
class LF_FishSpeciesList : LF_ConfigList
{
	protected LF_FishingConfig m_Config;

	void LF_FishSpeciesList(LF_FishingConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "fishing.species";
	}

	override int Count()
	{
		return m_Config.m_aSpecies.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aSpecies[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_FishSpeciesDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a species id such as bass";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_FishSpeciesDef.Make(id, null, null, null, 0);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_FishSpeciesDef def = LF_FishSpeciesDef.Cast(record);
		if (index < m_Config.m_aSpecies.Count())
			m_Config.m_aSpecies.Set(index, def);
		else
			m_Config.m_aSpecies.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aSpecies.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_FishSpeciesDef def = LF_FishSpeciesDef.Cast(record);
		int changed;
		// An array a pack sets replaces the shipped one whole.
		if (container.IsVariableSetDirectly("m_aSizePrefabs"))
		{
			array<ResourceName> prefabs = {};
			if (container.Get("m_aSizePrefabs", prefabs))
			{
				def.m_aSizePrefabs = new array<ResourceName>();
				def.m_aSizePrefabs.Copy(prefabs);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_aWater"))
		{
			array<string> water = {};
			if (container.Get("m_aWater", water))
			{
				def.m_aWater = new array<string>();
				def.m_aWater.Copy(water);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_aHours"))
		{
			array<int> hours = {};
			if (container.Get("m_aHours", hours))
			{
				def.m_aHours = new array<int>();
				def.m_aHours.Copy(hours);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_iMinFisherLevel") && container.Get("m_iMinFisherLevel", def.m_iMinFisherLevel))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_FishSpeciesDef def = LF_FishSpeciesDef.Cast(record);
		if (!def.m_aSizePrefabs || def.m_aSizePrefabs.Count() != LF_FishingConfig.SIZE_COUNT)
			return "a species needs exactly three size prefabs (small, medium, large)";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The fishing config's `m_aTables` list for the shared merge, keyed by table id.
class LF_FishTableList : LF_ConfigList
{
	protected LF_FishingConfig m_Config;

	void LF_FishTableList(LF_FishingConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "fishing.tables";
	}

	override int Count()
	{
		return m_Config.m_aTables.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aTables[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_FishTableDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a table id such as cove";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_FishTableDef.Make(id, null, null);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_FishTableDef def = LF_FishTableDef.Cast(record);
		if (index < m_Config.m_aTables.Count())
			m_Config.m_aTables.Set(index, def);
		else
			m_Config.m_aTables.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aTables.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_FishTableDef def = LF_FishTableDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_aSizeWeights"))
		{
			array<int> weights = {};
			if (container.Get("m_aSizeWeights", weights))
			{
				def.m_aSizeWeights = new array<int>();
				def.m_aSizeWeights.Copy(weights);
				changed++;
			}
		}

		// The species entries a pack writes replace the table's whole list.
		if (container.IsVariableSetDirectly("m_aSpecies"))
		{
			def.m_aSpecies = new array<ref LF_FishWeightDef>();
			BaseContainerList entries = container.GetObjectArray("m_aSpecies");
			if (entries)
			{
				for (int i = 0; i < entries.Count(); i++)
				{
					BaseContainer entry = entries.Get(i);
					if (!entry)
						continue;

					LF_FishWeightDef weight = LF_FishWeightDef.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
					if (weight)
						def.m_aSpecies.Insert(weight);
				}
			}

			changed++;
		}

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_FishTableDef.Cast(record).Problem();
	}
}
