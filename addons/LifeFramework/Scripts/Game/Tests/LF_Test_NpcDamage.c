// red-proof: Observed 2026-11-11 with `tools/cli test --no-build --only LF_Test_NpcDamage`: (a) making LF_NpcHitZone.OnDamage return before LF_NpcDamage.OnKilled failed `- a lethal hit kills the civilian and queues its removal`; (b) before the fix, LF_Witness.CollectNpc skipped every town NPC (IsAlive is false for a non-character) and failed `- a civilian beside the crime is a witness` and `- a shopkeeper beside the crime is a witness`. Restored/fixed; green.
// tier: WORLD
//! Which town NPCs can be shot (#610). An ambient civilian (Npc_Civilian_01) carries a hit zone and
//! dies to lethal damage; shop clerks, the fence and quest givers (Npc_Default_01 and children)
//! have none and stay protected. A killed civilian is queued for removal once.
class LF_Test_NpcDamage : LF_Test
{
	static const ResourceName CIVILIAN = "{6E8EF026940F3841}Prefabs/Characters/Npcs/Npc_Civilian_01.et";
	static const ResourceName CLERK = "{850859F356F5058E}Prefabs/Characters/Npcs/Npc_Default_01.et";
	static const ResourceName QUEST_GIVER = "{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et";
	static const ResourceName FENCE = "{12FCE81FDD79F2B4}Prefabs/Characters/Npcs/LF_Fence.et";

	override string GetName()
	{
		return "npc/damage";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_NpcDamage.ResetStatics();

		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK, "0 0 0");
		IEntity giver = LF_Utils.SpawnEntityPrefab(QUEST_GIVER, "0 0 0");
		IEntity fence = LF_Utils.SpawnEntityPrefab(FENCE, "0 0 0");
		ctx.True(clerk != null && giver != null && fence != null, "the protected NPC prefabs spawn");
		ctx.False(LF_NpcDamage.IsKillable(clerk), "a shop clerk takes no damage");
		ctx.False(LF_NpcDamage.IsKillable(giver), "a quest giver takes no damage");
		ctx.False(LF_NpcDamage.IsKillable(fence), "the fence takes no damage");
		ctx.False(LF_NpcDamage.IsKillable(null), "nothing is killable");

		IEntity civilian = LF_Utils.SpawnEntityPrefab(CIVILIAN, "0 0 0");
		ctx.True(civilian != null, "the civilian prefab spawns");
		if (civilian)
		{
			ctx.True(LF_NpcDamage.IsKillable(civilian), "an ambient civilian can be shot");
			RunCivilian(ctx, civilian);
		}

		RunWitness(ctx);

		LF_Test_NpcDamage.Delete(clerk);
		LF_Test_NpcDamage.Delete(giver);
		LF_Test_NpcDamage.Delete(fence);
		LF_Test_NpcDamage.Delete(civilian);
		LF_NpcDamage.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCivilian(LF_TestContext ctx, IEntity civilian)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(civilian.FindComponent(SCR_DamageManagerComponent));
		if (!damage || !damage.GetDefaultHitZone())
		{
			ctx.Fail("the civilian has no damage manager or default hit zone");
			return;
		}

		HitZone zone = damage.GetDefaultHitZone();
		ctx.True(zone.GetHealthScaled() > 0.99, "a fresh civilian is unhurt");
		vector hit[3];
		hit[0] = civilian.GetOrigin() + Vector(0, 1.2, 0);
		hit[1] = vector.Forward;
		hit[2] = vector.Up;

		SCR_DamageContext light = new SCR_DamageContext(EDamageType.KINETIC, 20, hit, civilian, zone, Instigator.CreateInstigator(null), null, -1, -1);
		damage.HandleDamage(light);
		ctx.True(zone.GetHealthScaled() < 0.99, "a bullet hurts the civilian");
		ctx.False(LF_NpcDamage.IsDying(civilian), "one light hit does not kill");

		SCR_DamageContext lethal = new SCR_DamageContext(EDamageType.KINETIC, 1000, hit, civilian, zone, Instigator.CreateInstigator(null), null, -1, -1);
		damage.HandleDamage(lethal);
		ctx.True(LF_NpcDamage.IsDying(civilian), "a lethal hit kills the civilian and queues its removal");
	}

	//------------------------------------------------------------------------------------------------
	//! A crime beside a town NPC makes it a witness (it used to be skipped as "not alive"), and a
	//! protected NPC like the clerk is one too.
	protected void RunWitness(LF_TestContext ctx)
	{
		LF_Witness.ResetStatics();
		IEntity suspect = LF_Utils.SpawnEntityPrefab(CLERK, "0 0 0");
		IEntity civilian = LF_Utils.SpawnEntityPrefab(CIVILIAN, "3 0 0");
		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK, "0 0 4");
		if (suspect && civilian && clerk)
		{
			LF_WitnessScene scene = LF_Witness.RecordCrime(901, "npc-damage-suspect", LF_WantedReports.OFFENCE_ROBBERY, suspect, "#jacket", "", "Test");
			ctx.True(scene != null, "the crime opens a scene");
			ctx.True(LF_Witness.SceneFor(LF_Witness.NpcKey(civilian), System.GetTickCount()) != null, "a civilian beside the crime is a witness");
			ctx.True(LF_Witness.SceneFor(LF_Witness.NpcKey(clerk), System.GetTickCount()) != null, "a shopkeeper beside the crime is a witness");
		}
		else
		{
			ctx.Fail("the witness fixtures spawn");
		}

		LF_Test_NpcDamage.Delete(suspect);
		LF_Test_NpcDamage.Delete(civilian);
		LF_Test_NpcDamage.Delete(clerk);
		LF_Witness.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Delete(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
