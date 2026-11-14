// red-proof: observed 2026-11-14 with `tools\cli test --no-build --local --only LF_Test_SystemsWorldResources,LF_Test_SystemsWorldPlaces`:
// with LF_Systems.IsEnabled returning true for Chains, Hideouts, Houses and Survival, systems/world-places failed
// `- Chains off: a load is refused (expected 11, got 3)`, `- Hideouts off: a lease is refused`, `- Hideouts off: the stash is refused`,
// `- Houses off: the same door is a plain door (expected 1, got 0)`, `- Survival off: starving costs no health (got 0.75)` and
// `- Survival off: food restores no hunger (expected 0, got 30)`. With it true for Emote alone the run stops with
// `Virtual Machine Exception ... SCR_EmoteBaseCommand.CanBePerformed` at the Emote-off check (the base command has no radial to run on). Restored; green.
// tier: WORLD
//! Systems (#1075), the place and body ones: Chains, Hideouts, Houses, Survival and Emote. Each is
//! turned off in the owner config and the real fixture is asked: a cider barrel refuses a load, a
//! hideout lease, door and stash are refused (and what was leased is kept), a locked house door opens
//! like a plain door, hunger and thirst stop costing health and food restores nothing, a seat and an
//! emote offer nothing. Turned on again, each works as before.
class LF_Test_SystemsWorldPlaces : LF_Test
{
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const ResourceName SEAT = "{BE839ACB6CB2726A}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_green.et";

	override string GetName()
	{
		return "systems/world-places";
	}

	//------------------------------------------------------------------------------------------------
	protected void Owner(string systems)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		if (!systems.IsEmpty())
			LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"systems\":{\"switches\":" + systems + "}}", "test");
		LF_Systems.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected int ShownActions(IEntity entity, IEntity user, typename actionType)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return -1;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		int shown;
		foreach (BaseUserAction action : actions)
		{
			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (scripted && scripted.IsInherited(actionType) && scripted.CanBeShownScript(user))
				shown++;
		}

		return shown;
	}

	//------------------------------------------------------------------------------------------------
	//! The first door action on the door that says it can be performed by the user, or -1 with none.
	protected int DoorPerformable(IEntity door, IEntity user)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(door.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return -1;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			SCR_DoorUserAction doorAction = SCR_DoorUserAction.Cast(action);
			if (doorAction)
			{
				if (doorAction.CanBePerformedScript(user))
					return 1;

				return 0;
			}
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Owner(string.Empty);
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(user, "a character spawns");
		if (!user)
			return;

		RunChains(ctx, user);
		RunHideouts(ctx);
		RunHouses(ctx, user);
		RunSurvival(ctx, user);
		RunEmote(ctx, user);

		LF_TestCharacterFixture.Delete(user);
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChains(LF_TestContext ctx, IEntity user)
	{
		LF_ChainStationComponent barrel = LF_ChainStations.Find(LF_TestAnnex.CHAINS_BARREL_1);
		ctx.NotNull(barrel, "a cider barrel is registered");
		if (!barrel)
			return;

		ctx.True(barrel.ServerLoad(user, 1000) != LF_EChainStation.SYSTEM_OFF, "Chains on: a load is not refused for the switch");
		Owner("{\"Chains\":{\"enabled\":false}}");
		ctx.Equal(LF_EChainStation.SYSTEM_OFF, barrel.ServerLoad(user, 1000), "Chains off: a load is refused");
		ctx.Equal(LF_EChainStation.SYSTEM_OFF, barrel.ServerWork(user, 1000), "Chains off: work is refused");
		ctx.Equal(LF_EChainStation.SYSTEM_OFF, barrel.ServerCollect(user, 1000), "Chains off: a collect is refused");
		ctx.EqualStr("#LF-Systems_Off", LF_ChainStationComponent.RefusalKey(LF_EChainStation.SYSTEM_OFF), "and the refusal has words");
		Owner(string.Empty);
		ctx.True(barrel.ServerLoad(user, 1000) != LF_EChainStation.SYSTEM_OFF, "Chains on again: the barrel answers for itself");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHideouts(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		string founder = "systems-hideout-founder";
		string hideoutId = LF_Hideout.OrgHideoutId();
		LF_Organization org = LF_OrganizationManager.Create(founder, "Systems Outfit");
		ctx.NotNull(org, "an organization is founded");
		if (!org)
			return;

		Owner("{\"Hideouts\":{\"enabled\":false}}");
		ctx.False(LF_Hideout.Assign(hideoutId, org.GetId()), "Hideouts off: a lease is refused");
		ctx.EqualStr("", LF_Hideout.GetOwner(hideoutId), "and nobody owns the hideout");

		Owner(string.Empty);
		ctx.True(LF_Hideout.Assign(hideoutId, org.GetId()), "Hideouts on: the lease goes through");
		ctx.True(LF_Hideout.AllowsStash(founder, hideoutId, false), "a member may use the stash");
		array<ref LF_HideoutRecord> saved = LF_Hideout.ExportAll();

		Owner("{\"Hideouts\":{\"enabled\":false}}");
		ctx.False(LF_Hideout.AllowsStash(founder, hideoutId, false), "Hideouts off: the stash is refused");
		ctx.False(LF_Hideout.TryUnlock(founder, hideoutId), "Hideouts off: the door is refused");
		ctx.False(LF_Hideout.CanProcess(founder, hideoutId), "Hideouts off: the hideout machine is refused");
		LF_Hideout.Reset();
		LF_Hideout.ApplyAll(saved);
		ctx.EqualStr(org.GetId(), LF_Hideout.GetOwner(hideoutId), "a saved lease loads while Hideouts is off");

		Owner(string.Empty);
		ctx.True(LF_Hideout.AllowsStash(founder, hideoutId, false), "Hideouts on again: the lease is still there");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouses(LF_TestContext ctx, IEntity user)
	{
		IEntity door = LF_Utils.SpawnEntityPrefab(DOOR, "60 0 60");
		ctx.NotNull(door, "a house door spawns");
		if (!door)
			return;

		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "the door carries a lock");
		if (doorLock)
		{
			doorLock.SetHouseIdentifier("systems-test-house");
			doorLock.Lock();
			ctx.Equal(0, DoorPerformable(door, user), "Houses on: a locked door does not open without its key");
			Owner("{\"Houses\":{\"enabled\":false}}");
			ctx.Equal(1, DoorPerformable(door, user), "Houses off: the same door is a plain door");
			ctx.True(doorLock.IsLocked(), "and its lock state is kept");
			Owner(string.Empty);
			ctx.Equal(0, DoorPerformable(door, user), "Houses on again: the lock holds");
			doorLock.Unlock();
		}

		SCR_EntityHelper.DeleteEntityAndChildren(door);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSurvival(LF_TestContext ctx, IEntity user)
	{
		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(user.FindComponent(LF_CharacterSurvivalComponent));
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(user.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.NotNull(survival, "the character carries survival");
		ctx.NotNull(damageManager, "the character has a damage manager");
		if (!survival || !damageManager)
			return;

		survival.Init("systems-survival");
		LF_SurvivalStats stats = survival.GetSurvivalStats();
		stats.SetHunger(0);
		stats.SetThirst(0);

		Owner("{\"Survival\":{\"enabled\":false}}");
		for (int i = 0; i < 100; i++)
			survival.ApplyNeedsToHealth(5);
		ctx.True(damageManager.GetHealthScaled() > 0.999, "Survival off: starving costs no health (got " + damageManager.GetHealthScaled() + ")");

		LF_ConsumableEffectSurvival food = new LF_ConsumableEffectSurvival();
		food.SetHungerRestore(30);
		food.SetThirstRestore(30);
		food.ApplyEffect(user, user, null, null);
		ctx.EqualFloat(0, stats.GetHunger(), 0.001, "Survival off: food restores no hunger");
		ctx.EqualFloat(0, stats.GetThirst(), 0.001, "Survival off: drink restores no thirst");

		Owner(string.Empty);
		food.ApplyEffect(user, user, null, null);
		ctx.EqualFloat(30, stats.GetHunger(), 0.001, "Survival on again: the saved stats are where they were, and food restores");
		for (int j = 0; j < 100; j++)
			survival.ApplyNeedsToHealth(5);
		ctx.True(damageManager.GetHealthScaled() > 0.999, "and fed, needs cost nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunEmote(LF_TestContext ctx, IEntity user)
	{
		IEntity seat = LF_Utils.SpawnEntityPrefab(SEAT, "66 0 66");
		ctx.NotNull(seat, "a bench spawns");
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		ctx.NotNull(character, "the user is a character");
		if (!seat || !character)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(seat);
			return;
		}

		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		ctx.NotNull(commanding, "the game mode carries the commanding manager");
		if (!commanding)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(seat);
			return;
		}

		SCR_BaseRadialCommand emote = commanding.FindCommand("CMD_Salute");
		ctx.NotNull(emote, "the salute command is in the emote radial");
		if (!emote)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(seat);
			return;
		}

		ctx.True(ShownActions(seat, user, LF_SitAction) > 0, "Emote on: the bench offers a seat");

		Owner("{\"Emote\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(seat, user, LF_SitAction), "Emote off: the bench offers no seat");
		ctx.False(emote.CanBePerformed(character), "Emote off: the emote may not be played");

		Owner(string.Empty);
		ctx.True(ShownActions(seat, user, LF_SitAction) > 0, "Emote on again: the seat is back");
		SCR_EntityHelper.DeleteEntityAndChildren(seat);
	}
}
