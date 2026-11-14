// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_FuelClerkDialogue`: made
// LF_DialogueHook_RobArmed.Allows return true; got `[LFTEST]   - an unarmed character is not offered
// the robbery`. Restored the weapon and faction check; green.
// tier: WORLD
//! The fuel attendant (#936): its generated conversation loads with every hook it names, the
//! robbery reply is offered only under the armed condition, every line the robbery says outside
//! the conversation is voiced, the attendant finds the fuel till behind its counter, that till is
//! robbed only through the attendant (its world action hidden and refused), and an unarmed
//! character is refused before anything is reported. The test spawns its own attendant and till,
//! the prefabs the fuel station places, on an empty annex spot.
class LF_Test_FuelClerkDialogue : LF_Test
{
	static const ResourceName CLERK = "{9FF2685A41D86574}Prefabs/Characters/Npcs/LF_FuelClerk.et";
	static const ResourceName TILL = "{9D3A51E060EFE982}Prefabs/Props/Commercial/LF_FuelTill_Attended.et";
	static const vector SPOT = "74 0 74";

	override string GetName()
	{
		return "crime/fuel-clerk-dialogue";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity tillEntity = LF_Utils.SpawnEntityPrefab(TILL, SPOT + "0 1 0");
		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK, SPOT + "0 0 1");
		ctx.True(clerk != null, "the fuel attendant prefab spawns");
		ctx.True(tillEntity != null, "the attended till prefab spawns");
		if (clerk && tillEntity)
			Check(ctx, clerk, tillEntity);

		if (clerk)
			SCR_EntityHelper.DeleteEntityAndChildren(clerk);

		if (tillEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(tillEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity clerk, IEntity tillEntity)
	{
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(clerk);
		ctx.True(config != null, "the attendant has a playable dialogue");
		if (!config)
			return;

		ctx.EqualStr("fuel_clerk", config.m_sId, "it is the attendant's dialogue");
		Resource project = Resource.Load(config.m_sSoundProject);
		ctx.True(project && project.IsValid(), "its audio project resolves");
		foreach (LF_DialogueLine line : config.m_aLines)
			ctx.True(line.m_fSeconds > 0.5 && !line.m_sMouth.IsEmpty(), "line " + line.m_sId + " has a recording and a mouth envelope");

		// The robbery reply is behind the armed condition; small talk and goodbye are not.
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.True(greet && greet.m_aOptions && greet.m_aOptions.Count() == 4, "the greeting has four replies");
		int robs = 0;
		int open = 0;
		foreach (LF_DialogueOption option : greet.m_aOptions)
		{
			if (option.m_sCondition == "RobArmed")
				robs++;
			else if (option.m_sCondition.IsEmpty())
				open++;
		}

		ctx.Equal(1, robs, "one greeting reply is the robbery, offered only when armed");
		ctx.Equal(3, open, "where, small talk and goodbye are always offered");
		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption each : node.m_aOptions)
			{
				if (!each.m_sCondition.IsEmpty())
					ctx.True(LF_DialogueHook.Find(each.m_sCondition) != null, "condition hook exists: " + each.m_sCondition);

				if (!each.m_sAction.IsEmpty())
					ctx.True(LF_DialogueHook.Find(each.m_sAction) != null, "action hook exists: " + each.m_sAction);
			}
		}

		// What the attendant says during and after the hold is voiced (LF_Dialogue.Announce).
		array<string> cries = {"#LF-RobHold_ClerkBeg", "#LF-RobHold_ClerkStammer", "#LF-RobHold_ClerkPaid", "#LF-RobHold_ClerkPolice", "#LF-RobHold_ClerkGone"};
		foreach (string key : cries)
			ctx.True(LF_DialogueFlow.FindLine(config, LF_DialogueFlow.SpokenFor(config, key)) != null, "the attendant has a voiced line for " + key);

		// The attendant's till is the fuel till, robbed only through the attendant.
		LF_RobAction till = LF_RobHold.TillOf(clerk);
		ctx.True(till != null && till.GetOwner() == tillEntity, "the attendant finds the fuel till behind its counter");
		if (!till)
			return;

		ctx.True(till.IsDialogueOnly(), "the fuel till is robbed through its attendant");
		ctx.False(till.CanBeShownScript(clerk), "the fuel till's world action is retired");
		ctx.True(FindRob(tillEntity) == till, "the attended till carries the robbery itself");
		ctx.True(till.GetHoldSeconds() >= 10 && till.GetHoldRadius() >= 3 && till.GetPoliceRadius() > till.GetHoldRadius(), "the hold has a time, a radius and a wider police radius");

		// An unarmed character: no robbery reply, refused before anything is reported.
		IEntity robber = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, tillEntity.GetOrigin() + "0 0 -1.5", false);
		if (!robber)
			return;

		LF_RobAction.ResetSession();
		LF_DialogueHook armed = LF_DialogueHook.Find("RobArmed");
		ctx.False(armed && armed.Allows(robber, clerk, ""), "an unarmed character is not offered the robbery");
		ctx.EqualStr(LF_RobHold.REFUSED_WEAPON, LF_RobHold.Refusal(robber, clerk), "the server refuses an unarmed robbery");
		ctx.EqualStr(LF_RobHold.REFUSED_WEAPON, LF_RobHold.Start(robber, clerk), "starting it unarmed is refused");
		ctx.Equal(0, LF_RobHold.ActiveCount(), "no hold runs");
		ctx.Equal(0, LF_RobAction.GetAlertCount(), "nothing is reported");
		SCR_EntityHelper.DeleteEntityAndChildren(robber);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RobAction FindRob(IEntity entity)
	{
		if (!entity)
			return null;

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction action : list)
		{
			LF_RobAction rob = LF_RobAction.Cast(action);
			if (rob)
				return rob;
		}

		return null;
	}
}
