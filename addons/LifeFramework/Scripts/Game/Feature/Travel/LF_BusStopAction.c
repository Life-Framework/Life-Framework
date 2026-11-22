// LF_BusStopAction.c - the bus in the sign's own action list. Each LF_BusStopAction stands for one
// other stop by its slot (0 the nearest): "Bus to Harbour: $18, 20 s"; F buys the ticket and starts
// the wait. While a ticket waits the destinations hide and LF_BusCancelAction offers the refund. The
// ticket itself is a server request that checks everything again (LF_BusTicket, LF_BusStop.Board).

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_BusStopAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Which destination this action offers: 0 the nearest other stop, 1 the next, ... (LF_BusTicket.MAX_SLOTS)")]
	protected int m_iSlot;

	//------------------------------------------------------------------------------------------------
	protected LF_BusOffer Offer()
	{
		return LF_BusTicket.OfferAt(GetOwner(), m_iSlot);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_BusStop.SYSTEM) || LF_BusTicket.IsWaiting())
			return false;

		return Offer() != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_BusOffer offer = Offer();
		if (!offer)
			return false;

		LF_EBusRefusal refusal = LF_BusTicket.LocalRefusal(user, offer);
		if (refusal == LF_EBusRefusal.NONE)
			return true;

		SetCannotPerformReason(LF_Terms.Text(LF_BusTicket.ReasonKey(refusal)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_BusOffer offer = Offer();
		if (!offer)
		{
			outName = "#LF-BusStop_Action";
			return true;
		}

		outName = LF_BusTicket.ActionName(offer);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ask goes from the player's own machine.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus stop action performed without a stop or a user");
			return;
		}

		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
			return;

		LF_BusOffer offer = Offer();
		if (!offer)
		{
			LF_Debug.Warn(LF_BusStop.FEATURE, string.Format("bus stop action slot %1 has no destination", m_iSlot));
			return;
		}

		LF_BusTicket.Ask(pOwnerEntity, offer);
	}
}

//------------------------------------------------------------------------------------------------
//! "Cancel trip (refund $18): bus in 14 s", shown only while this player's ticket waits.
[BaseContainerProps(configRoot: true)]
class LF_BusCancelAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_BusStop.SYSTEM) && LF_BusTicket.IsWaiting();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = LF_BusTicket.CancelName(LF_BusTicket.WaitingFare(), LF_BusTicket.SecondsLeft());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pUserEntity || !LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
			return;

		LF_BusTicket.AskCancel();
	}
}
