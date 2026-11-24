[BaseContainerProps()]
class LF_ProcessingInput
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab to Input", "et")]
	ResourceName m_InputPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Input/s amount per process")]
	int m_iInputAmount;
}

[BaseContainerProps()]
class LF_ProcessingOutput
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab to Output", "et")]
	ResourceName m_OutputPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Output amount per process")]
	int m_iOutputAmount;
}

class LF_ProcessAction : ScriptedUserAction
{
	//! The system a server switches the machines off with (#1075).
	static const string SYSTEM = "Processing";

	[Attribute("", UIWidgets.EditBox, "Chain machine id. When set, inputs and outputs come from ResourceChains.conf")]
	string m_sMachineId;

	[Attribute("", UIWidgets.Object, "List of inputs")]
	ref array<ref LF_ProcessingInput> m_aProcessingInputs;

	[Attribute("", UIWidgets.Object, "List of outputs")]
	ref array<ref LF_ProcessingOutput> m_aProcessingOutputs;

	[Attribute("", UIWidgets.CheckBox, "Force drop output? (Not spawning in inventory)")]
	bool m_bForceDropOutput;

	[Attribute("0 0 0", UIWidgets.EditBox, "Drop Offset", params: "inf inf 0 purposeCoords spaceEntity")]
	vector m_vDropOffset;

	[Attribute("0 0 0", UIWidgets.EditBox, "Drop Rotation")]
	vector m_vRotation;

	[Attribute("12", UIWidgets.Auto, "Seconds the machine is heard running after a process, when its prefab has an LF_AmbientSwitchComponent (the sawmill)")]
	float m_fRunSoundSeconds;

	protected LF_HideoutMachineComponent m_pHideoutMachine;

	//! The chain row this action read its recipe from; null for a prefab-configured recipe.
	protected LF_ResourceMachineDef m_Machine;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity)) return;
		if (LF_Systems.Refuses(SYSTEM, "process at " + m_sMachineId))
			return;
		if (!LF_NetworkUtils.IsOwner(pOwnerEntity))
			LF_Debug.Warn("Processing", string.Format("process applied without replication owner on %1", pOwnerEntity.GetName()));

		if (m_pHideoutMachine && !LF_Hideout.CanProcessUser(pUserEntity, m_pHideoutMachine.GetHideoutId()))
			return;

		if (!ApplyMachineFromCatalog())
			return;

		// A malformed recipe must be rejected before any inventory mutation.
		if (!IsRecipeConfigured(m_aProcessingInputs, m_aProcessingOutputs))
		{
			LF_Debug.Error("Processing", "rejected process: recipe is not configured");
			return;
		}
		if (!AreRecipeResourcesAvailable(m_aProcessingInputs, m_aProcessingOutputs))
		{
			LF_Debug.Error("Processing", "rejected process: recipe resource does not resolve");
			return;
		}

		// The machine's job-level gate (#3226): the server decides, the player reads the job's words.
		int level = LevelFor(pUserEntity, m_Machine);
		string levelRefusal = LevelRefusal(m_Machine, level);
		if (!levelRefusal.IsEmpty())
		{
			LF_Debug.Info("Processing", string.Format("refused process at %1: %2 level below %3", m_sMachineId, m_Machine.m_sLevelJob, m_Machine.m_iMinLevel));
			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Shown(levelRefusal), "", 4.0);
			return;
		}

		// A timed machine runs one job at a time (#3229); the second worker reads that it is busy.
		if (m_Machine && m_Machine.m_iProcessSeconds > 0 && LF_ProcessRun.IsBusy(pOwnerEntity))
		{
			LF_Debug.Info("Processing", string.Format("refused process at %1: a run is under way", m_sMachineId));
			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Text("#LF-Process_Busy"), "", 4.0);
			return;
		}

		InventoryStorageManagerComponent inventoryManager = LF_Component<InventoryStorageManagerComponent>.Find(pUserEntity);

		// As many whole batches as the inventory covers, up to the machine's cap.
		array<int> held = HeldAmounts(inventoryManager, m_aProcessingInputs);
		int batches = BatchesFor(held, m_aProcessingInputs, MaxBatchesAt(m_Machine, level));
		if (batches < 1)
		{
			ResourceName missing;
			int missingIndex = MissingInputIndex(held, m_aProcessingInputs);
			if (missingIndex >= 0)
				missing = m_aProcessingInputs[missingIndex].m_InputPrefab;

			LF_Debug.Warn("Processing", string.Format("rejected process at %1: missing input %2", m_sMachineId, missing));
			LF_Utils.NotifyEntity(pUserEntity, NeedsText(missing), "", 4.0);
			return;
		}

		array<ref LF_ProcessingInput> inputs = ScaleInputs(m_aProcessingInputs, batches);
		array<ref LF_ProcessingOutput> outputs = ScaleOutputs(m_aProcessingOutputs, batches);

		// Every input is verified and removed together, or none is.
		if (!TryConsumeInputs(inventoryManager, inputs))
			return;

		// A timed machine keeps the outputs back: they drop at the machine when the time is up (#3229).
		if (m_Machine && m_Machine.m_iProcessSeconds > 0)
		{
			int seconds = SecondsAt(m_Machine, level);
			if (!LF_ProcessRun.Start(pOwnerEntity, pUserEntity, m_sMachineId, outputs, batches, seconds, m_vDropOffset, m_vRotation))
			{
				RestoreInputs(inventoryManager, inputs);
				LF_Debug.Error("Processing", string.Format("rejected process at %1: the timed run did not start, inputs given back", m_sMachineId));
				return;
			}

			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Text("#LF-Process_Started", seconds.ToString()), "", 4.0);
			return;
		}

		array<IEntity> spawnedOutputs = {};
		foreach (LF_ProcessingOutput processingOutput : outputs)
		{
			if (m_bForceDropOutput)
			{
				// A hand-carry output that stacks (planks) drops as carried bundles: one trip takes a bundle (#4875).
				int left = processingOutput.m_iOutputAmount;
				while (left > 0)
				{
					IEntity outputEntity = LF_Utils.SpawnEntityPrefab(processingOutput.m_OutputPrefab, pOwnerEntity.GetOrigin() + m_vDropOffset, m_vRotation);
					if (outputEntity)
					{
						spawnedOutputs.Insert(outputEntity);
						left -= LF_ProcessRun.TakeIntoBundle(outputEntity, left);
						continue;
					}

					for (int outputIndex = 0; outputIndex < spawnedOutputs.Count(); outputIndex++)
						SCR_EntityHelper.DeleteEntityAndChildren(spawnedOutputs[outputIndex]);
					RestoreInputs(inventoryManager, inputs);

					LF_Debug.Error("Processing", "rejected process: output spawn failed");
					return;
				}
			}
			else
			{
				int added = LF_InventoryUtils.AddAmount(inventoryManager, processingOutput.m_OutputPrefab, processingOutput.m_iOutputAmount, true);
				if (added != processingOutput.m_iOutputAmount)
				{
					RestoreInputs(inventoryManager, inputs);

					LF_Debug.Error("Processing", string.Format("rejected process: output delivery failed (%1/%2)", added, processingOutput.m_iOutputAmount));
					return;
				}
			}

			// Notify job manager for reward
			LF_JobManager jobManager = LF_JobManager.GetInstance();
			if (jobManager)
			{
				// Once per batch, so a five-batch run pays what five single runs pay.
				for (int batch = 0; batch < batches; batch++)
					jobManager.OnProcessCompleted(pUserEntity, processingOutput.m_OutputPrefab);
			}

			LF_Debug.Log("Processing", string.Format("processed %1 x%2 (%3 batch(es))", processingOutput.m_OutputPrefab, processingOutput.m_iOutputAmount, batches));
		}

		// The machine's trade pays its XP once for the whole run (a furnace pays SMELTING, #4337).
		LF_ProcessXp.PayBatches(pUserEntity, m_sMachineId, batches);

		LF_Utils.NotifyEntity(pUserEntity, ToastText(outputs), "", 4.0);

		// The machine is heard running for a while (Feature/Atmosphere); no switch on the prefab, no sound.
		LF_AmbientSwitchComponent.PulseOn(pOwnerEntity, m_fRunSoundSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! A server with Processing off offers nothing at the machine (#1075).
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	//! While a timed run is under way the row reads the time left ("Smelt iron ore: 3:12 left").
	override bool GetActionNameScript(out string outName)
	{
		LF_ProcessRunFxComponent fx = LF_Component<LF_ProcessRunFxComponent>.Find(GetOwner());
		if (!fx || !fx.IsRunning())
			return false;

		UIInfo info = GetUIInfo();
		string name;
		if (info)
			name = info.GetName();

		outName = WidgetManager.Translate("#LF-Process_RunningLeft", WidgetManager.Translate(name), LF_ProcessRunFxComponent.Clock(fx.GetSecondsLeft()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
  	{
		// A run under way: the row shows the time left and takes no second load (the server refuses it too).
		LF_ProcessRunFxComponent runFx = LF_Component<LF_ProcessRunFxComponent>.Find(GetOwner());
		if (runFx && runFx.IsRunning())
		{
			SetCannotPerformReason("#LF-Process_Busy");
			return false;
		}

		if (m_pHideoutMachine && Replication.IsServer() && !LF_Hideout.CanProcessUser(user, m_pHideoutMachine.GetHideoutId()))
		{
			SetCannotPerformReason("#EL-Hideout_NoUpgrade");
			return false;
		}

		if (!ApplyMachineFromCatalog())
			return false;

		if (!IsRecipeConfigured(m_aProcessingInputs, m_aProcessingOutputs))
			return false;

		// Levels live on the server; a client is refused there, in the job's words.
		if (Replication.IsServer())
		{
			string levelRefusal = LevelRefusal(m_Machine, LevelFor(user, m_Machine));
			if (!levelRefusal.IsEmpty())
			{
				SetCannotPerformReason(levelRefusal);
				return false;
			}
		}

		InventoryStorageManagerComponent inventoryManager = LF_Component<InventoryStorageManagerComponent>.Find(user);
		int missingIndex = MissingInputIndex(HeldAmounts(inventoryManager, m_aProcessingInputs), m_aProcessingInputs);
		if (missingIndex >= 0)
		{
			// Names the reagent still needed ("Needs Coal"), not just "no items".
			SetCannotPerformReason(NeedsText(m_aProcessingInputs[missingIndex].m_InputPrefab));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The player's level in the machine's gate job; 0 when the machine has no gate.
	protected static int LevelFor(IEntity user, LF_ResourceMachineDef machine)
	{
		if (!machine || machine.m_sLevelJob.IsEmpty())
			return 0;

		return LF_JobLevels.LevelOfBody(user, machine.m_sLevelJob);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The `#` key a player at \p level reads at this machine, or empty when the machine has
	//! no gate or the level meets it. No job, or a minimum of 1 or less, is no gate (#3226).
	static string LevelRefusal(LF_ResourceMachineDef machine, int level)
	{
		if (!machine || machine.m_sLevelJob.IsEmpty() || machine.m_iMinLevel <= 1)
			return "";

		return LF_JobLevelWords.Refusal(machine.m_sLevelJob, level, machine.m_iMinLevel);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The batch cap of a machine row: its m_iMaxBatches, at least 1; 1 with no row.
	static int MaxBatches(LF_ResourceMachineDef machine)
	{
		if (!machine)
			return 1;

		return LF_Utils.MaxInt(1, machine.m_iMaxBatches);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The batch cap of a worker at \p level: the row's cap, raised by any step of m_aSteps at or below the level (#4861).
	static int MaxBatchesAt(LF_ResourceMachineDef machine, int level)
	{
		int cap = MaxBatches(machine);
		if (!machine || !machine.m_aSteps)
			return cap;

		foreach (LF_MachineStep step : machine.m_aSteps)
		{
			if (step && step.m_iLevel <= level && step.m_iMaxBatches > cap)
				cap = step.m_iMaxBatches;
		}

		return cap;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The seconds a run takes for a worker at \p level: the row's time, cut by any step of m_aSteps at or below the level; 0 for an instant machine (#4861).
	static int SecondsAt(LF_ResourceMachineDef machine, int level)
	{
		if (!machine || machine.m_iProcessSeconds <= 0)
			return 0;

		int seconds = machine.m_iProcessSeconds;
		if (!machine.m_aSteps)
			return seconds;

		foreach (LF_MachineStep step : machine.m_aSteps)
		{
			if (step && step.m_iLevel <= level && step.m_iProcessSeconds > 0 && step.m_iProcessSeconds < seconds)
				seconds = step.m_iProcessSeconds;
		}

		return seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How much of each input the inventory holds, in the order of \p inputs.
	static array<int> HeldAmounts(InventoryStorageManagerComponent inventoryManager, array<ref LF_ProcessingInput> inputs)
	{
		array<int> held = {};
		if (!inputs)
			return held;

		foreach (LF_ProcessingInput input : inputs)
		{
			if (!input || !inventoryManager)
			{
				held.Insert(0);
				continue;
			}

			held.Insert(LF_InventoryUtils.GetAmount(inventoryManager, input.m_InputPrefab));
		}

		return held;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The whole batches \p held covers, at most \p maxBatches (at least 1); 0 when any input
	//! is short or the recipe is malformed. Pure, so a LOGIC test drives it.
	static int BatchesFor(array<int> held, array<ref LF_ProcessingInput> inputs, int maxBatches)
	{
		if (!held || !inputs || inputs.IsEmpty() || held.Count() != inputs.Count())
			return 0;

		int batches = LF_Utils.MaxInt(1, maxBatches);
		for (int i = 0; i < inputs.Count(); i++)
		{
			LF_ProcessingInput input = inputs[i];
			if (!input || input.m_iInputAmount < 1)
				return 0;

			batches = LF_Utils.MinInt(batches, held[i] / input.m_iInputAmount);
		}

		return LF_Utils.MaxInt(0, batches);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The index of the first input \p held does not cover once, or -1 when all are covered.
	static int MissingInputIndex(array<int> held, array<ref LF_ProcessingInput> inputs)
	{
		if (!inputs)
			return -1;

		for (int i = 0; i < inputs.Count(); i++)
		{
			LF_ProcessingInput input = inputs[i];
			if (!input)
				return i;

			int have = 0;
			if (held && i < held.Count())
				have = held[i];
			if (have < input.m_iInputAmount)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fresh input rows for \p batches batches; the catalog rows are never changed.
	static array<ref LF_ProcessingInput> ScaleInputs(array<ref LF_ProcessingInput> inputs, int batches)
	{
		array<ref LF_ProcessingInput> scaled = new array<ref LF_ProcessingInput>();
		if (!inputs)
			return scaled;

		foreach (LF_ProcessingInput input : inputs)
		{
			LF_ProcessingInput row = new LF_ProcessingInput();
			row.m_InputPrefab = input.m_InputPrefab;
			row.m_iInputAmount = input.m_iInputAmount * batches;
			scaled.Insert(row);
		}

		return scaled;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fresh output rows for \p batches batches; the catalog rows are never changed.
	static array<ref LF_ProcessingOutput> ScaleOutputs(array<ref LF_ProcessingOutput> outputs, int batches)
	{
		array<ref LF_ProcessingOutput> scaled = new array<ref LF_ProcessingOutput>();
		if (!outputs)
			return scaled;

		foreach (LF_ProcessingOutput output : outputs)
		{
			LF_ProcessingOutput row = new LF_ProcessingOutput();
			row.m_OutputPrefab = output.m_OutputPrefab;
			row.m_iOutputAmount = output.m_iOutputAmount * batches;
			scaled.Insert(row);
		}

		return scaled;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal a player reads when an input is short: "Needs <item>"; the old "missing the
	//! input materials" when the recipe gives no item to name.
	static string NeedsText(ResourceName missing)
	{
		if (missing.IsEmpty())
			return LF_Terms.Text("#LF-Process_NoItems");

		return LF_Terms.Text("#LF-Process_Needs", LF_GatherAction.ItemName(missing));
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Copies inputs and outputs from the chain config when this action names a
	//! machine. An empty or unresolved recipe logs and returns before the
	//! caller is allowed to touch inventory. An empty id keeps the prefab arrays.
	protected bool ApplyMachineFromCatalog()
	{
		if (m_sMachineId.IsEmpty())
		{
			m_Machine = null;
			return true;
		}

		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(m_sMachineId);
		if (!LF_ResourceChainCatalog.IsMachineUsable(machine))
		{
			LF_Debug.Error("Processing", string.Format("rejected process: machine %1 recipe is empty or a prefab does not resolve", m_sMachineId));
			m_aProcessingInputs = new array<ref LF_ProcessingInput>();
			m_aProcessingOutputs = new array<ref LF_ProcessingOutput>();
			m_Machine = null;
			return false;
		}

		m_Machine = machine;
		m_aProcessingInputs = machine.m_aInputs;
		m_aProcessingOutputs = machine.m_aOutputs;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast sentence for a finished process: what came out, and what a trader pays for the first
	//! output a market row lists (left out when none does).
	static string ToastText(array<ref LF_ProcessingOutput> outputs)
	{
		string made;
		int price;
		string soldAsName;
		foreach (LF_ProcessingOutput output : outputs)
		{
			if (!output)
				continue;

			if (!made.IsEmpty())
				made = made + ", ";
			made = made + LF_Terms.Text("#LF-Gather_Toast", LF_GatherAction.ItemName(output.m_OutputPrefab), output.m_iOutputAmount.ToString());

			if (price > 0)
				continue;

			ResourceName soldAs;
			price = LF_GatherAction.TraderPrice(output.m_OutputPrefab, soldAs);
			if (price > 0)
				soldAsName = LF_GatherAction.ItemName(soldAs);
		}

		return made + LF_GatherAction.PriceSentence(price, soldAsName);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every input from the inventory, or none. A missing or short input logs and returns
	//! false before anything is taken; a removal that comes up short puts back what it took.
	//! Shared by the instant processors and the chain stations.
	static bool TryConsumeInputs(InventoryStorageManagerComponent inventoryManager, array<ref LF_ProcessingInput> inputs)
	{
		foreach (LF_ProcessingInput processingInput : inputs)
		{
			if (processingInput.m_iInputAmount < 1 || LF_InventoryUtils.GetAmount(inventoryManager, processingInput.m_InputPrefab) < processingInput.m_iInputAmount)
			{
				LF_Debug.Warn("Processing", string.Format("rejected process: missing input %1", processingInput.m_InputPrefab));
				return false;
			}
		}

		array<int> removedAmounts = {};
		for (int inputIndex = 0; inputIndex < inputs.Count(); inputIndex++)
		{
			LF_ProcessingInput processingInput = inputs[inputIndex];
			int removed = LF_InventoryUtils.RemoveAmount(inventoryManager, processingInput.m_InputPrefab, processingInput.m_iInputAmount);
			removedAmounts.Insert(removed);
			if (removed == processingInput.m_iInputAmount)
				continue;

			for (int rollbackIndex = 0; rollbackIndex <= inputIndex; rollbackIndex++)
			{
				if (removedAmounts[rollbackIndex] > 0)
					LF_InventoryUtils.AddAmount(inventoryManager, inputs[rollbackIndex].m_InputPrefab, removedAmounts[rollbackIndex]);
			}

			LF_Debug.Error("Processing", string.Format("rejected process: input removal changed during validation (%1/%2)", removed, processingInput.m_iInputAmount));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives back every input after TryConsumeInputs succeeded and a later step failed.
	static void RestoreInputs(InventoryStorageManagerComponent inventoryManager, array<ref LF_ProcessingInput> inputs)
	{
		foreach (LF_ProcessingInput processingInput : inputs)
			LF_InventoryUtils.AddAmount(inventoryManager, processingInput.m_InputPrefab, processingInput.m_iInputAmount);
	}

	//------------------------------------------------------------------------------------------------
	//! Validates recipe shape before the server touches inventory.
	static bool IsRecipeConfigured(array<ref LF_ProcessingInput> inputs, array<ref LF_ProcessingOutput> outputs)
	{
		if (!inputs || inputs.IsEmpty() || !outputs || outputs.IsEmpty())
			return false;

		foreach (LF_ProcessingInput input : inputs)
		{
			if (!input || input.m_InputPrefab.IsEmpty() || input.m_iInputAmount < 1)
				return false;
		}

		foreach (LF_ProcessingOutput output : outputs)
		{
			if (!output || output.m_OutputPrefab.IsEmpty() || output.m_iOutputAmount < 1)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Resource loading is a boundary. Validate every reference before inputs
	//! are consumed so a broken config cannot delete a player's materials.
	static bool AreRecipeResourcesAvailable(array<ref LF_ProcessingInput> inputs, array<ref LF_ProcessingOutput> outputs)
	{
		foreach (LF_ProcessingInput input : inputs)
		{
			Resource inputResource = Resource.Load(input.m_InputPrefab);
			if (!inputResource || !inputResource.IsValid())
				return false;
		}

		foreach (LF_ProcessingOutput output : outputs)
		{
			Resource outputResource = Resource.Load(output.m_OutputPrefab);
			if (!outputResource || !outputResource.IsValid())
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_pHideoutMachine = LF_Component<LF_HideoutMachineComponent>.Find(pOwnerEntity);
		SetCannotPerformReason("#LF-Process_NoItems");
	}
}
