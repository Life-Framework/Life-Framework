// red-proof: Observed 2026-11-18 with `tools/cli test --no-build --red LF_Test_SpawnPriority,LF_Test_HouseSpawn`: `RED PROOF OK (all 2 listed test(s) fail with the hook on)`; the order and verdict cases fail with the function skipped. Green on the real code.
// tier: LOGIC
//! spawn/priority (#2652): the login spawn order. One named case per pair: jail beats the hospital,
//! the house and the role; a death goes to the hospital even with a house; a rejoin spot beats the
//! house; the house beats the role; nothing set is the role's spawn point. A recorded house that is
//! gone or no longer the player's is refused (the caller logs the warning and takes the role spawn).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SpawnPriority : LF_Test
{
	protected static const string UID = "spawn-priority-owner";
	protected static const string OTHER = "spawn-priority-buyer";
	protected static const string GONE_HOUSE = "spawn-priority-gone-house";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/priority";
	}

	//------------------------------------------------------------------------------------------------
	protected int Rule(bool serving, bool wake, bool rejoin, bool house)
	{
		if (RedProof())
			return LF_SpawnRule.ROLE;

		return LF_SpawnPriority.Decide(serving, wake, rejoin, house);
	}

	//------------------------------------------------------------------------------------------------
	protected string Verdict(string houseId, bool inWorld, string liveOwner, string ownerKey)
	{
		if (RedProof())
			return "";

		return LF_SpawnPriority.HouseVerdict(houseId, inWorld, liveOwner, ownerKey);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_SpawnRule.JAIL, Rule(true, false, false, true), "jail beats the house");
		ctx.Equal(LF_SpawnRule.JAIL, Rule(true, true, true, true), "jail beats the hospital, the rejoin spot and the house");
		ctx.Equal(LF_SpawnRule.HOSPITAL, Rule(false, true, false, true), "a death goes to the hospital even with a house");
		ctx.Equal(LF_SpawnRule.HOSPITAL, Rule(false, true, true, false), "a death goes to the hospital before a rejoin spot");
		ctx.Equal(LF_SpawnRule.REJOIN, Rule(false, false, true, true), "a rejoin spot beats the house");
		ctx.Equal(LF_SpawnRule.HOUSE, Rule(false, false, false, true), "the house beats the role spawn");
		ctx.Equal(LF_SpawnRule.ROLE, Rule(false, false, false, false), "nothing set takes the role spawn point");

		ctx.EqualStr("", Verdict("", false, "", UID), "no spawn house chosen is not an error");
		ctx.EqualStr("", Verdict("house-a", true, UID, UID), "a house in the world the player owns is usable");
		ctx.False(Verdict("house-a", false, UID, UID).IsEmpty(), "a house that is not in the world is refused");
		ctx.False(Verdict("house-a", true, OTHER, UID).IsEmpty(), "a house now owned by someone else is refused");
		ctx.False(Verdict("house-a", true, "", UID).IsEmpty(), "a house now owned by nobody is refused");
		ctx.False(Verdict("house-a", true, "", "").IsEmpty(), "a player with no key never matches a house");

		RunGoneHouse(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A saved flag whose house is not in the world: the login falls back and leaves the position alone.
	protected void RunGoneHouse(LF_TestContext ctx)
	{
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		LF_PropertyRecord record = LF_Properties.Create(GONE_HOUSE);
		ctx.NotNull(record, "the property row is made");
		if (record)
		{
			LF_Properties.SetOwner(GONE_HOUSE, UID);
			record.m_bOwnerSpawnsHere = true;
			ctx.EqualStr(GONE_HOUSE, LF_Properties.SpawnHouseOf(UID), "the book still names the spawn house (the id is a dead one)");

			vector position = "1 2 3";
			vector heading = "4 5 6";
			bool used = false;
			if (!RedProof())
				used = LF_SpawnPriority.ResolveHouse(-1, UID, position, heading);

			ctx.False(used, "a spawn house that is not in the world falls back to the role spawn");
			ctx.True(position == "1 2 3", "a refused house leaves the position alone");
			ctx.True(used == false && LF_Properties.SpawnHouseOf(UID) == GONE_HOUSE, "a refused house keeps its flag for when the house is back");
		}

		LF_Properties.ApplyAll(before);
	}
}
