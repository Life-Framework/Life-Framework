// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_CharacterSwitchOffer`: RED-OK, with the hook skipping NoteOffer it failed `- a pick at the desk soon after the sheet counts`; green without it.
//! Review of #3509: the switch sheet's offer is good on the server only for a short while and only
//! where it was sent (the clerk's desk). Before, the offer stayed until a cancel or a pick, so a
//! client that never sent the cancel could pick anywhere later and vanish with no logout body.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CharacterSwitchOffer : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "character/switch-offer";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CharacterSwitch.ResetStatics();
		int playerId = 9301;
		vector desk = "100 0 200";
		vector nearDesk = "103 0 204";
		vector farAway = "900 0 200";

		ctx.False(LF_CharacterSwitch.OfferHolds(playerId, 20, desk), "no pick without a sheet sent");

		if (!RedProof())
			LF_CharacterSwitch.NoteOffer(playerId, 10, desk);
		ctx.True(LF_CharacterSwitch.OfferHolds(playerId, 20, nearDesk), "a pick at the desk soon after the sheet counts");

		ctx.False(LF_CharacterSwitch.OfferHolds(playerId, 10 + LF_CharacterSwitch.OFFER_SECONDS + 1, nearDesk), "a sheet kept open past the window counts for nothing");
		ctx.False(LF_CharacterSwitch.OfferHolds(playerId, 20, farAway), "a pick far from the clerk counts for nothing");

		LF_CharacterSwitch.Forget(playerId);
		ctx.False(LF_CharacterSwitch.OfferHolds(playerId, 20, desk), "a leaver's sheet goes with the connection");

		LF_CharacterSwitch.ResetStatics();
	}
}
