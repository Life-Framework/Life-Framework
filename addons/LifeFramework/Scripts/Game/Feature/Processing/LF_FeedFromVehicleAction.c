//! The station's process action reads "Feed from the truck" when the worker does not carry a batch and
//! a vehicle is backed up to the station (#4191). The server decides again on Perform: the worker's own
//! inventory still comes first, the vehicle only when it does not cover a batch.
modded class LF_ProcessAction
{
	//! Set by CanBeShownScript for the name the action list shows next.
	protected bool m_bFeedFromVehicle;

	//------------------------------------------------------------------------------------------------
	//! \return The vehicle this action would feed \p pOwnerEntity from for \p user, or null when the
	//! worker carries a batch, the recipe is not usable or no vehicle is backed up.
	IEntity FeedVehicleFor(IEntity pOwnerEntity, IEntity user)
	{
		if (!pOwnerEntity || !ApplyMachineFromCatalog() || !IsRecipeConfigured(m_aProcessingInputs, m_aProcessingOutputs))
			return null;

		InventoryStorageManagerComponent userStorage = LF_Component<InventoryStorageManagerComponent>.Find(user);
		if (MissingInputIndex(HeldAmounts(userStorage, m_aProcessingInputs), m_aProcessingInputs) < 0)
			return null;

		return LF_FeedFromVehicle.FindVehicle(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;

		m_bFeedFromVehicle = FeedVehicleFor(GetOwner(), user) != null;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_bFeedFromVehicle)
			return super.GetActionNameScript(outName);

		outName = "#LF-Process_FeedFromTruck";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		IEntity vehicle = FeedVehicleFor(GetOwner(), user);
		if (!vehicle)
			return super.CanBePerformedScript(user);

		if (m_pHideoutMachine && Replication.IsServer() && !LF_Hideout.CanProcessUser(user, m_pHideoutMachine.GetHideoutId()))
		{
			SetCannotPerformReason("#EL-Hideout_NoUpgrade");
			return false;
		}

		if (LF_FeedFromVehicle.IsLocked(vehicle))
		{
			SetCannotPerformReason("#LF-Process_TruckLocked");
			return false;
		}

		if (Replication.IsServer())
		{
			string levelRefusal = LevelRefusal(m_Machine, LevelFor(user, m_Machine));
			if (!levelRefusal.IsEmpty())
			{
				SetCannotPerformReason(levelRefusal);
				return false;
			}
		}

		int missingIndex = MissingInputIndex(HeldAmounts(LF_FeedFromVehicle.StorageOf(vehicle), m_aProcessingInputs), m_aProcessingInputs);
		if (missingIndex >= 0)
		{
			SetCannotPerformReason(NeedsText(m_aProcessingInputs[missingIndex].m_InputPrefab));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity))
			return;

		IEntity vehicle = FeedVehicleFor(pOwnerEntity, pUserEntity);
		if (!vehicle)
		{
			super.PerformAction(pOwnerEntity, pUserEntity);
			return;
		}

		FeedFrom(pOwnerEntity, pUserEntity, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! The server's feed: the same gates as a carried process, then every input from \p vehicle or none.
	protected void FeedFrom(IEntity pOwnerEntity, IEntity pUserEntity, IEntity vehicle)
	{
		if (LF_Systems.Refuses(SYSTEM, "feed from vehicle at " + m_sMachineId))
			return;

		if (m_pHideoutMachine && !LF_Hideout.CanProcessUser(pUserEntity, m_pHideoutMachine.GetHideoutId()))
			return;

		if (!AreRecipeResourcesAvailable(m_aProcessingInputs, m_aProcessingOutputs))
		{
			LF_Debug.Error("Processing", "rejected feed from vehicle: recipe resource does not resolve");
			return;
		}

		if (LF_FeedFromVehicle.IsLocked(vehicle))
		{
			LF_Debug.Info("Processing", string.Format("refused feed at %1: the vehicle is locked", m_sMachineId));
			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Text("#LF-Process_TruckLocked"), "", 4.0);
			return;
		}

		string levelRefusal = LevelRefusal(m_Machine, LevelFor(pUserEntity, m_Machine));
		if (!levelRefusal.IsEmpty())
		{
			LF_Debug.Info("Processing", string.Format("refused feed at %1: %2 level below %3", m_sMachineId, m_Machine.m_sLevelJob, m_Machine.m_iMinLevel));
			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Text(levelRefusal), "", 4.0);
			return;
		}

		bool timed = m_Machine && m_Machine.m_iProcessSeconds > 0;
		if (timed && LF_ProcessRun.IsBusy(pOwnerEntity))
		{
			LF_Debug.Info("Processing", string.Format("refused feed at %1: a run is under way", m_sMachineId));
			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Text("#LF-Process_Busy"), "", 4.0);
			return;
		}

		InventoryStorageManagerComponent vehicleStorage = LF_FeedFromVehicle.StorageOf(vehicle);
		array<int> held = HeldAmounts(vehicleStorage, m_aProcessingInputs);
		int batches = BatchesFor(held, m_aProcessingInputs, MaxBatches(m_Machine));
		if (batches < 1)
		{
			ResourceName missing;
			int missingIndex = MissingInputIndex(held, m_aProcessingInputs);
			if (missingIndex >= 0)
				missing = m_aProcessingInputs[missingIndex].m_InputPrefab;

			LF_Debug.Warn("Processing", string.Format("rejected feed at %1: the vehicle lacks %2", m_sMachineId, missing));
			LF_Utils.NotifyEntity(pUserEntity, NeedsText(missing), "", 4.0);
			return;
		}

		array<ref LF_ProcessingInput> inputs = ScaleInputs(m_aProcessingInputs, batches);
		array<ref LF_ProcessingOutput> outputs = ScaleOutputs(m_aProcessingOutputs, batches);

		// Every input leaves the vehicle together, or none does.
		if (!TryConsumeInputs(vehicleStorage, inputs))
			return;

		if (timed)
		{
			if (!LF_ProcessRun.Start(pOwnerEntity, pUserEntity, m_sMachineId, outputs, batches, m_Machine.m_iProcessSeconds, m_vDropOffset, m_vRotation))
			{
				RestoreInputs(vehicleStorage, inputs);
				LF_Debug.Error("Processing", string.Format("rejected feed at %1: the timed run did not start, inputs back in the vehicle", m_sMachineId));
				return;
			}

			LF_Debug.Info("Processing", string.Format("fed %1 batch(es) at %2 from the vehicle, timed run started", batches, m_sMachineId));
			LF_Utils.NotifyEntity(pUserEntity, LF_Terms.Text("#LF-Process_Started", m_Machine.m_iProcessSeconds.ToString()), "", 4.0);
			return;
		}

		if (!LF_FeedFromVehicle.DeliverOutputs(vehicleStorage, pUserEntity, pOwnerEntity, outputs, m_bForceDropOutput, m_vDropOffset, m_vRotation))
		{
			RestoreInputs(vehicleStorage, inputs);
			LF_Debug.Error("Processing", string.Format("rejected feed at %1: output delivery failed, inputs back in the vehicle", m_sMachineId));
			return;
		}

		LF_JobManager jobManager = LF_JobManager.GetInstance();
		foreach (LF_ProcessingOutput output : outputs)
		{
			if (!jobManager)
				break;

			for (int batch = 0; batch < batches; batch++)
				jobManager.OnProcessCompleted(pUserEntity, output.m_OutputPrefab);
		}

		LF_Debug.Info("Processing", string.Format("fed %1 batch(es) at %2 from the vehicle", batches, m_sMachineId));
		LF_Utils.NotifyEntity(pUserEntity, ToastText(outputs), "", 4.0);
		LF_AmbientSwitchComponent.PulseOn(pOwnerEntity, m_fRunSoundSeconds);
	}
}
