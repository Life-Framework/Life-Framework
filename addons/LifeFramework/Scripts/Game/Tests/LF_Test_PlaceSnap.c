// red-proof: `tools\cli test --no-build --red LF_Test_PlaceSnap` skips the Settle; every board check then fails (pivot 0.1 m over the board, the backpack 1.19 m).

//! LF_PlaceSnap rests an item's measured bottom on the board under its footprint (placement spike). A
//! ShelfUniversal_01_wide carries a solid proxy box round the whole unit beside its trimesh boards; the old store
//! trace started inside that box and left goods in the air (24 cm in the general store). Items whose pivot is not
//! their base are in the set: the ALICE backpack (pivot 1.09 m under the mesh), the M9 (8 cm above its base), the
//! vice (10 cm above). Each line also logs where the old store snap put the same item, for the spike's table.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlaceSnap : LF_Test
{
	static const ResourceName SHELF = "{D6E93EF9D3232D16}Prefabs/Props/Commercial/ShelfUniversal_01_wide.et";
	static const ResourceName GONDOLA = "{B7669B7F8B7923F6}Prefabs/Props/Commercial/ShelfShop_01.et";
	static const ResourceName COUNTER = "{E214337C6E1931C5}Prefabs/Props/Commercial/CounterShop_01.et";
	//! Board tops in prefab space (tools\cli measure, collider faces up).
	static const float SHELF_BOARD_2 = 0.588;
	static const float SHELF_BOARD_3 = 1.081;
	static const float GONDOLA_BOARD_1 = 0.428;
	static const float COUNTER_TOP = 1.039;
	static const float TOLERANCE = 0.003;

	override string GetName()
	{
		return "place/snap-contact";
	}

	//------------------------------------------------------------------------------------------------
	protected void Case(LF_TestContext ctx, string where, vector surfaceAt, float boardY, ResourceName item, vector at)
	{
		vector pivot = at + Vector(0, boardY + 0.1, 0);
		IEntity e = LF_Utils.SpawnEntityPrefab(item, pivot);
		ctx.NotNull(e, string.Format("%1 spawns", item));
		if (!e)
			return;

		// The footprint centre goes on the spot, not the pivot (the backpack's mesh is 0.3 m off its pivot).
		LF_PlaceSnap.CentreOn(e, at[0], at[2]);
		pivot = e.GetOrigin();
		// The old store rule, for the comparison: the pivot on the trace hit, plus 2 cm.
		vector oldPivot = LF_StoreComponent.SnapToSurfaceAt(pivot) + "0 0.02 0";
		vector mins;
		vector maxs;
		e.GetBounds(mins, maxs);
		float oldGap = oldPivot[1] + mins[1] * e.GetScale() - (surfaceAt[1] + boardY);

		int started = System.GetTickCount();
		LF_PlaceSnapResult r;
		if (!RedProof())
			r = LF_PlaceSnap.Settle(e);
		else
			r = LF_PlaceSnap.Measure(e);
		int ms = System.GetTickCount() - started;

		float board = surfaceAt[1] + boardY;
		bool ok = r && Math.AbsFloat(r.m_fBottomY - board) < TOLERANCE && Math.AbsFloat(r.m_fGap) < TOLERANCE;
		string line = string.Format("%1 %2: old gap %3, new %4 (%5 ms)", where, LF_PlacementAudit.PrefabBase(e), oldGap, "none", ms);
		if (r)
			line = string.Format("%1 %2: old gap %3, new bottom-board %4 gap %5 (%6) %7 ms", where, LF_PlacementAudit.PrefabBase(e), oldGap, r.m_fBottomY - board, r.m_fGap, r.Format(), ms);
		LF_Debug.Info(LF_PlaceSnap.FEATURE, "probe " + line);
		ctx.True(ok, line);
		SCR_EntityHelper.DeleteEntityAndChildren(e);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector shelfAt = "60 300 40";
		vector gondolaAt = "64 300 40";
		vector counterAt = "68 300 40";
		IEntity shelf = LF_Utils.SpawnEntityPrefab(SHELF, shelfAt);
		IEntity gondola = LF_Utils.SpawnEntityPrefab(GONDOLA, gondolaAt);
		IEntity counter = LF_Utils.SpawnEntityPrefab(COUNTER, counterAt);
		ctx.NotNull(shelf, "a wide universal shelf spawns");
		ctx.NotNull(gondola, "a gondola spawns");
		ctx.NotNull(counter, "a counter spawns");
		if (shelf && gondola && counter)
		{
			array<ResourceName> items = {
				"{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et",
				"{70BC751317551D9B}Prefabs/Items/Equipment/Canteens/Canteen_Soviet_01.et",
				"{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et",
				"{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et",
				"{2B0BD0720F8FB3A7}Prefabs/Props/Industrial/Workbench_Vice_01.et"
			};
			foreach (ResourceName item : items)
			{
				Case(ctx, "shelf-board-2", shelfAt, SHELF_BOARD_2, item, shelfAt + "-0.72 0 0.21");
				Case(ctx, "shelf-board-3", shelfAt, SHELF_BOARD_3, item, shelfAt + "-0.72 0 0.21");
				Case(ctx, "gondola-board-1", gondolaAt, GONDOLA_BOARD_1, item, gondolaAt + "0 0 0.27");
				Case(ctx, "counter", counterAt, COUNTER_TOP, item, counterAt + "0 0 -0.15");
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(shelf);
		SCR_EntityHelper.DeleteEntityAndChildren(gondola);
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
	}
}
