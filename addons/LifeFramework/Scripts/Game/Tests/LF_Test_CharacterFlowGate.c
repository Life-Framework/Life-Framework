// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_CharacterFlowGate` on the unfixed code: FAIL "a player the server is not arriving is refused the arrival flow" (got "", expected "#LF-CharSel_Refused"); green with the gate.
// tier: LOGIC
//! character/flow-gate (#2413): the quiz's Continue and the registration form re-enter the arrival
//! flow, which offers the character pick or a fresh spawn. Only a player the server is arriving
//! (flow opened on connect, closed on spawn) may reach it, so a dead, downed, cuffed or jailed
//! player cannot switch character or skip the respawn bookkeeping by sending the RPC.
class LF_Test_CharacterFlowGate : LF_Test
{
	protected static const int PLAYER = 7781;
	protected const string REFUSED = "#LF-CharSel_Refused";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "character/flow-gate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr(REFUSED, LF_CharacterCreationRules.RefuseFlowRequest(false), "a player the server is not arriving is refused the arrival flow");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseFlowRequest(true), "an arriving player continues the flow");

		LF_CharacterCreationManager manager = LF_CharacterCreationManager.GetInstance();
		ctx.EqualStr(REFUSED, manager.RefuseFlowRequest(PLAYER), "a player with no open flow (never connected, or already playing) is refused");

		manager.OpenFlow(PLAYER);
		ctx.EqualStr("", manager.RefuseFlowRequest(PLAYER), "an open flow lets the arrival through");

		// The spawn that ends the flow closes it: a body that dies later cannot re-enter it.
		if (!RedProof())
			manager.CloseFlow(PLAYER);

		ctx.EqualStr(REFUSED, manager.RefuseFlowRequest(PLAYER), "once spawned, the player is refused the flow again");

		// A disconnect forgets it too: ids are reused by the next arrival.
		manager.OpenFlow(PLAYER);
		if (!RedProof())
			LF_CharacterCreationManager.ForgetPlayer(PLAYER);

		ctx.EqualStr(REFUSED, manager.RefuseFlowRequest(PLAYER), "a disconnect forgets the open flow");
		ctx.False(manager.HasPlayerState(PLAYER), "no flow state outlives the connection");
	}
}
