//------------------------------------------------------------------------------------------------
//! Why a plot refused a farmer. OK is the only result that changed the plot.
enum LF_EFarmPlot
{
	OK,
	NO_PLOT,
	NOT_EMPTY,
	NOT_RIPE,
	NO_SEEDS,
	TOO_FAR,
	GRANT_FAILED,
	SYSTEM_OFF	//!< The Farming system is off on this server (#960).
}

//------------------------------------------------------------------------------------------------
//! One saved crop. Only planted plots are saved, so a row always means something grows there.
class LF_FarmPlotRow : Managed
{
	string m_sPlotKey;
	string m_sCropId;
	int m_iGrownSeconds;

	//------------------------------------------------------------------------------------------------
	LF_FarmPlotRow Copy()
	{
		LF_FarmPlotRow copy = new LF_FarmPlotRow();
		copy.m_sPlotKey = m_sPlotKey;
		copy.m_sCropId = m_sCropId;
		copy.m_iGrownSeconds = m_iGrownSeconds;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Pure growth rules of a plot, shared by the server, the action prompt and the tests.
class LF_FarmPlotRules
{
	static const int EMPTY = -1;

	//------------------------------------------------------------------------------------------------
	//! The stage a crop has reached after grownSeconds of growth. Every stage but the last lasts its
	//! m_fStageTime minutes; the last stage holds, so a ripe crop waits for the farmer.
	//! \return EMPTY when there are no stages, else an index into stages.
	static int StageForSeconds(array<ref LF_CropStage> stages, float grownSeconds)
	{
		if (!stages || stages.IsEmpty())
			return EMPTY;

		int last = stages.Count() - 1;
		float stageEnd = 0;
		for (int index = 0; index < last; index++)
		{
			LF_CropStage stage = stages[index];
			if (stage)
				stageEnd += stage.GetDurationMs() / 1000;

			if (grownSeconds < stageEnd)
				return index;
		}

		return last;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds of growth from planting to the last stage.
	static float SecondsToLastStage(array<ref LF_CropStage> stages)
	{
		if (!stages)
			return 0;

		float total = 0;
		for (int index = 0; index < stages.Count() - 1; index++)
		{
			if (stages[index])
				total += stages[index].GetDurationMs() / 1000;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! A plot can be harvested only at a stage authored as gatherable (the crop's own rule).
	static bool IsRipe(array<ref LF_CropStage> stages, int stageIndex)
	{
		return LF_BaseCrop.CanGatherAtStage(stages, stageIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Localization key for a refusal, empty for results the farmer needs no text for.
	static string RefusalKey(LF_EFarmPlot result)
	{
		switch (result)
		{
			case LF_EFarmPlot.NOT_EMPTY: return "#LF-Farm_NotEmpty";
			case LF_EFarmPlot.NOT_RIPE: return "#LF-Farm_NotRipe";
			case LF_EFarmPlot.NO_SEEDS: return "#LF-Farm_NeedSeeds";
			case LF_EFarmPlot.TOO_FAR: return "#LF-Farm_TooFar";
			case LF_EFarmPlot.GRANT_FAILED: return "#LF-Farm_PocketsFull";
			case LF_EFarmPlot.SYSTEM_OFF: return "#LF-Systems_Off";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Farming", description: "A dirt plot a farmer plants with a seed packet, watches grow and harvests. Actions: LF_FarmPlotAction.")]
class LF_FarmPlotComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A farm plot (#614): plant a seed packet, the crop grows through visible stages on the server's
//! clock, harvest it when ripe, and the plot is empty again. The server decides everything; the
//! replicated stage index is all a client needs to draw the crop and word the prompt.
//!
//! The crop is drawn by local, physics-free copies of m_CropVisualPrefab on m_sSlots, scaled per
//! stage (LF_CropStage.m_fStageScale, m_StageOffset, m_NewStageModel). A dedicated server draws
//! nothing.
//!
//! PERSISTENCE: a planted plot is saved by its m_sPlotKey (the "farmplots" payload, save version
//! 23) with its crop id and seconds grown. The key must be unique in the world and stable; it is
//! not the entity id. Growth counts only while a server runs.
class LF_FarmPlotComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable key, unique in the world. The saved crop is matched by it")]
	protected string m_sPlotKey;

	[Attribute("cabbage", UIWidgets.EditBox, "Crop id saved with the growth. A saved crop with another id is dropped")]
	protected string m_sCropId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Seed packet a farmer plants here; one is used per planting", "et")]
	protected ResourceName m_SeedPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item a ripe plot gives", "et")]
	protected ResourceName m_HarvestPrefab;

	[Attribute("3", UIWidgets.Auto, "How many items one harvest gives")]
	protected int m_iHarvestAmount;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Crop drawn on every slot: a mesh only, no physics, no replication", "et")]
	protected ResourceName m_CropVisualPrefab;

	[Attribute("", UIWidgets.EditBox, "Crop slots on the bed, local offsets separated by |, e.g. -0.3 0.2 0 | 0.3 0.2 0")]
	protected string m_sSlots;

	[Attribute("", UIWidgets.Object, "Growth stages. Every stage but the last lasts its time in minutes; the crop waits in the last one")]
	protected ref array<ref LF_CropStage> m_aStages;

	[Attribute("3", UIWidgets.Auto, "Metres from the plot a farmer must stand")]
	protected float m_fReach;

	//! EMPTY (-1) or the stage index. Server writes, every machine draws the crop from it.
	[RplProp(onRplName: "OnStageChanged")]
	protected int m_iStage = LF_FarmPlotRules.EMPTY;

	protected bool m_bPlanted;
	protected float m_fGrownSeconds;

	//! The stage the local crop copies show, and the copies themselves (local entities).
	protected int m_iShownStage = LF_FarmPlotRules.EMPTY;
	protected ref array<EntityID> m_aVisualIds = new array<EntityID>();

	//! Live plots by key. Lives here so the component's own OnDelete removes it.
	protected static ref map<string, LF_FarmPlotComponent> s_mLive;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		LF_FarmPlots.Register(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ClearVisuals(owner.GetWorld());

		if (s_mLive && s_mLive.Get(m_sPlotKey) == this)
			LF_FarmPlots.Unregister(this);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static map<string, LF_FarmPlotComponent> Live()
	{
		if (!s_mLive)
			s_mLive = new map<string, LF_FarmPlotComponent>();

		return s_mLive;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetLive()
	{
		s_mLive = null;
	}

	//------------------------------------------------------------------------------------------------
	string GetPlotKey()
	{
		return m_sPlotKey;
	}

	//------------------------------------------------------------------------------------------------
	string GetCropId()
	{
		return m_sCropId;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetSeedPrefab()
	{
		return m_SeedPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetHarvestPrefab()
	{
		return m_HarvestPrefab;
	}

	//------------------------------------------------------------------------------------------------
	int GetHarvestAmount()
	{
		return m_iHarvestAmount;
	}

	//------------------------------------------------------------------------------------------------
	array<ref LF_CropStage> GetStages()
	{
		return m_aStages;
	}

	//------------------------------------------------------------------------------------------------
	//! EMPTY or the stage index. Same answer on the server and on a client.
	int GetStage()
	{
		return m_iStage;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEmpty()
	{
		return m_iStage == LF_FarmPlotRules.EMPTY;
	}

	//------------------------------------------------------------------------------------------------
	bool IsRipe()
	{
		return LF_FarmPlotRules.IsRipe(m_aStages, m_iStage);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seconds the crop has grown, 0 for an empty plot.
	float GetGrownSeconds()
	{
		return m_fGrownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The authored name of the current stage (a #key), empty for an empty plot.
	string GetStageName()
	{
		LF_CropStage stage = LF_BaseCrop.StageAt(m_aStages, m_iStage);
		if (!stage)
			return "";

		return stage.m_sStageName;
	}

	//------------------------------------------------------------------------------------------------
	//! The crop slots on the bed, local to the plot.
	void GetSlots(notnull array<vector> outSlots)
	{
		LF_StoreRules.ParseSlots(m_sSlots, outSlots);
	}

	//------------------------------------------------------------------------------------------------
	//! Local crop copies this machine draws right now.
	int GetVisualCount()
	{
		return m_aVisualIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a farmer plants one seed packet from their inventory.
	LF_EFarmPlot ServerPlant(IEntity user)
	{
		if (!Replication.IsServer())
			return LF_EFarmPlot.NO_PLOT;

		if (LF_Systems.Refuses(LF_FarmPlots.SYSTEM, "planting at " + GetPlotKey()))
			return LF_EFarmPlot.SYSTEM_OFF;

		LF_EFarmPlot reach = CheckReach(user);
		if (reach != LF_EFarmPlot.OK)
			return reach;

		if (m_bPlanted)
			return LF_EFarmPlot.NOT_EMPTY;

		if (!m_aStages || m_aStages.IsEmpty() || m_SeedPrefab.IsEmpty())
		{
			LF_Debug.Error(LF_FarmPlots.FEATURE, string.Format("plot %1 has no stages or no seed prefab; nothing can grow here", m_sPlotKey));
			return LF_EFarmPlot.NO_PLOT;
		}

		if (LF_InventoryUtils.RemoveAmount(user, m_SeedPrefab, 1) != 1)
			return LF_EFarmPlot.NO_SEEDS;

		m_bPlanted = true;
		m_fGrownSeconds = 0;
		Sync();
		LF_Debug.Info(LF_FarmPlots.FEATURE, string.Format("plot %1 planted with %2 by %3", m_sPlotKey, m_sCropId, LF_Utils.GetPlayerUID(user)));
		return LF_EFarmPlot.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a farmer takes the ripe crop; the plot is empty again. A full inventory drops the
	//! rest at the farmer's feet, so a harvest is never lost.
	LF_EFarmPlot ServerHarvest(IEntity user)
	{
		if (!Replication.IsServer())
			return LF_EFarmPlot.NO_PLOT;

		if (LF_Systems.Refuses(LF_FarmPlots.SYSTEM, "harvesting at " + GetPlotKey()))
			return LF_EFarmPlot.SYSTEM_OFF;

		LF_EFarmPlot reach = CheckReach(user);
		if (reach != LF_EFarmPlot.OK)
			return reach;

		if (!m_bPlanted || !IsRipe())
			return LF_EFarmPlot.NOT_RIPE;

		int added = LF_InventoryUtils.AddAmount(user, m_HarvestPrefab, m_iHarvestAmount, true);
		if (added <= 0)
		{
			LF_Debug.Error(LF_FarmPlots.FEATURE, string.Format("plot %1 harvest grant failed: %2 x%3, the crop stays", m_sPlotKey, m_HarvestPrefab, m_iHarvestAmount));
			return LF_EFarmPlot.GRANT_FAILED;
		}

		Clear();
		LF_Debug.Info(LF_FarmPlots.FEATURE, string.Format("plot %1 harvested by %2: %3 x%4", m_sPlotKey, LF_Utils.GetPlayerUID(user), m_HarvestPrefab, added));

		LF_JobManager jobManager = LF_JobManager.GetInstance();
		if (jobManager)
			jobManager.OnGatherCompleted(user, m_HarvestPrefab);

		return LF_EFarmPlot.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EFarmPlot CheckReach(IEntity user)
	{
		if (!user || vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > m_fReach)
			return LF_EFarmPlot.TOO_FAR;

		return LF_EFarmPlot.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the crop grows by seconds (the LF_FarmPlots tick, or a test). An empty plot ignores it.
	void ServerAdvance(float seconds)
	{
		if (!m_bPlanted || seconds <= 0)
			return;

		m_fGrownSeconds += seconds;
		Sync();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the plot is empty again.
	void Clear()
	{
		m_bPlanted = false;
		m_fGrownSeconds = 0;
		Sync();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: derive the stage from the growth and replicate it only when it changes.
	protected void Sync()
	{
		int stage = LF_FarmPlotRules.EMPTY;
		if (m_bPlanted)
			stage = LF_FarmPlotRules.StageForSeconds(m_aStages, m_fGrownSeconds);

		if (stage == m_iStage)
			return;

		m_iStage = stage;
		Replication.BumpMe();
		LF_Debug.Info(LF_FarmPlots.FEATURE, string.Format("plot %1 stage -> %2 %3 (grown %4 s)", m_sPlotKey, m_iStage, GetStageName(), Math.Round(m_fGrownSeconds)));
		ApplyVisuals();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStageChanged()
	{
		ApplyVisuals();
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the crop for m_iStage on this machine: one local copy per slot, replaced when the
	//! stage changes. A dedicated server has no renderer and draws nothing.
	protected void ApplyVisuals()
	{
		if (RplSession.Mode() == RplMode.Dedicated || m_iShownStage == m_iStage)
			return;

		IEntity owner = GetOwner();
		ClearVisuals(owner.GetWorld());
		m_iShownStage = m_iStage;

		LF_CropStage stage = LF_BaseCrop.StageAt(m_aStages, m_iStage);
		if (!stage)
			return;

		Resource resource = Resource.Load(m_CropVisualPrefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(LF_FarmPlots.FEATURE, string.Format("plot %1: crop visual does not load: %2", m_sPlotKey, m_CropVisualPrefab));
			return;
		}

		VObject stageModel;
		if (stage.m_NewStageModel)
		{
			Resource modelResource = Resource.Load(stage.m_NewStageModel);
			if (modelResource && modelResource.IsValid())
				stageModel = modelResource.GetResource().ToVObject();
			else
				LF_Debug.Error(LF_FarmPlots.FEATURE, string.Format("plot %1: stage model does not resolve: %2", m_sPlotKey, stage.m_NewStageModel));
		}

		array<vector> slots = new array<vector>();
		GetSlots(slots);
		foreach (int index, vector slot : slots)
		{
			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;
			params.Parent = owner;
			// A fixed turn per slot so the row does not look stamped.
			Math3D.AnglesToMatrix(Vector(index * 67, 0, 0), params.Transform);
			params.Transform[3] = slot + stage.m_StageOffset;
			params.Scale = stage.m_fStageScale;

			IEntity visual = GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params);
			if (!visual)
			{
				LF_Debug.Error(LF_FarmPlots.FEATURE, string.Format("plot %1: crop visual failed to spawn on slot %2", m_sPlotKey, index));
				continue;
			}

			if (stageModel)
				visual.SetObject(stageModel, "");

			m_aVisualIds.Insert(visual.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearVisuals(BaseWorld world)
	{
		// owner.GetWorld(), not GetGame().GetWorld(): the World Editor deletes entities in a world the
		// game instance does not own.
		if (world)
		{
			foreach (EntityID id : m_aVisualIds)
			{
				IEntity visual = world.FindEntityByID(id);
				if (visual)
					SCR_EntityHelper.DeleteEntityAndChildren(visual);
			}
		}

		m_aVisualIds.Clear();
		m_iShownStage = LF_FarmPlotRules.EMPTY;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved crop, or null for an empty plot.
	LF_FarmPlotRow Export()
	{
		if (!m_bPlanted || m_sPlotKey.IsEmpty())
			return null;

		LF_FarmPlotRow row = new LF_FarmPlotRow();
		row.m_sPlotKey = m_sPlotKey;
		row.m_sCropId = m_sCropId;
		row.m_iGrownSeconds = Math.Floor(m_fGrownSeconds);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts a saved crop back, or empties the plot for a null row. A row for a
	//! different crop (the config changed) is dropped and logged. Idempotent.
	void Import(LF_FarmPlotRow row)
	{
		if (!row)
		{
			Clear();
			return;
		}

		if (row.m_sCropId != m_sCropId)
		{
			LF_Debug.Warn(LF_FarmPlots.FEATURE, string.Format("plot %1: saved %2 dropped, the plot now grows %3", m_sPlotKey, row.m_sCropId, m_sCropId));
			Clear();
			return;
		}

		m_bPlanted = true;
		m_fGrownSeconds = Math.Max(0, row.m_iGrownSeconds);
		Sync();
		LF_Debug.Info(LF_FarmPlots.FEATURE, string.Format("plot %1 restored: %2 grown %3 s, stage %4", m_sPlotKey, m_sCropId, row.m_iGrownSeconds, m_iStage));
	}
}
