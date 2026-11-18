//------------------------------------------------------------------------------------------------
//! Call a service on a carried radio set (#3155): one action per service on the item's actions.
//! Shown only to the carrier; the server checks the carrier, the route and the fee again
//! (LF_HandsetCall.ServerCall). The fee is the prefab's, read on the server.
class LF_HandsetCallAction : ScriptedUserAction
{
	[Attribute("taxi", UIWidgets.EditBox, "Service key from Configs/Comms/LF_ServiceCalls.conf")]
	protected string m_sService;

	[Attribute("5", UIWidgets.Auto, "Cash one call takes from the carrier")]
	protected int m_iFee;

	//------------------------------------------------------------------------------------------------
	//! The Comms system is off (#1076), or the radio is not the user's: no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Comms"))
			return false;

		return LF_HandsetCall.IsCarriedBy(GetOwner(), user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (LF_MoneyUtils.GetCash(user) < m_iFee)
		{
			SetCannotPerformReason("#LF-Handset_NoCash");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Comms", "LF_HandsetCallAction"))
			return;

		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_HandsetCall.FEATURE, "radio call performed without a radio or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		LF_HandsetCall.ServerCall(pUserEntity, pOwnerEntity, m_sService, m_iFee);
	}
}
