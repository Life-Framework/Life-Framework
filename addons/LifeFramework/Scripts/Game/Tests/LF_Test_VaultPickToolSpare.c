// red-proof: Observed on main with `tools/cli test --no-build --only LF_Test_VaultPickToolSpare`: a drill in the pack and a charge at a drill,charge vault picked "drill" (`- a spare pack drill does not block the charge`).
// tier: WORLD
//! A drill only in the pack never blocks the charge on a drill-or-charge vault (#2581): the robber
//! with a spare drill and a charge is sent to the charge; once the drill is in the hands it is the
//! drill. A drill-only vault still points a pack drill at "hold it".
class LF_Test_VaultPickToolSpare : LF_Test
{
	override string GetName()
	{
		return "crime/vault-picktool-spare";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity drill;
		IEntity robber = LF_TestVaultDrillFixture.Robber(ctx, drill);
		if (!robber)
			return;

		// A bare state: PickTool reads only the tool list, so no registered vault is touched.
		LF_BankVaultState state = new LF_BankVaultState();
		state.m_sVaultId = "vault-picktool-spare";
		state.m_sTools = "drill,charge";

		IEntity item;
		ctx.True(LF_Bank.PickTool(state, robber, item) == LF_Bank.TOOL_DRILL, "a spare drill alone: still the drill, with the hold hint");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(robber, LF_Breach.FENCE_CHARGE_PREFAB, 1), "a charge lands in the robber's inventory");
		string pickedSpare = LF_Bank.PickTool(state, robber, item);
		ctx.True(pickedSpare == LF_Bank.TOOL_CHARGE, "a spare pack drill does not block the charge");

		if (!RedProof())
			LF_TestVaultDrillFixture.Hold(robber, drill, true);

		string pickedHeld = LF_Bank.PickTool(state, robber, item);
		ctx.True(pickedHeld == LF_Bank.TOOL_DRILL, "the drill in the hands is the drill");
		ctx.True(item == drill, "and it is the held drill");

		LF_TestVaultDrillFixture.Hold(robber, drill, false);
		ctx.True(LF_Bank.PickTool(state, robber, item) == LF_Bank.TOOL_CHARGE, "drill back in the pack: the charge again");
	}
}
