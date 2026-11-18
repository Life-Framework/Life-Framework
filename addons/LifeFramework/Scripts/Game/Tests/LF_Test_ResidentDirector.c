// red-proof: fails with the hook on (`cli test --no-build --red LF_Test_ResidentDirector`): with RedProof() the pick falls back to
//   the id order the spawner used before #2910, so "the resident next to the player is live" and the counter-worker
//   assertions fail. RED-OK line recorded in the PR.
//! residents/resident-director: with more residents due than live slots, the director fills the place the
//! player stands in first (#2910). Pure rules, no world: due residents, anchor spots and player positions in,
//! the ids to spawn out. Cap 2 of four due picks the two nearest a player, not the first two by id; a counter
//! worker beside a player beats a nearer bench sitter; ties break by id; no slot picks nobody.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ResidentDirector : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/resident-director";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ResidentCandidate Due(string id, string role, string anchor, vector at)
	{
		LF_ResidentSlot slot = new LF_ResidentSlot();
		slot.m_sRole = role;
		slot.m_sAnchor = anchor;
		LF_ResidentSpot spot = new LF_ResidentSpot();
		spot.m_vOrigin = at;
		return LF_ResidentCandidate.Make(id, slot, spot);
	}

	//------------------------------------------------------------------------------------------------
	//! The pick, or with --red the old id-order pick the director replaces.
	protected int Pick(array<ref LF_ResidentCandidate> due, array<vector> players, int slots, array<string> into)
	{
		if (!RedProof())
			return LF_ResidentDirector.Pick(due, players, slots, into);

		into.Clear();
		array<string> ids = {};
		foreach (LF_ResidentCandidate candidate : due)
			ids.Insert(candidate.m_sId);

		ids.Sort();
		foreach (string id : ids)
		{
			if (into.Count() < slots)
				into.Insert(id);
		}

		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected bool Has(array<string> ids, string id)
	{
		return ids.Find(id) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<vector> players = {};
		array<string> picked = {};

		// Cap 2, four due: the player stands next to d_door; the first two by id (a_far, b_mid) are far.
		array<ref LF_ResidentCandidate> due = {};
		due.Insert(Due("a_far", "home", "bench", "500 0 0"));
		due.Insert(Due("b_mid", "home", "bench", "300 0 0"));
		due.Insert(Due("c_near", "home", "bench", "100 0 0"));
		due.Insert(Due("d_door", "home", "bench", "0 0 0"));
		players.Insert("2 0 1");
		int count = Pick(due, players, 2, picked);
		ctx.Equal(2, count, "cap 2 picks two of four");
		ctx.True(Has(picked, "d_door"), "the resident next to the player is live");
		ctx.True(Has(picked, "c_near"), "the next nearest is the second");
		ctx.False(Has(picked, "a_far"), "the first by id is not chosen when far");
		ctx.EqualStr("d_door", picked[0], "the nearest is first in the order");

		// A counter worker beside a player (10 m) beats a bench sitter 3 m away.
		due.Clear();
		due.Insert(Due("y_sitter", "home", "bench", "3 0 0"));
		due.Insert(Due("z_clerk", "work", "counter", "0 0 10"));
		due.Insert(Due("x_far", "home", "bench", "400 0 0"));
		players.Clear();
		players.Insert("0 0 0");
		Pick(due, players, 1, picked);
		ctx.EqualStr("z_clerk", picked[0], "a counter worker beside a player beats a bench sitter");
		Pick(due, players, 2, picked);
		ctx.True(Has(picked, "z_clerk") && Has(picked, "y_sitter"), "two slots take the clerk and the sitter, not the far one");
		ctx.EqualStr("z_clerk", picked[0], "the clerk is first, the sitter second");

		// A counter worker who is not beside the player gets no head start.
		due.Clear();
		due.Insert(Due("y_sitter", "home", "bench", "3 0 0"));
		due.Insert(Due("z_clerk", "work", "counter", "0 0 40"));
		Pick(due, players, 1, picked);
		ctx.EqualStr("y_sitter", picked[0], "a counter worker 40 m away loses to a sitter 3 m away");

		// Only a work line on the counter anchor is a counter worker.
		due.Clear();
		due.Insert(Due("y_sitter", "home", "bench", "3 0 0"));
		due.Insert(Due("z_porter", "work", "door", "0 0 10"));
		Pick(due, players, 1, picked);
		ctx.EqualStr("y_sitter", picked[0], "a worker at the door is no counter worker");

		// Ties break by id, whatever order they were found in.
		due.Clear();
		due.Insert(Due("m_b", "home", "bench", "5 0 0"));
		due.Insert(Due("m_a", "home", "bench", "-5 0 0"));
		Pick(due, players, 1, picked);
		ctx.EqualStr("m_a", picked[0], "equidistant residents: the lower id wins");

		// With the player's height ignored: a player on a roof is as near as one on the ground.
		due.Clear();
		due.Insert(Due("p_low", "home", "bench", "30 0 0"));
		due.Insert(Due("q_high", "home", "bench", "20 0 0"));
		players.Clear();
		players.Insert("0 25 0");
		Pick(due, players, 1, picked);
		ctx.EqualStr("q_high", picked[0], "distance is horizontal");

		// No free slot picks nobody; more slots than due picks them all; nobody due picks nobody.
		players.Clear();
		players.Insert("0 0 0");
		ctx.Equal(0, Pick(due, players, 0, picked), "no free slot picks nobody");
		ctx.Equal(2, Pick(due, players, 9, picked), "more slots than due picks all of them");
		due.Clear();
		ctx.Equal(0, Pick(due, players, 3, picked), "nobody due picks nobody");
	}
}
