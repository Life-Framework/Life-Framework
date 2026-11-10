//------------------------------------------------------------------------------------------------
enum LF_EChainMode
{
	LOAD,
	WORK,
	COLLECT
}

//------------------------------------------------------------------------------------------------
//! The three things a worker does at a hands-on station (#417), one class with a mode so a barrel,
//! a still and a press share it. LOAD puts a batch in, WORK is a held action that adds one unit of
//! work (a gesture while held, a sound when it lands), COLLECT takes the product out. There is no
//! menu: the offer changes with the station's replicated state, and the WORK prompt says in words
//! how far along the batch is.
class LF_ChainStationAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the station", "", ParamEnumArray.FromEnum(LF_EChainMode))]
	protected LF_EChainMode m_eMode;

	[Attribute("SOUND_PICK_UP", UIWidgets.EditBox, "Sound event on the station's SCR_SoundDataComponent, played for everyone nearby when the action lands")]
	protected string m_sSound;

	protected LF_ChainStationComponent m_Station;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Station = LF_Component<LF_ChainStationComponent>.Find(pOwnerEntity);
		SetCannotPerformReason("#LF-Chain_MissingInputs");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Station)
			return false;

		int percent = m_Station.GetPercent();
		if (m_eMode == LF_EChainMode.LOAD)
			return percent == LF_StationProgress.EMPTY;

		if (m_eMode == LF_EChainMode.WORK)
			return percent >= 0 && percent < LF_StationProgress.READY;

		return percent == LF_StationProgress.READY;
	}

	//------------------------------------------------------------------------------------------------
	//! A client check for the offer. The server checks everything again.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Station || m_eMode != LF_EChainMode.LOAD)
			return true;

		LF_ChainStepDef step = LF_ChainCatalog.FindStep(m_Station.GetChainId(), m_Station.GetStepId());
		if (!step)
			return false;

		InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(user);
		foreach (LF_ProcessingInput input : step.m_aInputs)
		{
			if (LF_InventoryUtils.GetAmount(inventory, input.m_InputPrefab) < input.m_iInputAmount)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The reach-and-work gesture starts when the hold starts, on the acting client only.
	override void OnActionStart(IEntity pUserEntity)
	{
		if (m_eMode != LF_EChainMode.WORK || !pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		PlayGesture(pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayGesture(IEntity user)
	{
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(user.FindComponent(CharacterControllerComponent));
		if (controller && controller.CanPlayItemGesture())
			controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Station || !pUserEntity)
		{
			LF_Debug.Error(LF_ChainSites.FEATURE, "station action performed without a station or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		LF_EChainStation result;
		if (m_eMode == LF_EChainMode.LOAD)
			result = m_Station.ServerLoad(pUserEntity, now);
		else if (m_eMode == LF_EChainMode.WORK)
			result = m_Station.ServerWork(pUserEntity, now);
		else
			result = m_Station.ServerCollect(pUserEntity, now);

		if (result != LF_EChainStation.OK)
		{
			string key = LF_ChainStationComponent.RefusalKey(result);
			LF_Debug.Info(LF_ChainSites.FEATURE, string.Format("station %1 refused %2 (%3)", m_Station.GetStationKey(), typename.EnumToString(LF_EChainMode, m_eMode), typename.EnumToString(LF_EChainStation, result)));
			if (!key.IsEmpty())
				LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Chain_Title", 4.0);
			return;
		}

		if (m_sSound.IsEmpty())
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (character)
			character.LF_BroadcastStationSound("LF_ChainStationAction", m_sSound);
	}

	//------------------------------------------------------------------------------------------------
	//! The state of the batch in words, so the barrel reads at a glance without a bar.
	override bool GetActionNameScript(out string outName)
	{
		if (m_eMode != LF_EChainMode.WORK || !m_Station)
			return false;

		int percent = m_Station.GetPercent();
		if (percent < 33)
			outName = "#LF-Chain_Work_Fresh";
		else if (percent < 80)
			outName = "#LF-Chain_Work_Mid";
		else
			outName = "#LF-Chain_Work_Late";

		return true;
	}
}
