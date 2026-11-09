[EntityEditorProps(category: "EveronLife/Crop", description: "Basic staged crop entity")]
class LF_BaseCropClass : LF_BaseBuildingClass
{
}

//------------------------------------------------------------------------------------------------
class LF_StagePhaseTitle : BaseContainerCustomTitle
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
[BaseContainerProps(), LF_StagePhaseTitle()]
class LF_CropStage
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
class LF_BaseCrop : LF_BaseBuilding
{
	[Attribute("-1", UIWidgets.EditBox, "Spawn items at this stage with area spawner. -1 to disable", category: "Crop")]
	protected int m_iSpawnItemsAtStage;

	[Attribute("0", UIWidgets.CheckBox, "Delete the crop after final stage?", category: "Crop")]
	protected bool m_bDeleteAfterFinalStage;

	[Attribute("", UIWidgets.Object, "List of stages", category: "Crop")]
	protected ref array<ref LF_CropStage> m_aCropStages;

	[RplProp(onRplName: "OnCropStageChanged")]
	protected int m_iCropStage = -1;

	protected bool m_IsInit = false;
	protected bool m_bRunsGrowth = false;
	private float m_fStageStartTime = 0;
	private vector m_vStartPos;

	//------------------------------------------------------------------------------------------------
	//! Change model, size etc..
	protected void OnCropStageChanged()
	{
		LF_Debug.Log("Farming", string.Format("crop stage -> %1", m_iCropStage));

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

		// New stage. A client can hear about the crop before its first stage (-1).
		LF_CropStage stage = StageAt(m_aCropStages, m_iCropStage);
		if (!stage)
			return;

		SetOrigin(m_vStartPos + stage.m_StageOffset);
		SetScale(stage.m_fStageScale);

		// Set new model
		if (stage.m_NewStageModel)
		{
			Resource modelResource = Resource.Load(stage.m_NewStageModel);
			if (modelResource && modelResource.IsValid())
				SetObject(modelResource.GetResource().ToVObject(), "");
			else
				LF_Debug.Error("Farming", string.Format("crop stage model does not resolve: %1", stage.m_NewStageModel));
		}

		// Start area spawner on the replication master
		if (m_iSpawnItemsAtStage == m_iCropStage)
		{
			RplComponent rplComponent = RplComponent.Cast(FindComponent(RplComponent));
			LF_AreaSpawnerComponent areaSpawner = LF_AreaSpawnerComponent.Cast(FindComponent(LF_AreaSpawnerComponent));
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
	//! The authored stage at stageIndex, or null when the index is outside the list or the
	//! entry is empty. Every stage read goes through here, so a bad index never VMEs.
	static LF_CropStage StageAt(array<ref LF_CropStage> stages, int stageIndex)
	{
		if (!stages || stageIndex < 0 || stageIndex >= stages.Count())
			return null;

		return stages[stageIndex];
	}

	//------------------------------------------------------------------------------------------------
	//! Growth runs on the server only. A client only shows the replicated stage.
	static bool RunsGrowth(bool isServer, bool hasRplComponent)
	{
		return isServer && hasRplComponent;
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
	static bool CanGatherAtStage(array<ref LF_CropStage> stages, int stageIndex)
	{
		LF_CropStage stage = StageAt(stages, stageIndex);
		if (!stage)
			return false;

		return stage.m_bCanGather;
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
	array<ref LF_CropStage> GetCropStages()
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

		// Growth runs on the server only. RplComponent.IsOwner() is still true on a client while
		// the world loads, so the client used to start its own growth clock, then the snapshot set
		// the stage back to the server's value and EOnFrame indexed it (VME on every client, #157).
		// RplSession.Mode() is Client on a game client from the start of the load.
		bool server = Replication.IsServer() && RplSession.Mode() != RplMode.Client;
		m_bRunsGrowth = RunsGrowth(server, rplComponent != null);
		LF_Debug.Log("Farming", string.Format("crop %1 init: grows here %2 (rpl mode %3)", GetName(), m_bRunsGrowth, typename.EnumToString(RplMode, RplSession.Mode())));
		if (!m_bRunsGrowth)
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

		// A client only shows the replicated stage (OnCropStageChanged).
		if (!m_bRunsGrowth)
			return;

		// A base crop without authored stages idles at stage 0
		if (!m_aCropStages)
			return;

		if (IsFinalStageReached(m_iCropStage, m_aCropStages.Count()))
			return;

		LF_CropStage stage = StageAt(m_aCropStages, m_iCropStage);
		if (!stage)
		{
			LF_Debug.Error("Farming", string.Format("crop %1 has no stage %2; growth stopped", GetName(), m_iCropStage));
			m_bRunsGrowth = false;
			ClearEventMask(EntityEvent.FRAME);
			return;
		}

		if (IsStageDue(m_fStageStartTime, stage.GetDurationMs(), GetGame().GetWorld().GetWorldTime()))
		{
			m_iCropStage++;
			Replication.BumpMe();

			m_fStageStartTime = GetGame().GetWorld().GetWorldTime();
		}
	}

	//------------------------------------------------------------------------------------------------
	void LF_BaseCrop(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
		SetFlags(EntityFlags.ACTIVE, true);
		m_vStartPos = GetOrigin();
	}
}