//------------------------------------------------------------------------------------------------
//! What a plant, water or harvest request did (#2664). OK did it.
enum LF_EPlantResult
{
	OK,
	NOT_SERVER,
	SYSTEM_OFF,
	BAD_CALLER,
	NO_CROP,
	NO_SEED,
	PLACE_REFUSED,
	NO_PLANT,
	TOO_FAR,
	NO_WATER,
	TOO_EARLY,
	NOT_GROWING,
	NOT_RIPE,
	CLEARED,
	GRANT_FAILED
}

//------------------------------------------------------------------------------------------------
//! One planted crop as the save keeps it (the "plants" payload). m_iPlantedAt and m_iWateredAt are
//! seconds on the farming clock (LF_Plants.Now), which runs only while a server runs with the Farming
//! system on. In a save they are relative to the save moment (zero or less), so a restart picks the
//! crop up where it was. m_sOwnerKey is the planter's player UID; anyone may harvest.
class LF_PlantRow : Managed
{
	string m_sPlantId;
	string m_sCropId;
	vector m_vPosition;
	int m_iPlantedAt;
	int m_iWateredAt;
	string m_sOwnerKey;

	//------------------------------------------------------------------------------------------------
	LF_PlantRow Copy()
	{
		LF_PlantRow copy = new LF_PlantRow();
		copy.m_sPlantId = m_sPlantId;
		copy.m_sCropId = m_sCropId;
		copy.m_vPosition = m_vPosition;
		copy.m_iPlantedAt = m_iPlantedAt;
		copy.m_iWateredAt = m_iWateredAt;
		copy.m_sOwnerKey = m_sOwnerKey;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Crops planted on open ground (#2664, spec #809): plant a seed, water it inside its window,
//! harvest it ripe. Anyone may harvest anyone's crop. Server only: the rows are the truth, and the
//! meshes are local copies respawned from them (on a listen host or Workbench; a dedicated server
//! draws nothing). Growth is derived from the two clocks in the row through LF_PlantRules, so no
//! per-plant state needs a tick: the 1 s tick only advances the farming clock and refreshes meshes.
class LF_Plants
{
	static const string FEATURE = "Farming";
	//! How far above the spot the open-ground trace starts, metres: above any roof of a house (#2821).
	static const float OPEN_TRACE_UP_M = 12.0;	// lf-fixed: open-ground trace height
	//! A trace hit this far above the terrain at the spot counts as something standing over it, metres (#2821).
	static const float OPEN_COVER_MIN_M = 0.3;	// lf-fixed: open-ground cover height
	static const int TICK_MS = 1000;	// lf-fixed: the farming clock tick, as LF_FarmPlots
	//! Metres a gardener may stand from a plant to water, harvest or inspect it.
	static const float REACH = LF_PlantRules.MAX_REACH;	// lf-fixed: the planting reach of spec #809
	//! Items that count as a water container. The core water bottle until a watering can exists.
	static const ResourceName WATER_BOTTLE = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";

	protected static ref map<string, ref LF_PlantRow> s_mPlants;
	protected static ref map<string, EntityID> s_mVisuals;
	protected static ref map<string, int> s_mShownStage;
	protected static ref LF_CropCatalogue s_CatalogueOverride;
	protected static bool s_bNoCatalogueForTest;
	protected static float s_fClock;
	protected static int s_iNextId;
	protected static bool s_bTicking;
	protected static float s_fLastTickMs = -1;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_PlantRow> Plants()
	{
		if (!s_mPlants)
			s_mPlants = new map<string, ref LF_PlantRow>();

		return s_mPlants;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> Visuals()
	{
		if (!s_mVisuals)
			s_mVisuals = new map<string, EntityID>();

		return s_mVisuals;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> ShownStages()
	{
		if (!s_mShownStage)
			s_mShownStage = new map<string, int>();

		return s_mShownStage;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		ClearAllVisuals();
		s_mPlants = null;
		s_mVisuals = null;
		s_mShownStage = null;
		s_CatalogueOverride = null;
		s_bNoCatalogueForTest = false;
		s_fClock = 0;
		s_iNextId = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: plant from this catalogue instead of the config one (null restores the config).
	static void SetCatalogue(LF_CropCatalogue catalogue)
	{
		s_CatalogueOverride = catalogue;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: act as if the crop catalogue failed to load (true) until ResetStatics.
	static void SimulateNoCatalogue(bool missing)
	{
		s_bNoCatalogueForTest = missing;
	}

	//------------------------------------------------------------------------------------------------
	//! The catalogue plants are read from: a test's, else the config one; null when it failed to load.
	protected static LF_CropCatalogue Catalogue()
	{
		if (s_bNoCatalogueForTest)
			return null;

		if (s_CatalogueOverride)
			return s_CatalogueOverride;

		return LF_CropCatalogue.Get();
	}

	//------------------------------------------------------------------------------------------------
	static LF_CropRow FindCrop(string cropId)
	{
		LF_CropCatalogue catalogue = Catalogue();
		if (!catalogue)
			return null;

		return catalogue.FindIn(cropId);
	}

	//------------------------------------------------------------------------------------------------
	static LF_CropRow FindCropBySeed(ResourceName seed)
	{
		LF_CropCatalogue catalogue = Catalogue();
		if (!catalogue)
			return null;

		return catalogue.FindBySeed(seed);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds on the farming clock.
	static int Now()
	{
		return Math.Floor(s_fClock);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the farming clock moves on by seconds (the tick, or a test) and meshes follow their stage.
	//! While the Farming system is off nothing grows and nothing dries out.
	static void AdvanceClock(float seconds)
	{
		if (seconds <= 0 || !LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM))
			return;

		s_fClock += seconds;
		foreach (string id, LF_PlantRow row : Plants())
			RefreshVisual(row);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking)
			return;

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetCallqueue())
			return;

		s_bTicking = true;
		s_fLastTickMs = -1;
		game.GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!Replication.IsServer())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float nowMs = world.GetWorldTime();
		if (s_fLastTickMs < 0 || nowMs < s_fLastTickMs)
		{
			s_fLastTickMs = nowMs;
			return;
		}

		float seconds = (nowMs - s_fLastTickMs) / 1000;
		s_fLastTickMs = nowMs;
		AdvanceClock(seconds);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Plants().Count();
	}

	//------------------------------------------------------------------------------------------------
	static int CountByOwner(string ownerKey)
	{
		int count;
		foreach (string id, LF_PlantRow row : Plants())
		{
			if (row.m_sOwnerKey == ownerKey)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! A copy of the live row, or null.
	static LF_PlantRow Find(string plantId)
	{
		LF_PlantRow row = Plants().Get(plantId);
		if (!row)
			return null;

		return row.Copy();
	}

	//------------------------------------------------------------------------------------------------
	//! The plant nearest to position within maxDistance, or empty.
	static string FindNearest(vector position, float maxDistance)
	{
		string best;
		float bestDistance = maxDistance;
		foreach (string id, LF_PlantRow row : Plants())
		{
			float distance = vector.Distance(row.m_vPosition, position);
			if (distance <= bestDistance)
			{
				best = id;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The plant's clocks run through the rules up to now: its status, growth and thirst.
	static LF_PlantState StateOf(LF_PlantRow row, LF_CropRow crop)
	{
		LF_PlantState state = new LF_PlantState();
		if (!row || !crop)
			return state;

		// At the last watering the plant was growing and had just been watered.
		state.m_fGrownSeconds = row.m_iWateredAt - row.m_iPlantedAt;
		if (state.m_fGrownSeconds >= crop.m_iGrowthSeconds)
		{
			state.m_fGrownSeconds = crop.m_iGrowthSeconds;
			state.m_eStatus = LF_EPlantStatus.RIPE;
		}

		LF_PlantRules.Advance(crop, state, Now() - row.m_iWateredAt);
		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character plants seedPrefab at spot. The crop is the catalogue row of that seed.
	static LF_EPlantResult Plant(IEntity user, ResourceName seedPrefab, vector spot, out string plantId, out LF_EPlantPlace place)
	{
		return PlantAs(user, LF_Utils.GetPlayerUID(user), seedPrefab, spot, plantId, place);
	}

	//------------------------------------------------------------------------------------------------
	//! Plant with the owner key given: the planter's player UID (Plant), or a test's key.
	static LF_EPlantResult PlantAs(IEntity user, string ownerKey, ResourceName seedPrefab, vector spot, out string plantId, out LF_EPlantPlace place)
	{
		place = LF_EPlantPlace.OK;
		if (!Replication.IsServer())
			return LF_EPlantResult.NOT_SERVER;

		if (LF_Systems.Refuses(LF_FarmPlots.SYSTEM, "planting a seed"))
			return LF_EPlantResult.SYSTEM_OFF;

		if (!user || ownerKey.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "plant refused: no character behind the request");
			return LF_EPlantResult.BAD_CALLER;
		}

		LF_CropRow crop = FindCropBySeed(seedPrefab);
		if (!crop || crop.m_sCropId.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("plant refused: %1 is not a seed of any crop", seedPrefab));
			return LF_EPlantResult.NO_CROP;
		}

		spot = SnapToGround(spot);
		float nearest = -1;
		string nearestId = FindNearest(spot, LF_PlantRules.MIN_SPACING * 4);
		if (!nearestId.IsEmpty())
			nearest = vector.Distance(Plants().Get(nearestId).m_vPosition, spot);

		bool inVehicle;
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (character)
			inVehicle = character.IsInVehicle();

		place = LF_PlantRules.CanPlace(nearest, vector.Distance(user.GetOrigin(), spot), inVehicle, LF_Zones.IsSafe(spot), CountByOwner(ownerKey), Count());
		if (place == LF_EPlantPlace.OK)
			place = OpenGroundAt(user, spot);

		if (place != LF_EPlantPlace.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 may not plant %2 at %3: %4", ownerKey, crop.m_sCropId, spot, typename.EnumToString(LF_EPlantPlace, place)));
			return LF_EPlantResult.PLACE_REFUSED;
		}

		if (LF_InventoryUtils.RemoveAmount(user, crop.m_SeedPrefab, 1) != 1)
			return LF_EPlantResult.NO_SEED;

		s_iNextId++;
		LF_PlantRow row = new LF_PlantRow();
		row.m_sPlantId = string.Format("p%1-%2", System.GetUnixTime(), s_iNextId);
		row.m_sCropId = crop.m_sCropId;
		row.m_vPosition = spot;
		row.m_iPlantedAt = Now();
		row.m_iWateredAt = Now();
		row.m_sOwnerKey = ownerKey;
		Plants().Set(row.m_sPlantId, row);
		plantId = row.m_sPlantId;
		RefreshVisual(row);
		StartTicking();
		LF_Debug.Info(FEATURE, string.Format("%1 planted %2 as %3 at %4", ownerKey, crop.m_sCropId, plantId, spot));
		return LF_EPlantResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Does this character carry any water container.
	static bool HasWater(IEntity user)
	{
		return user && LF_InventoryUtils.GetAmount(user, WATER_BOTTLE) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character with a water container waters a plant. Anyone may water any plant.
	static LF_EPlantResult Water(IEntity user, string plantId)
	{
		LF_PlantRow row;
		LF_CropRow crop;
		LF_EPlantResult reach = Reach(user, plantId, "watering", row, crop);
		if (reach != LF_EPlantResult.OK)
			return reach;

		if (!HasWater(user))
			return LF_EPlantResult.NO_WATER;

		LF_PlantState state = StateOf(row, crop);
		LF_EPlantWater watered = LF_PlantRules.Water(crop, state);
		if (watered == LF_EPlantWater.TOO_EARLY)
			return LF_EPlantResult.TOO_EARLY;

		if (watered != LF_EPlantWater.OK)
			return LF_EPlantResult.NOT_GROWING;

		row.m_iWateredAt = Now();
		LF_Debug.Info(FEATURE, string.Format("%1 watered %2 (%3)", LF_Utils.GetPlayerUID(user), plantId, row.m_sCropId));
		return LF_EPlantResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character takes a ripe crop, whoever planted it; the plant is gone. A dead or rotten
	//! plant is cleared with nothing given (CLEARED), so it stops counting against the caps. A full
	//! inventory drops the rest at the feet.
	static LF_EPlantResult Harvest(IEntity user, string plantId)
	{
		LF_PlantRow row;
		LF_CropRow crop;
		LF_EPlantResult reach = Reach(user, plantId, "harvesting", row, crop);
		if (reach != LF_EPlantResult.OK)
			return reach;

		LF_PlantState state = StateOf(row, crop);
		if (state.m_eStatus == LF_EPlantStatus.DEAD || state.m_eStatus == LF_EPlantStatus.ROTTEN)
		{
			Remove(plantId);
			LF_Debug.Info(FEATURE, string.Format("%1 cleared %2, a %3 %4", LF_Utils.GetPlayerUID(user), plantId, typename.EnumToString(LF_EPlantStatus, state.m_eStatus), row.m_sCropId));
			return LF_EPlantResult.CLEARED;
		}

		if (state.m_eStatus != LF_EPlantStatus.RIPE)
			return LF_EPlantResult.NOT_RIPE;

		int added = LF_InventoryUtils.AddAmount(user, crop.m_HarvestPrefab, crop.m_iYield, true);
		if (added <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 harvest grant failed: %2 x%3, the crop stays", plantId, crop.m_HarvestPrefab, crop.m_iYield));
			return LF_EPlantResult.GRANT_FAILED;
		}

		string ownerKey = row.m_sOwnerKey;
		Remove(plantId);
		LF_Debug.Info(FEATURE, string.Format("%1 harvested %2 (%3, planted by %4): %5 x%6", LF_Utils.GetPlayerUID(user), plantId, crop.m_sCropId, ownerKey, crop.m_HarvestPrefab, added));
		LF_JobManager jobManager = LF_JobManager.GetInstance();
		if (jobManager)
			jobManager.OnGatherCompleted(user, crop.m_HarvestPrefab);

		return LF_EPlantResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The plant in words for the inspect prompt: a stage key and a water key, no numbers.
	static bool Describe(string plantId, out string stageKey, out string waterKey)
	{
		LF_PlantRow row = Plants().Get(plantId);
		if (!row)
			return false;

		LF_CropRow crop = FindCrop(row.m_sCropId);
		if (!crop)
			return false;

		LF_PlantState state = StateOf(row, crop);
		waterKey = "";
		switch (state.m_eStatus)
		{
			case LF_EPlantStatus.RIPE: stageKey = "#LF-Plant_Stage_Ripe"; return true;
			case LF_EPlantStatus.DEAD: stageKey = "#LF-Plant_Stage_Dead"; return true;
			case LF_EPlantStatus.ROTTEN: stageKey = "#LF-Plant_Stage_Rotten"; return true;
		}

		stageKey = "#LF-Plant_Stage_Sprout";
		if (state.m_fGrownSeconds * 2 >= crop.m_iGrowthSeconds)
			stageKey = "#LF-Plant_Stage_Growing";

		waterKey = "#LF-Plant_Water_Fine";
		if (LF_PlantRules.NeedsWater(crop) && state.m_fSinceWaterSeconds >= crop.m_iWaterMinSeconds)
			waterKey = "#LF-Plant_Water_Thirsty";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The checks water and harvest share: server, system, caller, plant, crop, reach.
	protected static LF_EPlantResult Reach(IEntity user, string plantId, string what, out LF_PlantRow row, out LF_CropRow crop)
	{
		if (!Replication.IsServer())
			return LF_EPlantResult.NOT_SERVER;

		if (LF_Systems.Refuses(LF_FarmPlots.SYSTEM, what + " a plant"))
			return LF_EPlantResult.SYSTEM_OFF;

		if (!user)
		{
			LF_Debug.Error(FEATURE, what + " refused: no character behind the request");
			return LF_EPlantResult.BAD_CALLER;
		}

		row = Plants().Get(plantId);
		if (!row)
			return LF_EPlantResult.NO_PLANT;

		crop = FindCrop(row.m_sCropId);
		if (!crop)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 grows %2, which the catalogue no longer has", plantId, row.m_sCropId));
			return LF_EPlantResult.NO_CROP;
		}

		if (vector.Distance(user.GetOrigin(), row.m_vPosition) > REACH)
			return LF_EPlantResult.TOO_FAR;

		return LF_EPlantResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove(string plantId)
	{
		LF_PlantReplication.Retract(plantId);
		ClearVisual(plantId);
		Plants().Remove(plantId);
		if (Plants().IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Is spot (already on the terrain) open ground for this planter (#2821): the planter stands on the terrain
	//! and not on a floor, nothing built stands over the spot, it is not a road and not below sea level.
	//! Reads the world with a trace from above the spot; LF_PlantRules.OpenGround decides.
	static LF_EPlantPlace OpenGroundAt(IEntity user, vector spot)
	{
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld() || !user)
			return LF_EPlantPlace.OK;

		BaseWorld world = game.GetWorld();
		vector feet = user.GetOrigin();
		float standHeight = feet[1] - world.GetSurfaceY(feet[0], feet[2]);

		array<IEntity> skip = {user};
		bool covered;
		string material;
		for (int attempt = 0; attempt < 4; attempt++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = spot + Vector(0, OPEN_TRACE_UP_M, 0);
			trace.End = spot - Vector(0, 0.2, 0);
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			trace.LayerMask = EPhysicsLayerPresets.Projectile;
			trace.ExcludeArray = skip;
			float fraction = world.TraceMove(trace, null);
			if (fraction >= 1)
				break;

			if (trace.TraceEnt)
			{
				if (ChimeraCharacter.Cast(trace.TraceEnt))
				{
					// A person standing over the spot is not a roof.
					skip.Insert(trace.TraceEnt);
					continue;
				}

				// The ground itself can be an entity (a road mesh, DebugWorld's slab): only a hit above the terrain is cover.
				if (trace.ContactPoint[1] > spot[1] + OPEN_COVER_MIN_M)
					covered = true;
			}

			material = trace.TraceMaterial;
			break;
		}

		LF_EPlantPlace verdict = LF_PlantRules.OpenGround(standHeight, spot[1], world.GetOceanBaseHeight(), covered, material);
		if (verdict != LF_EPlantPlace.OK)
			LF_Debug.Info(FEATURE, string.Format("not open ground at %1: stand=%2 covered=%3 material=%4", spot, standHeight, covered, material));

		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	//! The spot moved onto the terrain under it, so a ground change never floats a plant.
	static vector SnapToGround(vector spot)
	{
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
			return spot;

		spot[1] = game.GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	//! Local mesh copies this machine draws right now.
	static int VisualCount()
	{
		return Visuals().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the plant's stage prefab on this machine, replaced when the stage changes. A dedicated
	//! server and a crop with no stage prefabs draw nothing.
	protected static void RefreshVisual(LF_PlantRow row)
	{
		// Clients on a dedicated server draw from the carrier table (#2819), told only when the stage changes.
		LF_PlantReplication.Publish(row);
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		LF_CropRow crop = FindCrop(row.m_sCropId);
		if (!crop || !crop.m_aStagePrefabs || crop.m_aStagePrefabs.IsEmpty())
			return;

		int stage = LF_PlantRules.StageOf(crop, StateOf(row, crop));
		if (stage < 0)
			return;

		if (stage >= crop.m_aStagePrefabs.Count())
			stage = crop.m_aStagePrefabs.Count() - 1;

		int shown;
		if (ShownStages().Find(row.m_sPlantId, shown) && shown == stage)
			return;

		ClearVisual(row.m_sPlantId);
		ShownStages().Set(row.m_sPlantId, stage);
		Resource resource = Resource.Load(crop.m_aStagePrefabs[stage]);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(FEATURE, string.Format("%1: stage prefab does not load: %2", row.m_sPlantId, crop.m_aStagePrefabs[stage]));
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = row.m_vPosition;
		IEntity visual = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
		if (!visual)
		{
			LF_Debug.Error(FEATURE, string.Format("%1: stage prefab failed to spawn", row.m_sPlantId));
			return;
		}

		Visuals().Set(row.m_sPlantId, visual.GetID());
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearVisual(string plantId)
	{
		if (s_mShownStage)
			s_mShownStage.Remove(plantId);

		if (!s_mVisuals)
			return;

		EntityID id;
		if (!s_mVisuals.Find(plantId, id))
			return;

		s_mVisuals.Remove(plantId);
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
			return;

		IEntity visual = game.GetWorld().FindEntityByID(id);
		if (visual)
			SCR_EntityHelper.DeleteEntityAndChildren(visual);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearAllVisuals()
	{
		if (!s_mVisuals)
			return;

		array<string> ids = new array<string>();
		foreach (string id, EntityID entityId : s_mVisuals)
			ids.Insert(id);

		foreach (string id : ids)
			ClearVisual(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every plant, clocks relative to now (zero or less).
	static array<ref LF_PlantRow> ExportAll()
	{
		array<ref LF_PlantRow> result = new array<ref LF_PlantRow>();
		int now = Now();
		foreach (string id, LF_PlantRow row : Plants())
		{
			LF_PlantRow copy = row.Copy();
			copy.m_iPlantedAt = row.m_iPlantedAt - now;
			copy.m_iWateredAt = row.m_iWateredAt - now;
			result.Insert(copy);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved plants replace the live ones, set on the ground under them.
	//! Rows with no id or an unknown crop are dropped and logged. With no crop catalogue loaded every row
	//! is kept unchecked (as reputation keeps its rows with no config), so a broken config never wipes
	//! the plants from the next save. Idempotent: a second apply converges.
	//! \return How many plants were kept.
	static int ApplyAll(array<ref LF_PlantRow> records)
	{
		LF_PlantReplication.RetractAll();
		ClearAllVisuals();
		s_mPlants = new map<string, ref LF_PlantRow>();
		int now = Now();
		bool haveCatalogue = Catalogue() != null;
		if (!haveCatalogue && records && !records.IsEmpty())
			LF_Debug.Error(FEATURE, string.Format("load: no crop catalogue, keeping %1 plant row(s) unchecked", records.Count()));

		if (records)
		{
			foreach (LF_PlantRow record : records)
			{
				if (!record || record.m_sPlantId.IsEmpty() || Plants().Contains(record.m_sPlantId))
				{
					LF_Debug.Warn(FEATURE, "load skipped a plant row with no id or a repeated id");
					continue;
				}

				if (haveCatalogue && !FindCrop(record.m_sCropId))
				{
					LF_Debug.Warn(FEATURE, string.Format("load dropped plant %1: crop %2 is not in the catalogue", record.m_sPlantId, record.m_sCropId));
					continue;
				}

				LF_PlantRow row = record.Copy();
				row.m_iPlantedAt = record.m_iPlantedAt + now;
				row.m_iWateredAt = record.m_iWateredAt + now;
				row.m_vPosition = SnapToGround(record.m_vPosition);
				Plants().Set(row.m_sPlantId, row);
				RefreshVisual(row);
			}
		}

		if (Plants().IsEmpty())
			StopTicking();
		else if (Replication.IsServer())
			StartTicking();

		return Plants().Count();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Plants's statics (LF_StaticReset finds it by reflection).
class LF_Plants_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Plants";
	}

	override void Apply()
	{
		LF_Plants.ResetStatics();
	}
}
