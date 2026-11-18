//------------------------------------------------------------------------------------------------
enum LF_ELaunderMode
{
	LOAD,
	COLLECT
}

//------------------------------------------------------------------------------------------------
//! The two things a character does at the laundering station (#2853): LOAD hands over marked notes,
//! COLLECT takes the clean cash out once the batch is washed. The offer follows the station's
//! replicated state; the server checks everything again and says a refusal in words.
class LF_LaunderStationAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the station", "", ParamEnumArray.FromEnum(LF_ELaunderMode))]
	protected LF_ELaunderMode m_eMode;

	protected LF_LaunderStationComponent m_Station;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Station = LF_Component<LF_LaunderStationComponent>.Find(pOwnerEntity);
		SetCannotPerformReason("#LF-Launder_NoNotes");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Station)
			return false;

		int state = m_Station.GetReplicatedState();
		if (m_eMode == LF_ELaunderMode.LOAD)
			return state == LF_LaunderStationComponent.STATE_EMPTY;

		return state != LF_LaunderStationComponent.STATE_EMPTY;
	}

	//------------------------------------------------------------------------------------------------
	//! Client hint only: loading needs marked notes on you.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Station || m_eMode != LF_ELaunderMode.LOAD)
			return true;

		return LF_MarkedCash.Count(user) > 0;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Station || !pUserEntity)
		{
			LF_Debug.Error(LF_LaunderStationComponent.FEATURE, "launder action performed without a station or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		LF_ELaunderStation result;
		if (m_eMode == LF_ELaunderMode.LOAD)
			result = m_Station.ServerLoad(pUserEntity, now);
		else
			result = m_Station.ServerCollect(pUserEntity, now);

		string key = "#LF-Launder_Loaded";
		if (m_eMode == LF_ELaunderMode.COLLECT)
			key = "#LF-Launder_Collected";

		if (result != LF_ELaunderStation.OK)
		{
			key = LF_LaunderStationComponent.RefusalKey(result);
			LF_Debug.Info(LF_LaunderStationComponent.FEATURE, string.Format("launder %1 refused (%2)", typename.EnumToString(LF_ELaunderMode, m_eMode), typename.EnumToString(LF_ELaunderStation, result)));
		}

		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Launder_Title", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! COLLECT says in words whether the batch is still washing.
	override bool GetActionNameScript(out string outName)
	{
		if (m_eMode != LF_ELaunderMode.COLLECT || !m_Station)
			return false;

		if (m_Station.GetReplicatedState() == LF_LaunderStationComponent.STATE_READY)
			outName = "#LF-Launder_Collect";
		else
			outName = "#LF-Launder_Washing";

		return true;
	}
}
