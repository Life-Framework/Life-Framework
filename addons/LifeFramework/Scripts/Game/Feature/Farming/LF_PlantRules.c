//------------------------------------------------------------------------------------------------
//! Why a seed may not be planted here (#2663). OK plants.
enum LF_EPlantPlace
{
	OK,
	TOO_CLOSE,
	TOO_FAR,
	IN_VEHICLE,
	SAFE_ZONE,
	CHARACTER_CAP,
	SERVER_CAP
}

//------------------------------------------------------------------------------------------------
//! What a watering did.
enum LF_EPlantWater
{
	OK,
	TOO_EARLY,
	NOT_GROWING
}

//------------------------------------------------------------------------------------------------
//! Where a planted crop is in its life. DEAD and ROTTEN are final.
enum LF_EPlantStatus
{
	GROWING,
	RIPE,
	DEAD,
	ROTTEN
}

//------------------------------------------------------------------------------------------------
//! One planted crop's clocks, in seconds. Pure data; LF_PlantRules advances it. The clocks and the
//! status are all a save needs to keep (plus the crop id), and none holds an entity.
class LF_PlantState
{
	float m_fGrownSeconds;
	float m_fSinceWaterSeconds;
	float m_fRipeSeconds;
	LF_EPlantStatus m_eStatus = LF_EPlantStatus.GROWING;
}

//------------------------------------------------------------------------------------------------
//! The planting and growing rules of a planted crop (#2663, spec #809): pure logic, no entity, no
//! world, no clock of its own. The caller feeds it elapsed seconds and the facts about the planter.
//! Stage maths reuse LF_FarmPlotRules, so a catalogue crop grows like a farm plot crop.
class LF_PlantRules
{
	//! Nearest another plant may stand, metres.
	static const float MIN_SPACING = 1.5;	// lf-fixed: the planting rule of spec #809
	//! Farthest from the planter the spot may be, metres.
	static const float MAX_REACH = 4.0;	// lf-fixed: the planting rule of spec #809
	//! Plants one character may have standing. Decided on #2663.
	static const int MAX_PER_CHARACTER = 12;	// lf-fixed: decided cap, #2663
	//! Plants standing on the server. Decided on #2663.
	static const int MAX_ON_SERVER = 300;	// lf-fixed: decided cap, #2663

	//------------------------------------------------------------------------------------------------
	//! May this seed go in the ground here. The first failing rule wins, in the order a player
	//! would fix them: where they stand, then the spot, then the counts.
	//! \param nearestPlantDistance metres to the closest other plant; negative when there is none
	//! \param reachDistance metres from the planter to the spot
	static LF_EPlantPlace CanPlace(float nearestPlantDistance, float reachDistance, bool inVehicle, bool inSafeZone, int plantedByCharacter, int plantedOnServer)
	{
		if (inVehicle)
			return LF_EPlantPlace.IN_VEHICLE;

		if (inSafeZone)
			return LF_EPlantPlace.SAFE_ZONE;

		if (reachDistance > MAX_REACH)
			return LF_EPlantPlace.TOO_FAR;

		if (nearestPlantDistance >= 0 && nearestPlantDistance < MIN_SPACING)
			return LF_EPlantPlace.TOO_CLOSE;

		if (plantedByCharacter >= MAX_PER_CHARACTER)
			return LF_EPlantPlace.CHARACTER_CAP;

		if (plantedOnServer >= MAX_ON_SERVER)
			return LF_EPlantPlace.SERVER_CAP;

		return LF_EPlantPlace.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Localization key for a planting refusal, empty for OK.
	static string PlaceRefusalKey(LF_EPlantPlace result)
	{
		switch (result)
		{
			case LF_EPlantPlace.TOO_CLOSE: return "#LF-Plant_TooClose";
			case LF_EPlantPlace.TOO_FAR: return "#LF-Plant_TooFar";
			case LF_EPlantPlace.IN_VEHICLE: return "#LF-Plant_InVehicle";
			case LF_EPlantPlace.SAFE_ZONE: return "#LF-Plant_SafeZone";
			case LF_EPlantPlace.CHARACTER_CAP: return "#LF-Plant_CharacterCap";
			case LF_EPlantPlace.SERVER_CAP: return "#LF-Plant_ServerCap";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The row's growth as a stage list for LF_FarmPlotRules: every stage but the last lasts an equal
	//! share of m_iGrowthSeconds, the last is the ripe crop and holds. Fewer than two stage prefabs
	//! still grow through a growing and a ripe stage.
	static array<ref LF_CropStage> BuildStages(LF_CropRow row)
	{
		array<ref LF_CropStage> stages = new array<ref LF_CropStage>();
		if (!row)
			return stages;

		int count = 2;
		if (row.m_aStagePrefabs && row.m_aStagePrefabs.Count() > count)
			count = row.m_aStagePrefabs.Count();

		float minutesEach = row.m_iGrowthSeconds / 60.0 / (count - 1);
		for (int index = 0; index < count; index++)
		{
			LF_CropStage stage = new LF_CropStage();
			stage.m_fStageTime = minutesEach;
			stage.m_bCanGather = (index == count - 1);
			stages.Insert(stage);
		}

		return stages;
	}

	//------------------------------------------------------------------------------------------------
	//! The stage a crop shows after grownSeconds of growth: an index into BuildStages, or
	//! LF_FarmPlotRules.EMPTY for a null row.
	static int StageForSeconds(LF_CropRow row, float grownSeconds)
	{
		if (!row)
			return LF_FarmPlotRules.EMPTY;

		return LF_FarmPlotRules.StageForSeconds(BuildStages(row), grownSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Does this crop need watering at all.
	static bool NeedsWater(LF_CropRow row)
	{
		return row && row.m_iWaterMaxSeconds > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the plant forward by dt seconds. Growing: both the growth and the water clock run, and a
	//! water clock past m_iWaterMaxSeconds kills the plant. Ripe: only the ripe clock runs, and it rots at
	//! m_iLifetimeAfterRipe. DEAD and ROTTEN do not change. Safe for any dt, a long gap included.
	static void Advance(LF_CropRow row, LF_PlantState state, float dt)
	{
		if (!row || !state || dt <= 0)
			return;

		if (state.m_eStatus == LF_EPlantStatus.GROWING)
		{
			float toRipe = row.m_iGrowthSeconds - state.m_fGrownSeconds;
			float step = dt;
			if (step > toRipe)
				step = toRipe;

			if (NeedsWater(row) && state.m_fSinceWaterSeconds + step > row.m_iWaterMaxSeconds)
			{
				// The window closed before the crop ripened.
				float alive = row.m_iWaterMaxSeconds - state.m_fSinceWaterSeconds;
				if (alive < 0)
					alive = 0;

				state.m_fGrownSeconds += alive;
				state.m_fSinceWaterSeconds = row.m_iWaterMaxSeconds + 1;
				state.m_eStatus = LF_EPlantStatus.DEAD;
				return;
			}

			state.m_fGrownSeconds += step;
			state.m_fSinceWaterSeconds += step;
			dt -= step;
			if (state.m_fGrownSeconds >= row.m_iGrowthSeconds)
				state.m_eStatus = LF_EPlantStatus.RIPE;
		}

		if (state.m_eStatus == LF_EPlantStatus.RIPE && dt > 0)
		{
			state.m_fRipeSeconds += dt;
			if (row.m_iLifetimeAfterRipe > 0 && state.m_fRipeSeconds >= row.m_iLifetimeAfterRipe)
				state.m_eStatus = LF_EPlantStatus.ROTTEN;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Waters the plant. Inside the window (m_iWaterMinSeconds to m_iWaterMaxSeconds since the last
	//! watering) the water clock resets; before it the plant is not thirsty yet and nothing changes.
	//! A ripe, dead or rotten plant takes no water.
	static LF_EPlantWater Water(LF_CropRow row, LF_PlantState state)
	{
		if (!row || !state || state.m_eStatus != LF_EPlantStatus.GROWING)
			return LF_EPlantWater.NOT_GROWING;

		if (state.m_fSinceWaterSeconds < row.m_iWaterMinSeconds)
			return LF_EPlantWater.TOO_EARLY;

		state.m_fSinceWaterSeconds = 0;
		return LF_EPlantWater.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The stage index a plant shows: its growth stage while growing or dead, the last once ripe or
	//! rotten. A dead or rotten plant keeps its look; the caller picks any wilted visual.
	static int StageOf(LF_CropRow row, LF_PlantState state)
	{
		if (!row || !state)
			return LF_FarmPlotRules.EMPTY;

		if (state.m_eStatus == LF_EPlantStatus.RIPE || state.m_eStatus == LF_EPlantStatus.ROTTEN)
			return BuildStages(row).Count() - 1;

		return StageForSeconds(row, state.m_fGrownSeconds);
	}
}
