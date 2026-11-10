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

	protected LF_HideoutMachineComponent m_pHideoutMachine;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity)) return;
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

		InventoryStorageManagerComponent inventoryManager = LF_Component<InventoryStorageManagerComponent>.Find(pUserEntity);

		// Every input is verified and removed together, or none is.
		if (!TryConsumeInputs(inventoryManager, m_aProcessingInputs))
			return;

		array<IEntity> spawnedOutputs = {};
		foreach (LF_ProcessingOutput processingOutput : m_aProcessingOutputs)
		{
			if (m_bForceDropOutput)
			{
				for (int i = 0; i < processingOutput.m_iOutputAmount; i++)
				{
					IEntity outputEntity = LF_Utils.SpawnEntityPrefab(processingOutput.m_OutputPrefab, pOwnerEntity.GetOrigin() + m_vDropOffset, m_vRotation);
					if (outputEntity)
					{
						spawnedOutputs.Insert(outputEntity);
						continue;
					}

					for (int outputIndex = 0; outputIndex < spawnedOutputs.Count(); outputIndex++)
						SCR_EntityHelper.DeleteEntityAndChildren(spawnedOutputs[outputIndex]);
					RestoreInputs(inventoryManager, m_aProcessingInputs);

					LF_Debug.Error("Processing", "rejected process: output spawn failed");
					return;
				}
			}
			else
			{
				int added = LF_InventoryUtils.AddAmount(inventoryManager, processingOutput.m_OutputPrefab, processingOutput.m_iOutputAmount, true);
				if (added != processingOutput.m_iOutputAmount)
				{
					RestoreInputs(inventoryManager, m_aProcessingInputs);

					LF_Debug.Error("Processing", string.Format("rejected process: output delivery failed (%1/%2)", added, processingOutput.m_iOutputAmount));
					return;
				}
			}

			// Notify job manager for reward
			LF_JobManager jobManager = LF_JobManager.GetInstance();
			if (jobManager)
			{
				jobManager.OnProcessCompleted(pUserEntity, processingOutput.m_OutputPrefab);
			}

			LF_Debug.Log("Processing", string.Format("processed %1 x%2", processingOutput.m_OutputPrefab, processingOutput.m_iOutputAmount));
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
  	{
		if (m_pHideoutMachine && Replication.IsServer() && !LF_Hideout.CanProcessUser(user, m_pHideoutMachine.GetHideoutId()))
		{
			SetCannotPerformReason("#EL-Hideout_NoUpgrade");
			return false;
		}

		if (!ApplyMachineFromCatalog())
			return false;

		if (!IsRecipeConfigured(m_aProcessingInputs, m_aProcessingOutputs))
			return false;

		InventoryStorageManagerComponent inventoryManager = LF_Component<InventoryStorageManagerComponent>.Find(user);
		foreach (LF_ProcessingInput processingInput : m_aProcessingInputs)
		{
			int inputPrefabsInInv = LF_InventoryUtils.GetAmount(inventoryManager, processingInput.m_InputPrefab);
			if (inputPrefabsInInv < processingInput.m_iInputAmount) return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Copies inputs and outputs from the chain config when this action names a
	//! machine. An empty or unresolved recipe logs and returns before the
	//! caller is allowed to touch inventory. An empty id keeps the prefab arrays.
	protected bool ApplyMachineFromCatalog()
	{
		if (m_sMachineId.IsEmpty())
			return true;

		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(m_sMachineId);
		if (!LF_ResourceChainCatalog.IsMachineUsable(machine))
		{
			LF_Debug.Error("Processing", string.Format("rejected process: machine %1 recipe is empty or a prefab does not resolve", m_sMachineId));
			m_aProcessingInputs = new array<ref LF_ProcessingInput>();
			m_aProcessingOutputs = new array<ref LF_ProcessingOutput>();
			return false;
		}

		m_aProcessingInputs = machine.m_aInputs;
		m_aProcessingOutputs = machine.m_aOutputs;
		return true;
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
