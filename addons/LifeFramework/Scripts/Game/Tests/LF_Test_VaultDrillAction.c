// red-proof: 2026-11-15 `tools/cli test --no-build --red LF_Test_HandToolEquip,LF_Test_VaultDrillAction,LF_Test_VaultDrillCancel,LF_Test_VaultDrillPayOnce` RED-OK, hook on failed `- drill in the hands: Drill the vault door`, `- drill in the hands: the gate passes`.
// tier: WORLD
//! The vault door offers the drill while it is held: on the annex vault the rob action is listed,
//! reads "Drill the vault door" with the drill in the hands and passes its gate; with the drill
//! only in the pack it reads the vault's rob name and the gate asks to hold the drill.
class LF_Test_VaultDrillAction : LF_Test
{
	override string GetName()
	{
		return "crime/vault-drill-action";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(door, "the annex vault door is placed");
		LF_BankVaultComponent vault = LF_Component<LF_BankVaultComponent>.Find(door);
		ctx.NotNull(vault, "it is a vault");
		if (!door || !vault)
			return;

		LF_BankRobAction action;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(door);
		array<BaseUserAction> list = {};
		if (actions)
			actions.GetActionsList(list);

		foreach (BaseUserAction listed : list)
		{
			if (LF_BankRobAction.Cast(listed))
				action = LF_BankRobAction.Cast(listed);
		}

		ctx.NotNull(action, "the door lists the vault action");

		IEntity drill;
		IEntity robber = LF_TestVaultDrillFixture.Robber(ctx, drill);
		if (!robber || !action)
			return;

		string uid = "vault-drill-action";
		ctx.True(action.CanBeShownScript(robber), "the action shows on the door");
		ctx.True(LF_BankRobAction.NameFor(vault, robber) == vault.GetActionName(), "drill in the pack: the plain rob name");
		ctx.True(LF_Bank.GateReason(vault.GetVaultId(), uid, robber) == "#LF-Bank_HoldDrill", "drill in the pack: hold it first");

		if (!RedProof())
			LF_TestVaultDrillFixture.Hold(robber, drill, true);

		ctx.True(LF_BankRobAction.NameFor(vault, robber) == "#LF-Bank_Drill", "drill in the hands: Drill the vault door");
		ctx.True(LF_Bank.GateReason(vault.GetVaultId(), uid, robber) == "", "drill in the hands: the gate passes");
	}
}
