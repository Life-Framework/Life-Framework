[BaseContainerProps()]
class EL_ProcessingInput
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab to Input", "et")]
	ResourceName m_InputPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Input/s amount per process")]
	int m_iInputAmount;
}

[BaseContainerProps()]
class EL_ProcessingOutput
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab to Output", "et")]
	ResourceName m_OutputPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Output amount per process")]
	int m_iOutputAmount;
}

class EL_ProcessAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.Object, "List of inputs")]
	ref array<ref EL_ProcessingInput> m_aProcessingInputs;

	[Attribute("", UIWidgets.Object, "List of outputs")]
	ref array<ref EL_ProcessingOutput> m_aProcessingOutputs;

	[Attribute("", UIWidgets.CheckBox, "Force drop output? (Not spawning in inventory)")]
	bool m_bForceDropOutput;

	[Attribute("0 0 0", UIWidgets.EditBox, "Drop Offset", params: "inf inf 0 purposeCoords spaceEntity")]
	vector m_vDropOffset;

	[Attribute("0 0 0", UIWidgets.EditBox, "Drop Rotation")]
	vector m_vRotation;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!EL_NetworkUtils.IsOwner(pOwnerEntity)) return;

		// A malformed recipe must be rejected before any inventory mutation.
		if (!IsRecipeConfigured(m_aProcessingInputs, m_aProcessingOutputs))
		{
			EL_Debug.Error("Processing", "rejected process: recipe is not configured");
			return;
		}
		if (!AreRecipeResourcesAvailable(m_aProcessingInputs, m_aProcessingOutputs))
		{
			EL_Debug.Error("Processing", "rejected process: recipe resource does not resolve");
			return;
		}

		InventoryStorageManagerComponent inventoryManager = EL_Component<InventoryStorageManagerComponent>.Find(pUserEntity);

		// Verify every input is fully present BEFORE consuming or producing anything
		foreach (EL_ProcessingInput processingInput : m_aProcessingInputs)
		{
			if (processingInput.m_iInputAmount < 1 || EL_InventoryUtils.GetAmount(inventoryManager, processingInput.m_InputPrefab) < processingInput.m_iInputAmount)
			{
				EL_Debug.Warn("Processing", string.Format("rejected process: missing input %1", processingInput.m_InputPrefab));
				return;
			}
		}

		array<int> removedAmounts = {};
		for (int inputIndex = 0; inputIndex < m_aProcessingInputs.Count(); inputIndex++)
		{
			EL_ProcessingInput processingInput = m_aProcessingInputs[inputIndex];
			int removed = EL_InventoryUtils.RemoveAmount(inventoryManager, processingInput.m_InputPrefab, processingInput.m_iInputAmount);
			removedAmounts.Insert(removed);
			if (removed == processingInput.m_iInputAmount)
				continue;

			for (int rollbackIndex = 0; rollbackIndex <= inputIndex; rollbackIndex++)
			{
				if (removedAmounts[rollbackIndex] > 0)
					EL_InventoryUtils.AddAmount(inventoryManager, m_aProcessingInputs[rollbackIndex].m_InputPrefab, removedAmounts[rollbackIndex]);
			}

			EL_Debug.Error("Processing", string.Format("rejected process: input removal changed during validation (%1/%2)", removed, processingInput.m_iInputAmount));
			return;
		}

		array<IEntity> spawnedOutputs = {};
		foreach (EL_ProcessingOutput processingOutput : m_aProcessingOutputs)
		{
			if (m_bForceDropOutput)
			{
				for (int i = 0; i < processingOutput.m_iOutputAmount; i++)
				{
					IEntity outputEntity = EL_Utils.SpawnEntityPrefab(processingOutput.m_OutputPrefab, pOwnerEntity.GetOrigin() + m_vDropOffset, m_vRotation);
					if (outputEntity)
					{
						spawnedOutputs.Insert(outputEntity);
						continue;
					}

					for (int outputIndex = 0; outputIndex < spawnedOutputs.Count(); outputIndex++)
						SCR_EntityHelper.DeleteEntityAndChildren(spawnedOutputs[outputIndex]);
					for (int rollbackIndex = 0; rollbackIndex < removedAmounts.Count(); rollbackIndex++)
					{
						if (removedAmounts[rollbackIndex] > 0)
							EL_InventoryUtils.AddAmount(inventoryManager, m_aProcessingInputs[rollbackIndex].m_InputPrefab, removedAmounts[rollbackIndex]);
					}

					EL_Debug.Error("Processing", "rejected process: output spawn failed");
					return;
				}
			}
			else
			{
				int added = EL_InventoryUtils.AddAmount(inventoryManager, processingOutput.m_OutputPrefab, processingOutput.m_iOutputAmount, true);
				if (added != processingOutput.m_iOutputAmount)
				{
					for (int rollbackIndex = 0; rollbackIndex < removedAmounts.Count(); rollbackIndex++)
					{
						if (removedAmounts[rollbackIndex] > 0)
							EL_InventoryUtils.AddAmount(inventoryManager, m_aProcessingInputs[rollbackIndex].m_InputPrefab, removedAmounts[rollbackIndex]);
					}

					EL_Debug.Error("Processing", string.Format("rejected process: output delivery failed (%1/%2)", added, processingOutput.m_iOutputAmount));
					return;
				}
			}

			// Notify job manager for reward
			EL_JobManager jobManager = EL_JobManager.GetInstance();
			if (jobManager)
			{
				jobManager.OnProcessCompleted(pUserEntity, processingOutput.m_OutputPrefab);
			}

			EL_Debug.Log("Processing", string.Format("processed %1 x%2", processingOutput.m_OutputPrefab, processingOutput.m_iOutputAmount));
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
  	{
		if (!IsRecipeConfigured(m_aProcessingInputs, m_aProcessingOutputs))
			return false;

		InventoryStorageManagerComponent inventoryManager = EL_Component<InventoryStorageManagerComponent>.Find(user);
		foreach (EL_ProcessingInput processingInput : m_aProcessingInputs)
		{
			int inputPrefabsInInv = EL_InventoryUtils.GetAmount(inventoryManager, processingInput.m_InputPrefab);
			if (inputPrefabsInInv < processingInput.m_iInputAmount) return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Validates recipe shape before the server touches inventory.
	static bool IsRecipeConfigured(array<ref EL_ProcessingInput> inputs, array<ref EL_ProcessingOutput> outputs)
	{
		if (!inputs || inputs.IsEmpty() || !outputs || outputs.IsEmpty())
			return false;

		foreach (EL_ProcessingInput input : inputs)
		{
			if (!input || input.m_InputPrefab.IsEmpty() || input.m_iInputAmount < 1)
				return false;
		}

		foreach (EL_ProcessingOutput output : outputs)
		{
			if (!output || output.m_OutputPrefab.IsEmpty() || output.m_iOutputAmount < 1)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Resource loading is a boundary. Validate every reference before inputs
	//! are consumed so a broken config cannot delete a player's materials.
	static bool AreRecipeResourcesAvailable(array<ref EL_ProcessingInput> inputs, array<ref EL_ProcessingOutput> outputs)
	{
		foreach (EL_ProcessingInput input : inputs)
		{
			Resource inputResource = Resource.Load(input.m_InputPrefab);
			if (!inputResource || !inputResource.IsValid())
				return false;
		}

		foreach (EL_ProcessingOutput output : outputs)
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
		SetCannotPerformReason("Can't find items");
	}
}
