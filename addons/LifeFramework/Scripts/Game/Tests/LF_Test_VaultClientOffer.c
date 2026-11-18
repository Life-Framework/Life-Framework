// red-proof: RED-OK crime/vault-client-offer (tools/cli test --no-build --red LF_Test_VaultClientOffer, 2026-11-18: "expected '', got 'not asked'"). The client bug itself was seen red in a dedicated session (agent-box/proofs/vault-offer-proof.mjs: every step read "This vault cannot be robbed").
//! The vault offer a dedicated client draws (#2418). The vault table is server-only, so a remote
//! client's prompt read "This vault cannot be robbed" on every vault (seen in a dedicated session,
//! agent-box/proofs/vault-offer-proof.mjs). The client now answers from what replicates, in the
//! server's order: faction, police, an open or armed door, the tool, the drill in the hands, a driller.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VaultClientOffer : LF_Test
{
	override string GetName()
	{
		return "crime/vault-client-offer";
	}

	override void Run(LF_TestContext ctx)
	{
		string drill = LF_Bank.TOOL_DRILL;
		string charge = LF_Bank.TOOL_CHARGE;

		string open = "not asked";
		if (!RedProof())
			open = LF_Bank.ClientReason(true, 0, 0, false, true, drill, true, false);
		ctx.EqualStr("", open, "a civilian with the drill in the hands may drill");

		ctx.EqualStr("", LF_Bank.ClientReason(true, 0, 0, false, true, charge, false, false), "a charge in the pack may be set");
		ctx.EqualStr("", LF_Bank.ClientReason(true, 0, 0, false, false, "", false, false), "a vault that needs no tool is open to a civilian");
		ctx.EqualStr("#EL-Bank_CiviliansOnly", LF_Bank.ClientReason(false, 0, 0, false, true, drill, true, false), "police may not rob");
		ctx.EqualStr("#EL-Bank_NeedPolice", LF_Bank.ClientReason(true, 1, 2, false, true, drill, true, false), "too few police on duty");
		ctx.EqualStr("#EL-Breach_AlreadyOpen", LF_Bank.ClientReason(true, 0, 0, true, true, drill, true, false), "an open or armed door");
		ctx.EqualStr("#LF-Bank_NeedTool", LF_Bank.ClientReason(true, 0, 0, false, true, "", false, false), "no tool");
		ctx.EqualStr("#LF-Bank_HoldDrill", LF_Bank.ClientReason(true, 0, 0, false, true, drill, false, false), "a pack drill points at the hands");
		ctx.EqualStr("#LF-Bank_DrillBusy", LF_Bank.ClientReason(true, 0, 0, false, true, drill, true, true), "someone else is drilling");
	}
}
