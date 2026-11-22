// LF_BusTravelClient.c - the bus requests and answers on the player's own controller, and the fade.
// Client -> server: RpcAsk_LF_BusTrip (stop ids, not entities: a sign needs no RplComponent) and
// RpcAsk_LF_BusCancel (take a waiting ticket back).
// Server -> owner: RpcDo_LF_BusResult (the ticket's answer), RpcDo_LF_BusDone (the wait is over:
// arrived or cancelled) and RpcDo_LF_BusFade (to and from black).

class LF_BusTravelClient
{
	//------------------------------------------------------------------------------------------------
	//! Server: fades a passenger's screen out (toBlack) or back in. No player (a test): nothing to do.
	static void SendFade(int playerId, bool toBlack, float seconds)
	{
		SCR_PlayerController controller = ControllerOf(playerId);
		if (controller)
			controller.LF_SendBusFade(toBlack, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells a passenger's machine the wait is over, so its stop actions go back to the list.
	static void SendDone(int playerId, bool arrived)
	{
		SCR_PlayerController controller = ControllerOf(playerId);
		if (controller)
			controller.LF_SendBusDone(arrived);
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController ControllerOf(int playerId)
	{
		if (playerId <= 0 || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
			LF_Debug.Warn(LF_BusStop.FEATURE, string.Format("bus message not sent: no controller for player %1", playerId));

		return controller;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the vanilla fast-travel fade (SCR_FadeInOutEffect), so it looks like the game's own.
	static void Fade(bool toBlack, float seconds)
	{
		SCR_ScreenEffectsManager manager = SCR_ScreenEffectsManager.GetScreenEffectsDisplay();
		if (!manager)
			return;

		SCR_FadeInOutEffect fade = SCR_FadeInOutEffect.Cast(manager.GetEffect(SCR_FadeInOutEffect));
		if (fade)
			fade.FadeOutEffect(toBlack, seconds);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for a ticket from one stop to another.
	void LF_AskBusTrip(string fromId, string toId)
	{
		Rpc(RpcAsk_LF_BusTrip, fromId, toId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: take the waiting ticket back (refunded).
	void LF_AskBusCancel()
	{
		Rpc(RpcAsk_LF_BusCancel);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the body must stand at the stop and pass every rule; the fare is the server's.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BusTrip(string fromId, string toId)
	{
		IEntity body = GetControlledEntity();
		int playerId = GetPlayerId();
		LF_EBusRefusal refusal = LF_BusStop.Board(playerId, body, LF_Utils.GetPlayerUID(playerId), fromId, toId);
		int fare = 0;
		int wait = 0;
		LF_BusTrip trip = LF_BusStop.TripOf(playerId, "");
		if (refusal == LF_EBusRefusal.NONE && trip)
		{
			fare = trip.m_iFare;
			wait = Math.Round(trip.m_fDepartAt - trip.m_fBoughtAt);
		}

		Rpc(RpcDo_LF_BusResult, refusal, fare, wait);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the trip found by the player's id; the refund goes through the same cancel as a walk-off.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BusCancel()
	{
		int playerId = GetPlayerId();
		if (!LF_BusStop.CancelAsked(playerId, LF_Utils.GetPlayerUID(playerId)))
			Rpc(RpcDo_LF_BusDone, false);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_BusResult(int refusal, int fare, int waitSeconds)
	{
		LF_BusTicket.OnResult(refusal, fare, waitSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: this player's wait is over.
	void LF_SendBusDone(bool arrived)
	{
		Rpc(RpcDo_LF_BusDone, arrived);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_BusDone(bool arrived)
	{
		LF_BusTicket.OnDone(arrived);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fade this player's screen.
	void LF_SendBusFade(bool toBlack, float seconds)
	{
		Rpc(RpcDo_LF_BusFade, toBlack, seconds);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_BusFade(bool toBlack, float seconds)
	{
		LF_BusTravelClient.Fade(toBlack, seconds);
	}
}
