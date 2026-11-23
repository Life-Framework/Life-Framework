// red-proof: RedProof() drops the pack tables, so the north and south crates have no rows and every assertion below fails (`tools\cli test --no-build --red LF_Test_EveronOutpostStock`).
//! O6 (#4453, N29) in game: under the Everon 1989 header (the pack leg of `tools\cli test`), each outpost's
//! crate stock loads from `LF_OutpostStock_Everon1989.conf`. The north table sells pistols, rifles and the gang
//! radio by tier; every row's prefab resolves and its price and tier sit inside the N29 bands (pistol $400 to
//! $600 at tier 1, rifle $1,500 to $2,500 at tier 2, the radio $60 for anyone); the south table is a different stock.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronOutpostStock : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/outpost-stock";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasRow(array<ref LF_OutpostStockItem> rows, string id)
	{
		foreach (LF_OutpostStockItem row : rows)
		{
			if (row.m_sId == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every row's prefab resolves and loads, its name key is set, and its price and tier are in its class's band.
	protected static void CheckRows(LF_TestContext ctx, string outpost, array<ref LF_OutpostStockItem> rows)
	{
		foreach (LF_OutpostStockItem row : rows)
		{
			string label = string.Format("%1/%2", outpost, row.m_sId);
			ResourceName prefab = row.ResolvePrefab();
			ctx.False(prefab.IsEmpty(), label + " has a prefab");
			if (!prefab.IsEmpty())
				ctx.True(Resource.Load(prefab).IsValid(), label + " prefab loads");

			ctx.True(row.m_sNameKey.StartsWith("#"), label + " names itself with a # key");
			if (row.m_sId == "gang_radio")
			{
				ctx.Equal(60, row.m_iPrice, label + " is $60");
				ctx.Equal(0, row.m_iMinTier, label + " is for anyone");
			}
			else if (row.m_sId.StartsWith("pistol"))
			{
				ctx.True(row.m_iPrice >= 400 && row.m_iPrice <= 600, label + " is $400 to $600");
				ctx.Equal(1, row.m_iMinTier, label + " is tier 1");
			}
			else if (row.m_sId.StartsWith("rifle"))
			{
				ctx.True(row.m_iPrice >= 1500 && row.m_iPrice <= 2500, label + " is $1,500 to $2,500");
				ctx.Equal(2, row.m_iMinTier, label + " is tier 2");
			}
			else
			{
				ctx.Fail(label + " is not a pistol, a rifle or the gang radio");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OutpostStockTables.Reload();
		// RedProof(): the pack tables are replaced by none, so no outpost has a table (`--red`).
		if (RedProof())
			LF_OutpostStockTables.UseTablesForTest(new array<ref LF_OutpostStockTable>());

		array<ref LF_OutpostStockItem> north = LF_OutpostStockTables.RowsFor("north");
		array<ref LF_OutpostStockItem> south = LF_OutpostStockTables.RowsFor("south");
		ctx.True(north != null, "the pack gives the north outpost a table");
		ctx.True(south != null, "the pack gives the south outpost a table");
		ctx.True(LF_OutpostStockTables.RowsFor("nowhere") == null, "an outpost with no table gets none (the crate falls back to the default stock)");
		if (!north || !south)
		{
			LF_OutpostStockTables.Reload();
			return;
		}

		ctx.True(HasRow(north, "gang_radio") && HasRow(north, "pistol") && HasRow(north, "rifle"), "the north crate sells the gang radio, a pistol and a rifle");
		CheckRows(ctx, "north", north);
		CheckRows(ctx, "south", south);

		// The south stock is its own: not the north's rows.
		bool same = north.Count() == south.Count();
		if (same)
		{
			foreach (LF_OutpostStockItem row : north)
			{
				if (!HasRow(south, row.m_sId))
					same = false;
			}
		}

		ctx.False(same, "the south table is a different stock from the north's");

		LF_OutpostStockTables.Reload();
	}
}
