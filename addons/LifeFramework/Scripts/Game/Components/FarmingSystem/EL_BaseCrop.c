[EntityEditorProps(category: "EveronLife/Crop", description: "Basic staged crop entity")]
class EL_BaseCropClass : EL_BaseBuildingClass
{
}

//------------------------------------------------------------------------------------------------
class EL_StagePhaseTitle : BaseContainerCustomTitle
{
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)
	{
		float time = 0;
		string name = "";
		source.Get("m_fStageTime", time);
		source.Get("m_sStageName", name);
		title = string.Format("%1 Stage | %2min", name, time);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), EL_StagePhaseTitle()]
class EL_CropStage
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Model to use for the stage. Leave empty to use current model", "xob")]
	ResourceName m_NewStageModel;

	[Attribute("1", UIWidgets.Range, "How long this stage lasts (m)")]
	float m_fStageTime;

	[Attribute("", UIWidgets.Auto, "Name of this stage")]
	string m_sStageName;

	[Attribute("0 0 0", UIWidgets.Auto, "Total offset in this stage")]
	vector m_StageOffset;

	[Attribute("1", UIWidgets.Auto, "Scale change in this stage")]
	float m_fStageScale;

	[Attribute("0", UIWidgets.Auto, "Can crop be gathered in this stage?")]
	bool m_bCanGather;

	//------------------------------------------------------------------------------------------------
	//! Stage time is authored in minutes; progression compares against world time in ms
	float GetDurationMs()
	{
		return m_fStageTime * 60000;
	}
}

//------------------------------------------------------------------------------------------------
class EL_BaseCrop : EL_BaseBuilding
{
	[Attribute("-1", UIWidgets.EditBox, "Spawn items at this stage with area spawner. -1 to disable", category: "Crop")]
	protected int m_iSpawnItemsAtStage;

	[Attribute("0", UIWidgets.CheckBox, "Delete the crop after final stage?", category: "Crop")]
	protected bool m_bDeleteAfterFinalStage;

	[Attribute("", UIWidgets.Object, "List of stages", category: "Crop")]
	protected ref array<ref EL_CropStage> m_aCropStages;

	[RplProp(onRplName: "OnCropStageChanged")]
	protected int m_iCropStage = -1;

	protected bool m_IsInit = false;
	private float m_fStageStartTime = 0;
	private vector m_vStartPos;

	//------------------------------------------------------------------------------------------------
	//! Change model, size etc..
	protected void OnCropStageChanged()
	{
		Print(string.Format("[EL_BaseCrop] New stage %1 (%2)", m_iCropStage, this), LogLevel.NORMAL);

		// A base crop without authored stages never leaves stage 0
		if (!m_aCropStages)
			return;

		// Check if end of last stage
		if (IsFinalStageReached(m_iCropStage, m_aCropStages.Count()))
		{
			RplComponent rplComponent = RplComponent.Cast(FindComponent(RplComponent));
			if (m_bDeleteAfterFinalStage && rplComponent && rplComponent.IsMaster())
				rplComponent.DeleteRplEntity(this, false);
			return;
		}

		// New stage
		EL_CropStage stage = m_aCropStages[m_iCropStage];
		SetOrigin(m_vStartPos + stage.m_StageOffset);
		SetScale(stage.m_fStageScale);

		// Set new model
		if (stage.m_NewStageModel)
			SetObject(Resource.Load(stage.m_NewStageModel).GetResource().ToVObject(), "");

		// Start area spawner on the replication master
		if (m_iSpawnItemsAtStage == m_iCropStage)
		{
			RplComponent rplComponent = RplComponent.Cast(FindComponent(RplComponent));
			EL_AreaSpawnerComponent areaSpawner = EL_AreaSpawnerComponent.Cast(FindComponent(EL_AreaSpawnerComponent));
			if (areaSpawner && rplComponent && rplComponent.IsMaster())
				areaSpawner.SpawnPrefabs(this);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when the current stage is past the last one, so the crop idles or deletes
	static bool IsFinalStageReached(int stageIndex, int stageCount)
	{
		return stageIndex >= stageCount;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the elapsed world time since the stage start has crossed the stage duration
	static bool IsStageDue(float stageStartMs, float stageDurationMs, float nowMs)
	{
		return nowMs >= stageStartMs + stageDurationMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage can be gathered only at a valid, gatherable stage; before init (-1) and
	//! past the final stage it cannot
	static bool CanGatherAtStage(array<ref EL_CropStage> stages, int stageIndex)
	{
		if (!stages || stageIndex < 0 || IsFinalStageReached(stageIndex, stages.Count()))
			return false;

		return stages[stageIndex].m_bCanGather;
	}

	//------------------------------------------------------------------------------------------------
	bool CanGather()
	{
		return CanGatherAtStage(m_aCropStages, m_iCropStage);
	}

	//------------------------------------------------------------------------------------------------
	int GetStageIndex()
	{
		return m_iCropStage;
	}

	//------------------------------------------------------------------------------------------------
	array<ref EL_CropStage> GetCropStages()
	{
		return m_aCropStages;
	}

	//------------------------------------------------------------------------------------------------
	int GetSpawnItemsAtStage()
	{
		return m_iSpawnItemsAtStage;
	}

	//------------------------------------------------------------------------------------------------
	bool GetDeleteAfterFinalStage()
	{
		return m_bDeleteAfterFinalStage;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitCrop(IEntity owner)
	{
		RplComponent rplComponent = RplComponent.Cast(FindComponent(RplComponent));
		m_IsInit = true;

		// Clear frame for anything but the replication owner
		if (!rplComponent.IsOwner())
		{
			ClearEventMask(EntityEvent.FRAME);
			ClearFlags(EntityFlags.ACTIVE, true);
			return;
		}

		// Set to stage 0
		m_iCropStage++;
		Replication.BumpMe();

		m_fStageStartTime = GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_IsInit)
		{
			InitCrop(owner);
			return;
		}

		// A base crop without authored stages idles at stage 0
		if (!m_aCropStages)
			return;

		if (IsFinalStageReached(m_iCropStage, m_aCropStages.Count()))
			return;

		EL_CropStage stage = m_aCropStages[m_iCropStage];
		if (IsStageDue(m_fStageStartTime, stage.GetDurationMs(), GetGame().GetWorld().GetWorldTime()))
		{
			m_iCropStage++;
			Replication.BumpMe();

			m_fStageStartTime = GetGame().GetWorld().GetWorldTime();
		}
	}

	//------------------------------------------------------------------------------------------------
	void EL_BaseCrop(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
		SetFlags(EntityFlags.ACTIVE, true);
		m_vStartPos = GetOrigin();
	}
}