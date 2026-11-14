// red-proof: observed 2026-11-14 with `tools\cli test --no-build --local --only LF_Test_SystemsWorldResources,LF_Test_SystemsWorldPlaces`:
// with LF_Systems.IsEnabled returning true for Gathering, Mining, Processing, Resources and Atmosphere (and Toys),
// systems/world-resources failed `- Resources off: the pickaxe deals no damage`, `- Mining off: a hit on the rock drops nothing (expected 0, got 4)`,
// `- Mining off: the area places no ore (expected 0, got 2)`, `- Mining off: a spent rock does not grow back`,
// `- Gathering off: the pile offers no gather action (expected 0, got 1)`, `- Processing off: the lathe offers no process action (expected 0, got 1)`,
// `- Atmosphere off: the radio offers no switch (expected 0, got 1)` and `- Toys off: a jump pad is inactive`. Restored; green.
// tier: WORLD
//! Systems (#1075), the world-resource ones: Gathering, Processing, Mining, Resources, Atmosphere
//! and Toys. Each is turned off in the owner config and the real fixture is asked: the gather pile,
//! the lathe and the radio offer no action; a rock takes no hit, drops nothing and does not grow back;
//! a pickaxe deals no damage; a mining area places no ore; the jump pad is inactive. Turned on again,
//! every one of them works as before and the rock that was spent grows back.
class LF_Test_SystemsWorldResources : LF_Test
{
	protected static const ResourceName PILE = "{81CB1B2631584427}Prefabs/Props/Construction/CementPile.et";
	protected static const ResourceName LATHE = "{9FB8E1E473BF6503}Prefabs/Crafting/Lathe.et";
	protected static const ResourceName RADIO = "{16C2DFD610CC7AFB}Prefabs/Atmosphere/LF_Radio.et";
	protected static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const vector SPOT = "44 0 44";

	protected ref array<IEntity> m_aFound = {};

	override string GetName()
	{
		return "systems/world-resources";
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
	protected bool CollectFound(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsNugget(IEntity entity)
	{
		return entity && LF_Utils.GetPrefabName(entity) == NUGGET && !entity.GetParent();
	}

	//------------------------------------------------------------------------------------------------
	protected int NuggetsNear(vector center, bool cleanUp)
	{
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(center, 3, CollectFound, IsNugget);
		int count = m_aFound.Count();
		if (cleanUp)
		{
			foreach (IEntity nugget : m_aFound)
				SCR_EntityHelper.DeleteEntityAndChildren(nugget);
		}

		m_aFound = new array<IEntity>();
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_DamageContext Swing(IEntity rock, HitZone zone, IEntity tool)
	{
		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = rock.GetOrigin() + Vector(-0.6, 0.4, 0);
		hitPosDirNorm[1] = vector.Right;
		hitPosDirNorm[2] = -vector.Right;

		SCR_DamageContext context = new SCR_DamageContext(EDamageType.MELEE, 20.23, hitPosDirNorm,
			rock, zone, Instigator.CreateInstigator(tool), null, -1, -1);
		context.damageSource = tool;
		return context;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Owner(string.Empty);
		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		vector at = Vector(SPOT[0], y, SPOT[2]);
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		IEntity pile = LF_Utils.SpawnEntityPrefab(PILE, at + "6 0 0");
		IEntity lathe = LF_Utils.SpawnEntityPrefab(LATHE, at + "9 0 0");
		IEntity radio = LF_Utils.SpawnEntityPrefab(RADIO, at + "12 0 0");
		IEntity rock = LF_Utils.SpawnEntityPrefab(IRON_ORE, at);
		IEntity pickaxe = LF_Utils.SpawnEntityPrefab(PICKAXE, at + Vector(0, 5, 0));
		ctx.NotNull(user, "a character spawns");
		ctx.NotNull(pile, "the gather pile spawns");
		ctx.NotNull(lathe, "the lathe spawns");
		ctx.NotNull(radio, "the radio spawns");
		ctx.NotNull(rock, "the iron rock spawns");
		ctx.NotNull(pickaxe, "the pickaxe spawns");
		if (user && pile && lathe && radio && rock && pickaxe)
			RunChecks(ctx, user, pile, lathe, radio, rock, pickaxe, at);

		NuggetsNear(at, true);
		LF_TestCharacterFixture.Delete(user);
		SCR_EntityHelper.DeleteEntityAndChildren(pile);
		SCR_EntityHelper.DeleteEntityAndChildren(lathe);
		SCR_EntityHelper.DeleteEntityAndChildren(radio);
		SCR_EntityHelper.DeleteEntityAndChildren(rock);
		SCR_EntityHelper.DeleteEntityAndChildren(pickaxe);
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity user, IEntity pile, IEntity lathe, IEntity radio, IEntity rock, IEntity pickaxe, vector at)
	{
		LF_DestructibleResourceComponent damage = LF_Component<LF_DestructibleResourceComponent>.Find(rock);
		LF_OreNodeComponent ore = LF_OreNodeComponent.Cast(rock.FindComponent(LF_OreNodeComponent));
		ctx.NotNull(damage, "the rock takes damage through LF_DestructibleResourceComponent");
		ctx.NotNull(ore, "the rock carries LF_OreNodeComponent");
		if (!damage || !ore)
			return;

		HitZone zone = damage.GetDefaultHitZone();
		ctx.NotNull(zone, "the rock has a hit zone");
		if (!zone)
			return;

		// All on: the shipped default.
		ctx.True(ShownActions(pile, user, LF_GatherAction) > 0, "Gathering on: the pile offers a gather action");
		ctx.True(ShownActions(lathe, user, LF_ProcessAction) > 0, "Processing on: the lathe offers its process action");
		ctx.Equal(1, ShownActions(radio, user, LF_AmbientSwitchAction), "Atmosphere on: the radio offers its switch");
		ctx.True(LF_JumpPadComponent.SwitchedOn(), "Toys on: a jump pad is active");
		ctx.EqualFloat(20, zone.ComputeEffectiveDamage(Swing(rock, zone, pickaxe), false), 0.01, "Resources on: the pickaxe deals its 20");

		// Resources off: the pickaxe does nothing to the rock; nothing else turns off with it.
		Owner("{\"Resources\":{\"enabled\":false}}");
		ctx.False(LF_Systems.IsEnabled("Resources"), "the owner turns Resources off");
		ctx.True(zone.ComputeEffectiveDamage(Swing(rock, zone, pickaxe), false) <= 0, "Resources off: the pickaxe deals no damage");
		for (int i = 0; i < 3; i++)
			damage.HandleDamage(Swing(rock, zone, pickaxe));
		ctx.Equal(0, ore.GetDroppedCount(), "Resources off: three pickaxe hits drop nothing");
		ctx.True(LF_Systems.IsEnabled("Mining"), "and Mining stays on");

		// Mining off: the rock refuses a hit made directly, and a mining area places no ore.
		Owner("{\"Mining\":{\"enabled\":false}}");
		ctx.Equal(0, ore.OnToolHit(500, rock.GetOrigin(), vector.Right, pickaxe), "Mining off: a hit on the rock drops nothing");
		ctx.Equal(0, ore.GetBreaks(), "and breaks no piece off");
		ctx.Equal(0, NuggetsNear(at, false), "and no nugget lies beside the rock");

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = at + "0 0 20";
		LF_MiningArea area = LF_MiningArea.Cast(GetGame().SpawnEntity(LF_MiningArea, GetGame().GetWorld(), params));
		ctx.NotNull(area, "a mining area spawns");
		if (area)
		{
			LF_WeightedSpawn weight = new LF_WeightedSpawn();
			weight.m_iChance = 100;
			weight.m_Prefab = IRON_ORE;
			area.m_PrefabsToSpawn = new array<ref LF_WeightedSpawn>();
			area.m_PrefabsToSpawn.Insert(weight);
			area.SpawnOres(2);
			ctx.Equal(0, SCR_EntityHelper.GetChildrenCount(area), "Mining off: the area places no ore");
			Owner(string.Empty);
			area.SpawnOres(2);
			ctx.Equal(2, SCR_EntityHelper.GetChildrenCount(area), "Mining on: the same area places its ore");
			SCR_EntityHelper.DeleteEntityAndChildren(area);
		}

		// A spent rock stays spent while Mining is off, and grows back once it is on.
		Owner(string.Empty);
		for (int hit = 0; hit < 20; hit++)
			ore.OnToolHit(500, rock.GetOrigin(), vector.Right, pickaxe);
		ctx.True(ore.IsDepleted(), "Mining on: enough hits spend the rock");
		Owner("{\"Mining\":{\"enabled\":false}}");
		ore.Regrow();
		ctx.True(ore.IsDepleted(), "Mining off: a spent rock does not grow back");
		Owner(string.Empty);
		ore.Regrow();
		ctx.False(ore.IsDepleted(), "Mining on again: the rock grows back as it would have");

		// Gathering, Processing, Atmosphere, Toys: each off on its own.
		Owner("{\"Gathering\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(pile, user, LF_GatherAction), "Gathering off: the pile offers no gather action");
		ctx.True(ShownActions(lathe, user, LF_ProcessAction) > 0, "and the lathe still works");
		Owner("{\"Processing\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(lathe, user, LF_ProcessAction), "Processing off: the lathe offers no process action");
		ctx.True(ShownActions(pile, user, LF_GatherAction) > 0, "and the pile still gathers");
		Owner("{\"Atmosphere\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(radio, user, LF_AmbientSwitchAction), "Atmosphere off: the radio offers no switch");
		Owner("{\"Toys\":{\"enabled\":false}}");
		ctx.False(LF_JumpPadComponent.SwitchedOn(), "Toys off: a jump pad is inactive");

		Owner(string.Empty);
		ctx.True(ShownActions(pile, user, LF_GatherAction) > 0, "all on again: the pile gathers");
		ctx.True(ShownActions(lathe, user, LF_ProcessAction) > 0, "the lathe processes");
		ctx.Equal(1, ShownActions(radio, user, LF_AmbientSwitchAction), "the radio switches");
		ctx.True(LF_JumpPadComponent.SwitchedOn(), "the pad is active");
	}
}
