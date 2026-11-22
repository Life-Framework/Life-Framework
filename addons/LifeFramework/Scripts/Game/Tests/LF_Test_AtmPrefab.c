// red-proof: 2026-11-19, cli test --no-build --red LF_Test_AtmPrefab: RED-OK money/atm-prefab (fails with the hook on, 3 assertions; the spawn is skipped), then --only passed=2 failed=0.
//! money/atm-prefab: the ATM is a placeable prop (#3352). Spawned from LF_ATM.et it carries LF_ATMAction and
//! the per-part quick lines (cash slot, deposit drawer, screen),
//! registers with LF_Places under the kind "ATM", has a mesh that resolves, and leaves no place behind once deleted.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_AtmPrefab : LF_Test
{
	protected static const ResourceName PREFAB = "{7070D89FFB6DC880}Prefabs/Props/Money/LF_ATM.et";
	protected static const string PLACE_KIND = "ATM";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/atm-prefab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(anchor, "the annex vault is placed, to spawn the ATM near");
		if (!anchor)
			return;

		array<IEntity> before = {};
		int placedBefore = LF_Places.FindAll(PLACE_KIND, before);

		IEntity atm = null;
		if (!RedProof())
			atm = LF_Utils.SpawnEntityPrefab(PREFAB, anchor.GetOrigin() + "0 -20 0", "0 0 0", false);

		ctx.NotNull(atm, "the ATM prefab spawns");
		if (!atm)
			return;

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(atm.FindComponent(ActionsManagerComponent));
		ctx.NotNull(actions, "the prefab carries an ActionsManagerComponent");
		bool hasAction = false;
		int withdrawLines = 0;
		int depositLines = 0;
		int balanceLines = 0;
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction a : list)
			{
				if (LF_ATMAction.Cast(a))
					hasAction = true;

				LF_ATMQuickAction quick = LF_ATMQuickAction.Cast(a);
				if (!quick)
					continue;

				if (quick.GetKind() == LF_EATMQuickKind.WITHDRAW)
					withdrawLines++;
				else if (quick.GetKind() == LF_EATMQuickKind.DEPOSIT)
					depositLines++;
				else
					balanceLines++;
			}
		}
		ctx.True(hasAction, "the prefab carries LF_ATMAction");
		ctx.Equal(5, withdrawLines, "the cash slot offers five withdraw lines ($20, $50, $100, $500, all)");
		ctx.Equal(4, depositLines, "the deposit drawer offers four deposit lines (all, $50, $100, $500)");
		ctx.Equal(1, balanceLines, "the screen offers Check balance");
		ctx.True(atm.FindComponent(RplComponent) != null, "the prefab carries an RplComponent");
		ctx.True(atm.GetVObject() != null, "the ATM mesh resolves");

		array<IEntity> after = {};
		ctx.Equal(placedBefore + 1, LF_Places.FindAll(PLACE_KIND, after), "the spawned ATM registers one place of kind ATM");
		ctx.True(LF_Places.Find(PLACE_KIND, "atm") == atm, "the prefab's default id is atm (a town fit sets its own id per machine)");
		ctx.True(after.Find(atm) != -1, "LF_Places lists the spawned ATM under the kind ATM");

		SCR_EntityHelper.DeleteEntityAndChildren(atm);
		array<IEntity> gone = {};
		ctx.Equal(placedBefore, LF_Places.FindAll(PLACE_KIND, gone), "a deleted ATM is no longer a place");
	}
}
