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
				return false;

			LF_ECargoRefusal refusal = m_Bed.DecideFor(user, LF_CargoBed.KindIndex(profile.GetKind()));
			if (refusal == LF_ECargoRefusal.NONE)
				return true;

			SetCannotPerformReason(LF_CargoBed.RefusalKey(refusal));
			return false;
		}

		if (LF_ShoulderCarry.Carried(user))
		{
			SetCannotPerformReason("#LF-Cargo_HandsFull");
			return false;
		}

		return true;
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
