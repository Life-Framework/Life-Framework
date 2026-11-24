//------------------------------------------------------------------------------------------------
enum LF_ECargoMode
{
	LOAD,
	UNLOAD
}

//------------------------------------------------------------------------------------------------
//! What a hauler does at the tailgate (#3265), one class with a mode. LOAD shows while the player carries a
//! load the bed takes; UNLOAD shows while the bed holds something and puts the top item on the shoulder. The
//! server does the work and sends a toast on a refusal; the bed on the truck is the state, there is no menu.
class LF_CargoBedAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the bed", "", ParamEnumArray.FromEnum(LF_ECargoMode))]
	protected LF_ECargoMode m_eMode;

	protected LF_CargoBedComponent m_Bed;
	protected LF_ECargoRefusal m_eLastRefusal;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Bed = LF_Component<LF_CargoBedComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Bed || m_Bed.GetCapacity() <= 0)
			return false;

		if (m_eMode == LF_ECargoMode.LOAD)
			return LF_ShoulderCarry.Carried(user) != null;

		return !m_Bed.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! A client check for the offer. The server checks everything again.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Bed)
			return false;

		if (m_eMode == LF_ECargoMode.LOAD)
		{
			IEntity item = LF_ShoulderCarry.Carried(user);
			LF_ShoulderCarryComponent profile = LF_Component<LF_ShoulderCarryComponent>.Find(item);
			if (!profile)
				return ClientRefuse(LF_ECargoRefusal.NO_LOAD, user);

			LF_ECargoRefusal refusal = m_Bed.DecideFor(user, LF_CargoBed.KindIndex(profile.GetKind()));
			if (refusal == LF_ECargoRefusal.NONE)
			{
				m_eLastRefusal = LF_ECargoRefusal.NONE;
				return true;
			}

			return ClientRefuse(refusal, user);
		}

		if (m_Bed.IsLockedFor(user))
			return ClientRefuse(LF_ECargoRefusal.LOCKED, user);

		if (LF_ShoulderCarry.Carried(user))
			return ClientRefuse(LF_ECargoRefusal.NO_HANDS, user);

		m_eLastRefusal = LF_ECargoRefusal.NONE;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The client side of a refusal (#4886): the reason a player reads is always set, and the log names
	//! which check refused, once per change (the offer is evaluated every frame).
	protected bool ClientRefuse(LF_ECargoRefusal refusal, IEntity user)
	{
		SetCannotPerformReason(LF_CargoBed.RefusalKey(refusal));
		if (refusal != m_eLastRefusal)
		{
			m_eLastRefusal = refusal;
			float away = -1;
			if (user && m_Bed)
				away = vector.Distance(user.GetOrigin(), m_Bed.GetTailgateWorld());

			LF_Debug.Info(LF_CargoBed.FEATURE, string.Format("%1 offer refused on the client: %2 (%3 m from the tailgate)", typename.EnumToString(LF_ECargoMode, m_eMode), LF_CargoBed.RefusalText(refusal), away));
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eMode == LF_ECargoMode.LOAD)
		{
			outName = "#LF-Cargo_Load";
			return true;
		}

		outName = "#LF-Cargo_Unload";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Bed || !pUserEntity)
		{
			LF_Debug.Error(LF_CargoBed.FEATURE, "cargo action performed without a bed or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		LF_ECargoRefusal result;
		if (m_eMode == LF_ECargoMode.LOAD)
			result = m_Bed.ServerLoad(pUserEntity);
		else
			result = m_Bed.ServerUnload(pUserEntity);

		if (result == LF_ECargoRefusal.NONE)
			return;

		string key = LF_CargoBed.RefusalKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Cargo_Title", 4.0);
	}
}
