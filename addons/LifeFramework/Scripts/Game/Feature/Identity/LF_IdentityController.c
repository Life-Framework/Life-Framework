//------------------------------------------------------------------------------------------------
//! The identity card's network path (#902), on the player controller.
//!   server -> owner : LF_ShowIdCardView (the card sheet), LF_AskForIdCard (an officer asks)
//!   owner -> server : LF_InspectIdCard (read your own card), LF_AnswerIdRequest
//! The server identifies the player by this controller, never by a client-sent id, and reads every
//! card itself; a client names a card by RplId and the server checks it carries that card. A
//! listen host's own controller is local on both ends, so it calls the handlers directly.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	protected bool LF_IsLocalIdController()
	{
		return GetGame().GetPlayerController() == this;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the card sheet on this player's machine. Printed values travel as values; the
	//! look is the photo's GUIDs; the shower travels as an RplId so a shown card closes when they
	//! walk off (RplId.Invalid() for your own card).
	void LF_ShowIdCardView(string firstName, string lastName, int age, string number, string issued, string look, bool isVoid, RplId shower, float seconds)
	{
		// An RPC takes at most eight values: the number and the date travel as one.
		string issue = LF_IdCardView.PackIssue(number, issued);
		if (LF_IsLocalIdController())
			RpcDo_LF_ShowIdCardView(firstName, lastName, age, issue, look, isVoid, shower, seconds);
		else
			Rpc(RpcDo_LF_ShowIdCardView, firstName, lastName, age, issue, look, isVoid, shower, seconds);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowIdCardView(string firstName, string lastName, int age, string issue, string look, bool isVoid, RplId shower, float seconds)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "card view not opened: no workspace on this machine");
			return;
		}

		IEntity showerEntity;
		if (shower.IsValid())
			showerEntity = LF_NetworkUtils.FindEntityByRplId(shower);

		string number;
		string issued;
		LF_IdCardView.UnpackIssue(issue, number, issued);
		LF_IdCardView.Open(firstName, lastName, age, number, issued, look, isVoid, showerEntity, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: read the card you carry (inventory Inspect). The server checks you carry it.
	void LF_InspectIdCard(RplId card)
	{
		if (Replication.IsServer())
			RpcAsk_LF_InspectIdCard(card);
		else
			Rpc(RpcAsk_LF_InspectIdCard, card);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_InspectIdCard(RplId cardId)
	{
		int playerId = GetPlayerId();
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		IEntity item = LF_NetworkUtils.FindEntityByRplId(cardId);
		LF_IdCardComponent card;
		if (item)
			card = LF_IdCardComponent.Cast(item.FindComponent(LF_IdCardComponent));

		if (!LF_IdCard.IsOwnerCarrying(character, card))
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, string.Format("inspect refused: player %1 does not carry that card", playerId));
			return;
		}

		LF_IdCard.SendView(playerId, card, null, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer asks this player for their card; their screen asks them.
	void LF_AskForIdCard()
	{
		if (LF_IsLocalIdController())
			RpcDo_LF_AskForIdCard();
		else
			Rpc(RpcDo_LF_AskForIdCard);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_AskForIdCard()
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "ID request not shown: no workspace on this machine");
			return;
		}

		LF_IdRequestSheet.Open();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the answer to an officer's request.
	void LF_AnswerIdRequest(bool show)
	{
		if (Replication.IsServer())
			RpcAsk_LF_AnswerIdRequest(show);
		else
			Rpc(RpcAsk_LF_AnswerIdRequest, show);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_AnswerIdRequest(bool show)
	{
		LF_IdCheck.Answer(GetPlayerId(), show);
	}
}
