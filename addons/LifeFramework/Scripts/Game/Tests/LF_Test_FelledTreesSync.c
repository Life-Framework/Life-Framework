// red-proof: `tools\cli test --no-build --red LF_Test_FelledTreesSync`: RedProof() skips the server
// Publish, so "two published keys are in the join snapshot" fails, and picks the neighbour, so
// "the middle tree" fails. Before LF_FelledTreesReplication existed the test did not compile.

//! lumber/felled-trees-sync (#3261): a felled tree reaches every client by its key. The join snapshot
//! (what RplSave writes) round-trips through the same writer and reader code, a corrupt or foreign
//! snapshot is refused whole, and the client finds the felled tree among neighbours 0.5 m apart by its
//! prefab and its position to 0.1 m. The in-world hide and show, and the real late joiner, are the
//! `fell-sync` session proof.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FelledTreesSync : LF_Test
{
	protected static const string PINE = "{AAAAAAAAAAAAAAA1}Prefabs/Vegetation/Tree/Pinus_sylvestris/T_Pinus_sylvestris_2f.et";
	protected static const string BIRCH = "{AAAAAAAAAAAAAAA2}Prefabs/Vegetation/Tree/Betula_pendula/T_Betula_pendula_3a.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lumber/felled-trees-sync";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FelledTreesReplication.ResetStatics();
		CheckSnapshot(ctx);
		CheckRefusals(ctx);
		CheckPick(ctx);
		LF_FelledTreesReplication.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSnapshot(LF_TestContext ctx)
	{
		string keyA = KeyAt("1234.52 10 -87.31", PINE);
		string keyB = KeyAt("1234.99 10 -87.31", PINE);
		string keyC = KeyAt("-5.04 2 40", BIRCH);

		if (!RedProof())
		{
			ctx.True(LF_FelledTreesReplication.Publish(keyA), "a felled tree is published");
			ctx.True(LF_FelledTreesReplication.Publish(keyC), "a second felled tree is published");
		}

		ctx.False(LF_FelledTreesReplication.Publish(keyC), "the same tree twice changes nothing");
		ctx.False(LF_FelledTreesReplication.Publish(""), "an empty key is refused");
		ctx.False(LF_FelledTreesReplication.Publish("not-a-key"), "a key that is not a tree key is refused");

		array<string> table = LF_FelledTreesReplication.Collect();
		ctx.Equal(2, table.Count(), "two published keys are in the join snapshot");

		// The snapshot goes through the writer and comes back through the reader.
		LF_FelledTreesTestWire wire = new LF_FelledTreesTestWire();
		LF_FelledTreesReplication.Write(wire, table);
		array<string> joined = {};
		ctx.True(LF_FelledTreesReplication.Read(wire, joined), "the join snapshot reads back");
		ctx.Equal(2, joined.Count(), "the joiner reads both keys");
		ctx.True(joined.Contains(keyA), "the joiner has the first key exactly");
		ctx.True(joined.Contains(keyC), "the joiner has the second key exactly (negative x, other prefab)");
		ctx.False(joined.Contains(keyB), "a key never sent is not in the snapshot");

		// An empty table is a valid snapshot.
		LF_FelledTreesTestWire emptyWire = new LF_FelledTreesTestWire();
		LF_FelledTreesReplication.Write(emptyWire, {});
		array<string> none = {};
		ctx.True(LF_FelledTreesReplication.Read(emptyWire, none), "an empty snapshot reads");
		ctx.Equal(0, none.Count(), "an empty snapshot holds no keys");

		// A regrown tree leaves the table.
		ctx.True(LF_FelledTreesReplication.Retract(keyA), "a regrown tree is retracted");
		ctx.False(LF_FelledTreesReplication.Retract(keyA), "retracting twice changes nothing");
		ctx.Equal(1, LF_FelledTreesReplication.Collect().Count(), "the regrown tree is out of the snapshot");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRefusals(LF_TestContext ctx)
	{
		// A snapshot from a format this build does not know gives no keys but keeps the stream aligned.
		LF_FelledTreesTestWire future = new LF_FelledTreesTestWire();
		future.WriteInt(LF_FelledTreesReplication.FORMAT_VERSION + 1);
		future.WriteInt(1);
		future.WriteString(PINE);
		future.WriteInt(1);
		future.WriteInt(10);
		future.WriteInt(20);
		future.WriteInt(0);
		future.WriteInt(777);
		array<string> keys = {};
		ctx.True(LF_FelledTreesReplication.Read(future, keys), "a newer format is read to the end");
		ctx.Equal(0, keys.Count(), "a newer format gives no keys");
		int tail;
		ctx.True(future.ReadInt(tail), "the stream stays aligned for what follows");
		ctx.Equal(777, tail, "the value after the snapshot is intact");

		// A short stream is refused.
		LF_FelledTreesTestWire shortWire = new LF_FelledTreesTestWire();
		shortWire.WriteInt(LF_FelledTreesReplication.FORMAT_VERSION);
		shortWire.WriteInt(1);
		shortWire.WriteString(PINE);
		shortWire.WriteInt(5);
		array<string> shortKeys = {};
		ctx.False(LF_FelledTreesReplication.Read(shortWire, shortKeys), "a short stream is refused");

		// A claimed count far above the cap is refused before any allocation loop.
		LF_FelledTreesTestWire huge = new LF_FelledTreesTestWire();
		huge.WriteInt(LF_FelledTreesReplication.FORMAT_VERSION);
		huge.WriteInt(0);
		huge.WriteInt(LF_FelledTreesReplication.MAX_KEYS + 1);
		array<string> hugeKeys = {};
		ctx.False(LF_FelledTreesReplication.Read(huge, hugeKeys), "a count above the cap is refused");

		// A key pointing at a prefab slot that does not exist is refused.
		LF_FelledTreesTestWire stray = new LF_FelledTreesTestWire();
		stray.WriteInt(LF_FelledTreesReplication.FORMAT_VERSION);
		stray.WriteInt(1);
		stray.WriteString(PINE);
		stray.WriteInt(1);
		stray.WriteInt(10);
		stray.WriteInt(20);
		stray.WriteInt(3);
		array<string> strayKeys = {};
		ctx.False(LF_FelledTreesReplication.Read(stray, strayKeys), "a key naming a missing prefab slot is refused");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPick(LF_TestContext ctx)
	{
		// Three pines in a row 0.5 m apart, a birch on the exact spot of the middle pine, and a pine
		// further along. The key of the middle pine must pick the middle pine and nothing else.
		array<vector> positions = {};
		array<string> prefabs = {};
		AddCandidate(positions, prefabs, "500 12 700", PINE);
		AddCandidate(positions, prefabs, "500.5 12 700", PINE);
		AddCandidate(positions, prefabs, "501 12 700", PINE);
		AddCandidate(positions, prefabs, "500.5 12 700", BIRCH);
		AddCandidate(positions, prefabs, "520 12 700", PINE);

		string middle = KeyAt("500.5 12 700", PINE);
		int picked = -1;
		if (!RedProof())
			picked = LF_FelledTreesReplication.Pick(positions, prefabs, middle);
		else
			picked = 0;

		ctx.Equal(1, picked, "the middle tree is picked among neighbours 0.5 m apart");
		ctx.Equal(0, LF_FelledTreesReplication.Pick(positions, prefabs, KeyAt("500 12 700", PINE)), "the first neighbour is picked by its own key");
		ctx.Equal(2, LF_FelledTreesReplication.Pick(positions, prefabs, KeyAt("501 12 700", PINE)), "the last neighbour is picked by its own key");
		ctx.Equal(3, LF_FelledTreesReplication.Pick(positions, prefabs, KeyAt("500.5 12 700", BIRCH)), "the prefab decides when two trees share a spot");

		// A key made from a position a few centimetres off (the key is rounded to 0.1 m) still finds its tree.
		ctx.Equal(1, LF_FelledTreesReplication.Pick(positions, prefabs, KeyAt("500.54 12 700.04", PINE)), "the 0.1 m rounding still finds the tree");

		// No tree there, a tree of another prefab, or a broken key picks nothing.
		ctx.Equal(-1, LF_FelledTreesReplication.Pick(positions, prefabs, KeyAt("510 12 700", PINE)), "an empty spot picks nothing");
		ctx.Equal(-1, LF_FelledTreesReplication.Pick(positions, prefabs, KeyAt("520 12 700", BIRCH)), "a tree of another prefab picks nothing");
		ctx.Equal(-1, LF_FelledTreesReplication.Pick(positions, prefabs, "garbage"), "a broken key picks nothing");
		array<vector> noPositions = {};
		array<string> noPrefabs = {};
		ctx.Equal(-1, LF_FelledTreesReplication.Pick(noPositions, noPrefabs, middle), "no candidates picks nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected static string KeyAt(string position, string prefab)
	{
		return LF_TreeFelling.KeyOf(position.ToVector(), prefab);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCandidate(array<vector> positions, array<string> prefabs, string position, string prefab)
	{
		positions.Insert(position.ToVector());
		prefabs.Insert(prefab);
	}
}

//------------------------------------------------------------------------------------------------
//! A writer and a reader over the same token list, so the join snapshot's format code runs without a
//! network stream. The bit-stream adapters are one line per call.
class LF_FelledTreesTestWire : LF_FelledTreesWire
{
	protected ref array<string> m_aTokens = {};
	protected int m_iAt;

	//------------------------------------------------------------------------------------------------
	override void WriteInt(int value)
	{
		m_aTokens.Insert(value.ToString());
	}

	//------------------------------------------------------------------------------------------------
	override void WriteString(string value)
	{
		m_aTokens.Insert(value);
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadInt(out int value)
	{
		if (m_iAt >= m_aTokens.Count())
			return false;

		value = m_aTokens[m_iAt].ToInt();
		m_iAt++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadString(out string value)
	{
		if (m_iAt >= m_aTokens.Count())
			return false;

		value = m_aTokens[m_iAt];
		m_iAt++;
		return true;
	}
}
