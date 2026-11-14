// red-proof: 2026-11-14 `tools\cli test --no-build --only LF_Test_TestOrgSignWorld` with the expected owner name changed to Nobody failed `the door knows its owner's name (expected 'Nobody', got 'Test Organisation')`. Restored; green.
// tier: WORLD
//! Test org sign (#934): the sign prefab offers Join and Leave beside its Read card, the card
//! resolves, and a hideout door learns the name of the organisation that holds it.
class LF_Test_TestOrgSignWorld : LF_Test
{
	static const ResourceName SIGN_PREFAB = "{256EE1573D428340}Prefabs/Props/Signs/LF_Sign_TestOrg.et";

	override string GetName()
	{
		return "organizations/test-org-sign-world";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity sign = LF_Utils.SpawnEntityPrefab(SIGN_PREFAB, "0 0 0");
		ctx.NotNull(sign, "the test org sign prefab spawns");
		if (!sign)
			return;

		ActionsManagerComponent manager = LF_Component<ActionsManagerComponent>.Find(sign);
		ctx.NotNull(manager, "the sign carries an action manager");
		if (!manager)
			return;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		int joins;
		int leaves;
		int reads;
		foreach (BaseUserAction action : actions)
		{
			LF_TestOrgSignAction orgAction = LF_TestOrgSignAction.Cast(action);
			if (orgAction)
			{
				string name;
				orgAction.GetActionNameScript(name);
				if (name == "#LF-TestOrgSign_Join")
					joins++;
				else if (name == "#LF-TestOrgSign_Leave")
					leaves++;
			}

			if (LF_SignReadAction.Cast(action))
				reads++;
		}

		ctx.Equal(1, joins, "the sign offers Join test organisation once");
		ctx.Equal(1, leaves, "the sign offers Leave organisation once");
		ctx.Equal(1, reads, "the sign keeps its Read card");
		SCR_EntityHelper.DeleteEntityAndChildren(sign);

		ctx.True(LF_SignCatalog.Find("test_org") != null, "the test_org card resolves");

		// A hideout door says whose it is: the owner's name reaches the door component.
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_DOOR);
		ctx.NotNull(door, LF_TestAnnex.HIDEOUT_DOOR + " is placed");
		LF_HideoutDoorComponent hideoutDoor;
		if (door)
			hideoutDoor = LF_Component<LF_HideoutDoorComponent>.Find(door);

		ctx.NotNull(hideoutDoor, "the hideout door carries LF_HideoutDoorComponent");
		if (!hideoutDoor)
			return;

		ctx.EqualStr("", hideoutDoor.GetOwnerName(), "an unowned door has no owner name");
		LF_Organization org = LF_OrganizationManager.Create("test-org-sign-world-founder", LF_TestOrgRules.ORG_NAME);
		ctx.NotNull(org, "the test organisation is founded");
		if (!org)
			return;

		ctx.True(LF_Hideout.Assign(LF_TestAnnex.HIDEOUT_ID, org.GetId()), "the organisation takes the annex hideout");
		ctx.EqualStr(LF_TestOrgRules.ORG_NAME, hideoutDoor.GetOwnerName(), "the door knows its owner's name");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}
