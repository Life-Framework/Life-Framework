// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_NpcStunWorld`: asserting the stunned NPC stays upright failed `a stunned NPC lies down`; restored, green.
// tier: WORLD
//! The taser on a town NPC (#922): a town NPC is stunned, lies down, cannot be talked to, wakes
//! where it stood, and a stale wake or a second stun does nothing. A dart read from a shooter with
//! no stun weapon stuns nobody.
class LF_Test_NpcStunWorld : LF_Test
{
	static const ResourceName NPC_PREFAB = "{850859F356F5058E}Prefabs/Characters/Npcs/Npc_Default_01.et";
	static const ResourceName TASER_PREFAB = "{6D583A40C5C5C150}Prefabs/Weapons/Handguns/Taser/Handgun_Taser.et";

	override string GetName()
	{
		return "police/npc-stun-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_NpcStun.ResetStatics();

		IEntity entity = LF_Utils.SpawnEntityPrefab(NPC_PREFAB, "0 -50 0");
		LF_NpcBaseEntity npc = LF_NpcBaseEntity.Cast(entity);
		ctx.True(npc != null, "the town NPC spawns as an LF_NpcBaseEntity");
		if (!npc)
			return;

		vector rest = npc.GetOrigin();
		ctx.False(LF_NpcStun.BlocksTalk(npc), "a standing NPC can be talked to");
		ctx.True(npc.GetTransformAxis(1)[1] > 0.99, "a standing NPC is upright");

		ctx.False(LF_NpcStun.Apply(npc, 0), "a stun of no time is refused");
		ctx.True(LF_NpcStun.Apply(npc, 5, "test"), "a stun is applied");
		ctx.True(npc.LF_IsStunned(), "the NPC carries the stun flag");
		ctx.True(LF_NpcStun.BlocksTalk(npc), "a stunned NPC cannot be talked to");
		ctx.True(npc.GetTransformAxis(1)[1] < 0.1, "a stunned NPC lies down");
		ctx.Equal(1, LF_NpcStun.GetStunCount(), "one stun was counted");

		ctx.False(LF_NpcStun.Apply(npc, 5, "test"), "an NPC that is down is not stunned again");
		ctx.Equal(1, LF_NpcStun.GetStunCount(), "the refused stun was not counted");

		LF_NpcStun.Wake(npc, 99);
		ctx.True(npc.LF_IsStunned(), "a stale wake leaves the stun in place");

		LF_NpcStun.Wake(npc, 1);
		ctx.False(npc.LF_IsStunned(), "the timed wake ends the stun");
		ctx.False(LF_NpcStun.BlocksTalk(npc), "a woken NPC can be talked to");
		ctx.True(npc.GetTransformAxis(1)[1] > 0.99, "a woken NPC stands up");
		ctx.EqualFloat(0, vector.Distance(rest, npc.GetOrigin()), 0.01, "a woken NPC stands where it fell");

		LF_NpcStun.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(npc);

		IEntity taser = LF_Utils.SpawnEntityPrefab(TASER_PREFAB, "0 -50 0");
		ctx.True(taser != null && taser.FindComponent(LF_StunWeaponComponent) != null, "the taser is a stun weapon");
		if (taser)
			SCR_EntityHelper.DeleteEntityAndChildren(taser);

		ctx.False(LF_NpcStun.HarnessShot(null, null), "a shot from nobody stuns nobody");
	}
}
