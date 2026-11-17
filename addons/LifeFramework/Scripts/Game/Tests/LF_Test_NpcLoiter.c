// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_NpcLoiter`: with Release skipped it failed `releasing stands the NPC up`, `releasing frees the seat` and `the freed seat is found again`; RED PROOF OK. Green without the hook.
// tier: WORLD
//! npc/loiter (#1928): an NPC of a loiter role takes a free seat near it (the same LF_SitAction a player
//! sits on), the seat reads taken, a second NPC finds no free place, and releasing frees it. The role and
//! pose choices are pure rules.
class LF_Test_NpcLoiter : LF_Test
{
	protected static const ResourceName CHAIR = "{69D6D9BFDFEC3225}Prefabs/Props/Seats/LF_Seat_Chair_02_green.et";

	override string GetName()
	{
		return "npc/loiter";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_NpcLoiter.IsLoiterRole("townsfolk"), "a resident passes the time");
		ctx.True(LF_NpcLoiter.IsLoiterRole("farmer"), "a farmer passes the time");
		ctx.False(LF_NpcLoiter.IsLoiterRole("police"), "police stand at their post");
		ctx.False(LF_NpcLoiter.IsLoiterRole("teller"), "a bank teller stays behind the desk");
		ctx.False(LF_NpcLoiter.IsLoiterRole(""), "no role, no loiter");
		ctx.False(LF_NpcLoiter.IsLoiterRole("farm"), "a role is matched whole");

		ctx.Equal(LF_ENpcLoiter.SEAT, LF_NpcLoiter.Choose(true, 7), "a free seat is taken first");
		ctx.Equal(LF_ENpcLoiter.LEAN, LF_NpcLoiter.Choose(false, 2), "no seat: an even seed leans");
		ctx.Equal(LF_ENpcLoiter.SMOKE, LF_NpcLoiter.Choose(false, 3), "no seat: an odd seed smokes");

		LF_NpcLoiter.ResetStatics();
		IEntity npc = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		if (npc && other)
			RunSeat(ctx, npc, other);

		LF_NpcLoiter.ResetStatics();
		LF_TestCharacterFixture.Delete(npc);
		LF_TestCharacterFixture.Delete(other);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSeat(LF_TestContext ctx, IEntity npc, IEntity other)
	{
		Resource res = Resource.Load(CHAIR);
		ctx.True(res.IsValid(), "the chair loads");
		if (!res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = "1 0 0";
		params.Transform[1] = "0 1 0";
		params.Transform[2] = "0 0 1";
		params.Transform[3] = npc.GetOrigin() + "1.5 0 0";
		IEntity chair = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(chair, "the chair spawns beside the NPC");
		if (!chair)
			return;

		LF_SitAction seat = LF_NpcLoiter.FindFreeSeat(npc);
		ctx.NotNull(seat, "a free seat within range is found");
		if (!seat)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(chair);
			return;
		}

		ctx.False(LF_NpcLoiter.IsTaken(seat), "the seat starts free");
		ctx.True(LF_NpcLoiter.Sit(npc, seat), "the NPC sits");
		ctx.True(LF_NpcLoiter.IsSeated(npc), "the NPC reads seated");
		ctx.True(LF_NpcLoiter.IsTaken(seat), "the seat reads taken");
		ctx.True(seat.IsOccupiedBy(npc), "the NPC is the seat's occupant");
		ctx.True(LF_NpcLoiter.FindFreeSeat(other) == null, "a second NPC finds no free place");
		ctx.False(LF_NpcLoiter.Sit(other, seat), "a second NPC cannot take the taken seat");

		if (!RedProof())
			LF_NpcLoiter.Release(npc, "test");

		ctx.False(LF_NpcLoiter.IsSeated(npc), "releasing stands the NPC up");
		ctx.False(LF_NpcLoiter.IsTaken(seat), "releasing frees the seat");
		ctx.NotNull(LF_NpcLoiter.FindFreeSeat(other), "the freed seat is found again");

		SCR_EntityHelper.DeleteEntityAndChildren(chair);
	}
}
