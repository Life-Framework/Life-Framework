// red-proof: made LF_AccountOwnerIndex.Resolve skip the cached owner and the once-per-frame walk
// (a walk per change, the old Publish cost) and ran
// `tools\cli test --no-build --only LF_Test_AccountPublishCost` (2026-11-14). Observed:
//   [LFTEST]   - one walk over the players for a frame of 128 changes (expected 1, got 128)
//   [LFTEST]   - a frame of 128 changes reads at most N + D identities, read 16384
//   [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1
// Restored Resolve and the test passed.

// tier: LOGIC
//! #716: an account change finds its player without walking every player each time. 128 synthetic
//! players, one change per account in one frame (a payday or a load): the old Publish read
//! 2 * 128 * 128 = 32768 identities; the owner index must stay within N + D = 256 and still
//! resolve each account to the right player, miss offline accounts, follow a player that left,
//! and keep two players on one account.
class LF_Test_AccountPublishCost : LF_Test
{
	override string GetName()
	{
		return "account/publish-cost";
	}

	override void Run(LF_TestContext ctx)
	{
		int PLAYERS = 128;
		LF_Test_FakeOwnerIndex index = new LF_Test_FakeOwnerIndex();
		for (int i = 1; i <= PLAYERS; i++)
			index.m_mUids.Set(i, "uid-" + i);

		array<int> owners = {};
		int wrong = 0;
		for (int i = 1; i <= PLAYERS; i++)
		{
			index.Resolve("uid-" + i, owners);
			if (owners.Count() != 1 || owners[0] != i)
				wrong++;
		}

		ctx.True(index.Resolve("uid-1", owners), "a cached owner is a fresh answer");
		ctx.Equal(0, wrong, "every account resolves to the player playing it");
		ctx.Equal(1, index.GetScans(), "one walk over the players for a frame of 128 changes");
		ctx.True(index.GetUidReads() <= PLAYERS * 2, "a frame of 128 changes reads at most N + D identities, read " + index.GetUidReads());

		int before = index.GetUidReads();
		index.Resolve("uid-offline", owners);
		ctx.Equal(0, owners.Count(), "an offline account has no owner");
		ctx.True(!index.Resolve("uid-offline-2", owners), "a miss on this frame's earlier walk is flagged for a retry");
		ctx.Equal(1, index.GetScans(), "misses after the frame's walk do not walk again");
		ctx.Equal(before, index.GetUidReads(), "misses after the frame's walk read nothing");

		// Player 5 leaves and player 200 joins on the same account: the stale cache is not trusted.
		index.m_mUids.Remove(5);
		index.m_mUids.Set(200, "uid-5");
		index.EndFrame();
		index.Resolve("uid-5", owners);
		ctx.Equal(1, owners.Count(), "a rejoined account has one owner");
		if (owners.Count() == 1)
			ctx.Equal(200, owners[0], "a rejoined account resolves to its new player id");
		ctx.Equal(2, index.GetScans(), "a stale owner walks the players once in the next frame");

		// Two players on one account (a session fallback): both bodies are written, as before.
		index.m_mUids.Set(201, "uid-7");
		index.EndFrame();
		index.Resolve("uid-7", owners);
		ctx.Equal(1, owners.Count(), "the cached single owner still matches until the next walk");
		index.Resolve("uid-new", owners);
		index.Resolve("uid-7", owners);
		ctx.Equal(2, owners.Count(), "after a walk both players on one account are owners");

		index.Resolve("", owners);
		ctx.Equal(0, owners.Count(), "an empty uid has no owner");
	}
}

//! Synthetic players for LF_Test_AccountPublishCost: player id -> account uid.
class LF_Test_FakeOwnerIndex : LF_AccountOwnerIndex
{
	ref map<int, string> m_mUids = new map<int, string>();

	override protected void ListPlayers(notnull array<int> playerIds)
	{
		foreach (int playerId, string uid : m_mUids)
		{
			playerIds.Insert(playerId);
		}
	}

	override protected string UidOf(int playerId)
	{
		return m_mUids.Get(playerId);
	}
}
