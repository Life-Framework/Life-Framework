//------------------------------------------------------------------------------------------------
enum LF_EPlantActionMode
{
	PLANT,
	WATER,
	HARVEST,
	INSPECT
}

//------------------------------------------------------------------------------------------------
//! What a gardener does with free crops (#2664), one class with a mode. The plants themselves are
//! rows on the server with local meshes, so the action sits on a replicated item the gardener holds
//! (PLANT on a seed, WATER on a water container) or on the character (HARVEST, INSPECT) and acts on
//! the spot in front of the user or the nearest plant within reach. The server checks everything.
class LF_PlantAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does", "", ParamEnumArray.FromEnum(LF_EPlantActionMode))]
	protected LF_EPlantActionMode m_eMode;

	[Attribute("1.2", UIWidgets.Auto, "PLANT: metres in front of the user the seed goes in")]
	protected float m_fPlantAhead;

	protected IEntity m_Owner;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Owner = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	LF_EPlantActionMode GetMode()
	{
		return m_eMode;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user && LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		switch (m_eMode)
		{
			case LF_EPlantActionMode.PLANT: outName = "#LF-Plant_Action_Plant"; return true;
			case LF_EPlantActionMode.WATER: outName = "#LF-Plant_Action_Water"; return true;
			case LF_EPlantActionMode.HARVEST: outName = "#LF-Plant_Action_Harvest"; return true;
		}

		outName = "#LF-Plant_Action_Inspect";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a seed goes: on the ground a little in front of the user.
	static vector SpotAhead(IEntity user, float ahead)
	{
		vector transform[4];
		user.GetWorldTransform(transform);
		return LF_Plants.SnapToGround(transform[3] + transform[2] * ahead);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity || !Replication.IsServer())
			return;

		LF_EPlantResult result;
		LF_EPlantPlace place = LF_EPlantPlace.OK;
		string plantId;
		if (m_eMode == LF_EPlantActionMode.PLANT)
		{
			result = LF_Plants.Plant(pUserEntity, LF_Utils.GetPrefabName(pOwnerEntity), SpotAhead(pUserEntity, m_fPlantAhead), plantId, place);
		}
		else
		{
			plantId = LF_Plants.FindNearest(pUserEntity.GetOrigin(), LF_Plants.REACH);
			if (m_eMode == LF_EPlantActionMode.WATER)
				result = LF_Plants.Water(pUserEntity, plantId);
			else if (m_eMode == LF_EPlantActionMode.HARVEST)
				result = LF_Plants.Harvest(pUserEntity, plantId);
			else
				result = Inspect(pUserEntity, plantId);
		}

		string key = MessageKey(result, place);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Plant_Title", 4.0);

		if (result != LF_EPlantResult.OK && result != LF_EPlantResult.CLEARED)
			LF_Debug.Info(LF_Plants.FEATURE, string.Format("%1 %2 refused (%3)", typename.EnumToString(LF_EPlantActionMode, m_eMode), plantId, typename.EnumToString(LF_EPlantResult, result)));
	}

	//------------------------------------------------------------------------------------------------
	//! INSPECT: the plant's stage and water in words, as a toast.
	protected static LF_EPlantResult Inspect(IEntity user, string plantId)
	{
		LF_PlantRow row = LF_Plants.Find(plantId);
		if (!row)
			return LF_EPlantResult.NO_PLANT;

		string stageKey;
		string waterKey;
		if (!LF_Plants.Describe(plantId, stageKey, waterKey))
			return LF_EPlantResult.NO_CROP;

		LF_Utils.NotifyEntity(user, stageKey, "#LF-Plant_Title", 4.0);
		if (!waterKey.IsEmpty())
			LF_Utils.NotifyEntity(user, waterKey, "#LF-Plant_Title", 4.0);

		return LF_EPlantResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast for a result; empty when INSPECT already spoke.
	static string MessageKey(LF_EPlantResult result, LF_EPlantPlace place)
	{
		switch (result)
		{
			case LF_EPlantResult.PLACE_REFUSED: return LF_PlantRules.PlaceRefusalKey(place);
			case LF_EPlantResult.NO_SEED: return "#LF-Plant_NoSeed";
			case LF_EPlantResult.NO_CROP: return "#LF-Plant_NoCrop";
			case LF_EPlantResult.NO_PLANT: return "#LF-Plant_NoPlant";
			case LF_EPlantResult.TOO_FAR: return "#LF-Plant_TooFar";
			case LF_EPlantResult.NO_WATER: return "#LF-Plant_NoWater";
			case LF_EPlantResult.TOO_EARLY: return "#LF-Plant_NotThirsty";
			case LF_EPlantResult.NOT_GROWING: return "#LF-Plant_NotGrowing";
			case LF_EPlantResult.NOT_RIPE: return "#LF-Plant_NotRipe";
			case LF_EPlantResult.CLEARED: return "#LF-Plant_Cleared";
			case LF_EPlantResult.GRANT_FAILED: return "#LF-Plant_GrantFailed";
		}

		return "";
	}
}
