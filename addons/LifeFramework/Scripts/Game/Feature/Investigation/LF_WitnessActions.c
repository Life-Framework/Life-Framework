//------------------------------------------------------------------------------------------------
//! Payphone entry: "Describe the suspect's clothing / vehicle / plate". One action per thing seen
//! (a phone carries one entry per preset). The server finds the caller's own witness record; the
//! client names only the field. What the caller could see comes from the server (LF_Witness).
class LF_WitnessPayphoneAction : ScriptedUserAction
{
	[Attribute("clothing", UIWidgets.EditBox, "What this entry reports: clothing, vehicle or plate")]
	protected string m_sField;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		LF_EWitnessField field;
		if (!ParseField(m_sField, field) || uid.IsEmpty())
		{
			LF_Debug.Error(LF_Witness.FEATURE, string.Format("payphone witness action has bad field '%1'", m_sField));
			return;
		}

		LF_EWitnessResult result = LF_Witness.Give(LF_Witness.SceneFor(uid, System.GetTickCount()), uid, field, "payphone");
		LF_Utils.NotifyEntity(pUserEntity, RefusalKey(result), "#LF-Payphone_Title", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	static bool ParseField(string name, out LF_EWitnessField field)
	{
		if (name == "clothing")
			field = LF_EWitnessField.CLOTHING;
		else if (name == "vehicle")
			field = LF_EWitnessField.VEHICLE;
		else if (name == "plate")
			field = LF_EWitnessField.PLATE;
		else
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Toast key for a result; OK reads as the statement being taken down.
	static string RefusalKey(LF_EWitnessResult result)
	{
		switch (result)
		{
			case LF_EWitnessResult.OK:
				return "#LF-Witness_Noted";
			case LF_EWitnessResult.ALREADY_GIVEN:
				return "#LF-Witness_AlreadyGiven";
			case LF_EWitnessResult.NOT_SEEN:
				return "#LF-Witness_NotSeen";
		}

		return "#LF-Witness_NothingSeen";
	}
}
