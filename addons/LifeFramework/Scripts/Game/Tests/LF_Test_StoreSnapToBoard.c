// red-proof: observed 2026-11-14 with `tools\cli test --no-build --remote lf-testbox --only LF_Test_StoreSnapToBoard`: SNAP_ABOVE 0.5 (the old trace start) failed `- a point over the third board lands on it, not on the top board (y 1.17694, board 0.82)`; the item skip removed failed `- a loose item on the board is looked past (y 0.524628)`; restored, green.

// tier: WORLD
//! Shelf goods land on the board under their shelf point (#1143). A building store's gondolas and
//! coolers come from its furniture prefab, which are not runtime child entities of the store, so the
//! spawn trace takes any static surface under the point: a gondola board, the board under the point
//! and not the one above it, a counter top. A loose item already lying there is looked past, so
//! copies never stack. A point over nothing stays where it was written.
class LF_Test_StoreSnapToBoard : LF_Test
{
	static const ResourceName GONDOLA = "{B7669B7F8B7923F6}Prefabs/Props/Commercial/ShelfShop_01.et";
	static const ResourceName COUNTER = "{E214337C6E1931C5}Prefabs/Props/Commercial/CounterShop_01.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	//! Board tops of the vanilla models, prefab space (tools/buildings/pipeline/fixtures.json).
	static const float GONDOLA_BOARD_1 = 0.428;
	static const float GONDOLA_BOARD_2 = 0.82;
	static const float COUNTER_TOP = 1.039;

	override string GetName()
	{
		return "store/snap-to-board";
	}

	override void Run(LF_TestContext ctx)
	{
		vector shelfAt = "46 300 40";
		vector counterAt = "46 300 44";
		IEntity shelf = LF_Utils.SpawnEntityPrefab(GONDOLA, shelfAt);
		IEntity counter = LF_Utils.SpawnEntityPrefab(COUNTER, counterAt);
		ctx.NotNull(shelf, "a gondola spawns");
		ctx.NotNull(counter, "a counter spawns");
		if (!shelf || !counter)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shelf);
			SCR_EntityHelper.DeleteEntityAndChildren(counter);
			return;
		}

		// A point 0.1 m above the second board lands on that board.
		vector onBoard1 = LF_StoreComponent.SnapToSurfaceAt(shelfAt + Vector(0, GONDOLA_BOARD_1 + 0.1, 0.27));
		ctx.True(Math.AbsFloat(onBoard1[1] - (shelfAt[1] + GONDOLA_BOARD_1)) < 0.02, string.Format("a point over a gondola board lands on it (y %1, board %2)", onBoard1[1] - shelfAt[1], GONDOLA_BOARD_1));

		// The top board is 0.36 m above the third: the trace must start under it.
		vector onBoard2 = LF_StoreComponent.SnapToSurfaceAt(shelfAt + Vector(0, GONDOLA_BOARD_2 + 0.1, 0.2));
		ctx.True(Math.AbsFloat(onBoard2[1] - (shelfAt[1] + GONDOLA_BOARD_2)) < 0.02, string.Format("a point over the third board lands on it, not on the top board (y %1, board %2)", onBoard2[1] - shelfAt[1], GONDOLA_BOARD_2));

		// A counter top.
		vector onCounter = LF_StoreComponent.SnapToSurfaceAt(counterAt + Vector(0, COUNTER_TOP + 0.1, -0.15));
		ctx.True(Math.AbsFloat(onCounter[1] - (counterAt[1] + COUNTER_TOP)) < 0.02, string.Format("a point over a counter lands on its top (y %1)", onCounter[1] - counterAt[1]));

		// An apple already lying on the board is looked past: the next copy stands on the board.
		vector applePoint = shelfAt + Vector(0.4, GONDOLA_BOARD_1 + 0.03, 0.27);
		IEntity apple = LF_Utils.SpawnEntityPrefab(APPLE, applePoint);
		ctx.NotNull(apple, "an apple spawns on the board");
		vector pastApple = LF_StoreComponent.SnapToSurfaceAt(applePoint);
		ctx.True(Math.AbsFloat(pastApple[1] - (shelfAt[1] + GONDOLA_BOARD_1)) < 0.02, string.Format("a loose item on the board is looked past (y %1)", pastApple[1] - shelfAt[1]));

		// Over nothing: the written point.
		vector air = "40 300 40";
		ctx.True(vector.Distance(LF_StoreComponent.SnapToSurfaceAt(air), air) < 0.001, "a point over nothing stays where it was written");

		SCR_EntityHelper.DeleteEntityAndChildren(apple);
		SCR_EntityHelper.DeleteEntityAndChildren(shelf);
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
	}
}
