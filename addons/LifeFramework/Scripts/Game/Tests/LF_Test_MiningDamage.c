// red-proof: 2026-11-08, removed the hit zone heal (SetHealthScaled(1)) in LF_DestructibleResourceHitZone.OnDamage; `--only LF_Test_MiningDamage` failed "the rock's hit zone heals back" and "fifteen hits drop the iron row's five nuggets (expected 5, got 1)": the rock was destroyed by health after the first piece. Restored, green.

// tier: WORLD
//! Mining by hitting the rock. A melee context built the way SCR_MeleeComponent
//! builds it (damageSource = the swung tool) goes through the iron rock's damage
//! manager: the axe is refused, three pickaxe hits drop one iron nugget beside
//! the rock, five pieces spend it (hidden, no more hits), and it grows back.
class LF_Test_MiningDamage : LF_Test
{
	protected static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const vector SPOT = "40 0 40";

	protected ref array<IEntity> m_aFound = {};
	protected float ground;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/pickaxe-hits";
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNugget(IEntity entity)
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
	protected int NuggetsNear(vector center)
	{
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(center, 3, CollectNugget, IsNugget);
		return m_aFound.Count();
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
		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		vector at = Vector(SPOT[0], y, SPOT[2]);
		IEntity rock = LF_Utils.SpawnEntityPrefab(IRON_ORE, at);
		IEntity pickaxe = LF_Utils.SpawnEntityPrefab(PICKAXE, at + Vector(0, 5, 0));
		IEntity axe = LF_Utils.SpawnEntityPrefab(AXE, at + Vector(0, 6, 0));
		int nuggetsBefore = NuggetsNear(at);

		RunChecks(ctx, rock, pickaxe, axe, nuggetsBefore);

		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(at, 3, CollectNugget, IsNugget);
		foreach (IEntity nugget : m_aFound)
			SCR_EntityHelper.DeleteEntityAndChildren(nugget);
		m_aFound = new array<IEntity>();

		if (rock)
			SCR_EntityHelper.DeleteEntityAndChildren(rock);
		if (pickaxe)
			SCR_EntityHelper.DeleteEntityAndChildren(pickaxe);
		if (axe)
			SCR_EntityHelper.DeleteEntityAndChildren(axe);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity rock, IEntity pickaxe, IEntity axe, int nuggetsBefore)
	{
		ctx.NotNull(rock, "iron rock spawns");
		ctx.NotNull(pickaxe, "pickaxe spawns");
		ctx.NotNull(axe, "axe spawns");
		if (ctx.FailureCount() > 0)
			return;

		LF_DestructibleResourceComponent damage = LF_Component<LF_DestructibleResourceComponent>.Find(rock);
		LF_OreNodeComponent ore = LF_OreNodeComponent.Cast(rock.FindComponent(LF_OreNodeComponent));
		ctx.NotNull(damage, "the rock takes damage through LF_DestructibleResourceComponent");
		ctx.NotNull(ore, "the rock carries LF_OreNodeComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr("iron", ore.GetNodeId(), "the rock reads the iron row");
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(rock);
		ctx.True(actions == null, "the rock has no gather action: you mine it by hitting it");

		HitZone zone = damage.GetDefaultHitZone();
		ctx.NotNull(zone, "the rock has a default hit zone");
		if (ctx.FailureCount() > 0)
			return;

		SCR_DamageContext axeHit = Swing(rock, zone, axe);
		ctx.True(zone.ComputeEffectiveDamage(axeHit, false) <= 0, "the axe deals no damage to the rock");
		for (int i = 0; i < 3; i++)
			damage.HandleDamage(Swing(rock, zone, axe));
		ctx.Equal(0, ore.GetBreaks(), "three axe hits break nothing off");
		ctx.Equal(0, ore.GetDroppedCount(), "three axe hits drop nothing");

		int swapsBefore = LF_QuantityComponent.s_iModelSwaps;
		SCR_DamageContext pickHit = Swing(rock, zone, pickaxe);
		ctx.EqualFloat(20, zone.ComputeEffectiveDamage(pickHit, false), 0.01, "the pickaxe deals its 20 to the rock");

		damage.HandleDamage(Swing(rock, zone, pickaxe));
		damage.HandleDamage(Swing(rock, zone, pickaxe));
		ctx.Equal(0, ore.GetDroppedCount(), "two pickaxe hits drop nothing yet");
		damage.HandleDamage(Swing(rock, zone, pickaxe));
		ctx.Equal(1, ore.GetDroppedCount(), "the third pickaxe hit drops one nugget");
		ctx.Equal(nuggetsBefore + 1, NuggetsNear(rock.GetOrigin()), "that nugget lies in the world beside the rock");
		ctx.Equal(swapsBefore, LF_QuantityComponent.s_iModelSwaps, "a dropped nugget keeps its prefab mesh and material (no quantity mesh swap)");
		ctx.EqualFloat(0.89, ore.GetRockScale(), 0.001, "one piece off shrinks the rock one step");
		ctx.EqualFloat(0.89, rock.GetScale(), 0.001, "the rock entity is drawn at that scale");
		ground = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		ctx.True(m_aFound.Count() > 0 && Math.AbsFloat(m_aFound[0].GetOrigin()[1] - ground) < 0.3, "the nugget rests on the ground, not at the hit height");
		ctx.True(zone.GetHealthScaled() > 0.99, "the rock's hit zone heals back: it is never destroyed by health");

		for (int hit = 0; hit < 12; hit++)
			damage.HandleDamage(Swing(rock, zone, pickaxe));
		ctx.Equal(5, ore.GetDroppedCount(), "fifteen hits drop the iron row's five nuggets");
		ctx.True(ore.IsDepleted(), "five pieces spend the rock");
		ctx.True((rock.GetFlags() & EntityFlags.VISIBLE) == 0, "a spent rock is hidden");
		ctx.True(zone.ComputeEffectiveDamage(Swing(rock, zone, pickaxe), false) <= 0, "a spent rock takes no more damage");
		damage.HandleDamage(Swing(rock, zone, pickaxe));
		ctx.Equal(5, ore.GetDroppedCount(), "hitting a spent rock drops nothing");
		ctx.Equal(nuggetsBefore + 5, NuggetsNear(rock.GetOrigin()), "five nuggets lie beside the rock");

		ore.Regrow();
		ctx.False(ore.IsDepleted(), "the rock grows back");
		ctx.EqualFloat(1, rock.GetScale(), 0.001, "a grown rock is full size again");
		ctx.True((rock.GetFlags() & EntityFlags.VISIBLE) != 0, "a grown rock is visible again");
		ctx.Equal(0, ore.GetBreaks(), "a grown rock starts at zero pieces");
		for (int again = 0; again < 3; again++)
			damage.HandleDamage(Swing(rock, zone, pickaxe));
		ctx.Equal(6, ore.GetDroppedCount(), "a grown rock yields again");
	}
}
