// red-proof: 2026-11-15 `tools/cli test --no-build --red LF_Test_HandToolEquip,LF_Test_VaultDrillAction,LF_Test_VaultDrillCancel,LF_Test_VaultDrillPayOnce` RED-OK, hook on failed `- in the hands it is held`, `- the held tool is that drill`.
// tier: WORLD
//! The drill is a hand tool (core LF_HandTool): it carries the gadget component with tool id drill,
//! can be held, goes into a 1-0 quick slot, and once in the hands it reads as held; in the pack it
//! does not.
class LF_Test_HandToolEquip : LF_Test
{
	override string GetName()
	{
		return "handtool/equip";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity drill;
		IEntity robber = LF_TestVaultDrillFixture.Robber(ctx, drill);
		if (!robber)
			return;

		LF_HandToolComponent tool = LF_HandTool.Of(drill);
		ctx.NotNull(tool, "the drill carries LF_HandToolComponent");
		if (!tool)
			return;

		ctx.True(tool.GetToolId() == "drill", "its tool id is drill");
		ctx.True(tool.CanBeHeld(), "it can be held in the hands");
		ctx.True(tool.GetType() == EGadgetType.SPECIALIST_ITEM, "it is a tool gadget like the wrench");
		ctx.True(LF_HandTool.QuickSlot(robber, drill) >= 0, "it goes into a 1-0 quick slot");
		ctx.False(LF_HandTool.Holds(robber, "drill"), "in the pack it is not held");
		ctx.True(LF_HandTool.Equip(robber, drill), "the quick slot equip is accepted");

		if (!RedProof())
			LF_TestVaultDrillFixture.Hold(robber, drill, true);

		ctx.True(LF_HandTool.Holds(robber, "drill"), "in the hands it is held");
		ctx.True(LF_HandTool.HeldTool(robber, "drill") == drill, "the held tool is that drill");
		ctx.False(LF_HandTool.Holds(robber, "parcel"), "another tool id is not held");
		ctx.False(LF_HandTool.Matches("drill", EGadgetMode.IN_SLOT, "drill"), "a slotted tool is not held");
	}
}
