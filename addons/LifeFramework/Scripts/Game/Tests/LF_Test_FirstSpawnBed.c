// red-proof: observed 2026-11-16 with `tools/cli test --no-build --red LF_Test_FirstSpawnBed` (RED PROOF OK): with the fixture beds not spawned, "the second new character lands on a bed too" and the other bed assertions failed; green on the real code.
// tier: WORLD
//! #1965: a new character's first spawn lands on a free first-spawn bed, the next new character on
//! the next bed, and a spawn that is not a first spawn (a respawn after a death, a rejoin) or a
//! world with every bed taken takes the normal spawn point.
class LF_Test_FirstSpawnBed : LF_Test
{
	static const ResourceName BED_PREFAB = "{15468371DAC524FC}Prefabs/Medic/LF_FirstSpawnBed.et";
	protected static const string FIRST_ID = "firstspawnbed-first";
	protected static const string SECOND_ID = "firstspawnbed-second";
	protected static const string OLD_ID = "firstspawnbed-existing";
	//! Far from the grid annex (x 18..114, z 18..114), the hospital ward fixture (400, 560) and the gate spawn.
	static const float BED_X = 520;
	static const float BED_Z = 640;

	override string GetName()
	{
		return "spawn/first-spawn-bed";
	}

	override void Run(LF_TestContext ctx)
	{
		// Pure pick: nothing to lie in, nothing free.
		array<vector> none = {};
		array<vector> nobody = {};
		ctx.Equal(-1, LF_FirstSpawnBeds.PickFree(none, nobody), "no bed, no pick");
		ctx.Equal(-1, LF_FirstSpawnBeds.PickFree(null, nobody), "a missing list gives no pick");

		array<vector> row = {};
		row.Insert(Vector(10, 0, 10));
		row.Insert(Vector(12, 0, 10));
		ctx.Equal(0, LF_FirstSpawnBeds.PickFree(row, nobody), "an empty row starts at the first bed");
		array<vector> lying = {};
		lying.Insert(Vector(10.2, 0, 10.1));
		ctx.Equal(1, LF_FirstSpawnBeds.PickFree(row, lying), "a bed with someone in it is skipped");
		lying.Insert(Vector(12, 0, 10));
		ctx.Equal(-1, LF_FirstSpawnBeds.PickFree(row, lying), "a full row has no free bed");

		// The fixture row: two beds the test spawns and deletes.
		array<IEntity> fixture = {};
		if (!RedProof())
		{
			for (int i = 0; i < 2; i++)
			{
				IEntity bed = LF_Utils.SpawnEntityPrefab(BED_PREFAB, Vector(BED_X + i * 2.5, 0, BED_Z));
				ctx.True(bed != null, "fixture: a first-spawn bed spawns");
				if (bed)
					fixture.Insert(bed);
			}
		}

		array<LF_FirstSpawnBedComponent> beds = {};
		LF_FirstSpawnBeds.GetBeds(beds);
		ctx.True(beds.Count() >= 2, string.Format("the world has first-spawn beds (found %1)", beds.Count()));
		array<vector> wake = {};
		foreach (LF_FirstSpawnBedComponent registered : beds)
		{
			vector transform[4];
			registered.GetWakeTransform(transform);
			wake.Insert(transform[3]);
		}

		array<ref LF_FirstSpawnRow> before = LF_FirstSpawn.ExportAll();
		LF_FirstSpawn.ApplyAll(null);
		LF_FirstSpawn.Mark(FIRST_ID);
		LF_FirstSpawn.Mark(SECOND_ID);

		vector position, angles;
		array<vector> taken = {};
		bool landed = LF_FirstSpawnBeds.Resolve(1, FIRST_ID, position, angles, taken);
		ctx.True(landed, "a new character's first spawn lands on a bed");
		ctx.True(wake.Find(position) >= 0, "and the spot is a registered bed's wake point");
		vector firstSpot = position;

		// The first body now lies there: the next new character takes the next bed.
		taken.Insert(firstSpot);
		vector secondPosition, secondAngles;
		bool secondLanded = LF_FirstSpawnBeds.Resolve(2, SECOND_ID, secondPosition, secondAngles, taken);
		ctx.True(secondLanded, "the second new character lands on a bed too");
		ctx.True(wake.Find(secondPosition) >= 0, "the second spot is a registered bed's wake point");
		ctx.True(vector.DistanceSqXZ(secondPosition, firstSpot) > 1, "the second character takes a different bed");

		// A taken bed is never offered: with every bed taken the spawn falls back to the normal point.
		vector fullPosition, fullAngles;
		ctx.False(LF_FirstSpawnBeds.Resolve(3, FIRST_ID, fullPosition, fullAngles, wake), "every bed taken takes the normal spawn");

		// A respawn is not a first spawn: once the mark is consumed, or for a character that was never marked.
		ctx.True(LF_FirstSpawn.Consume(FIRST_ID), "the first spawn clears the mark");
		vector laterPosition, laterAngles;
		ctx.False(LF_FirstSpawnBeds.Resolve(1, FIRST_ID, laterPosition, laterAngles, none), "a respawn takes the normal spawn");
		ctx.False(LF_FirstSpawnBeds.Resolve(4, OLD_ID, laterPosition, laterAngles, none), "an existing character takes the normal spawn");
		ctx.False(LF_FirstSpawnBeds.Resolve(5, "", laterPosition, laterAngles, none), "an empty id takes the normal spawn");

		// The spawn logic's bed flags for the two players above go away with their (here absent) bodies.
		LF_FirstSpawnBeds.OnFirstSpawn(1, null);
		LF_FirstSpawnBeds.OnFirstSpawn(2, null);

		foreach (IEntity placed : fixture)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(placed);
		}

		array<LF_FirstSpawnBedComponent> after = {};
		LF_FirstSpawnBeds.GetBeds(after);
		ctx.Equal(beds.Count() - fixture.Count(), after.Count(), "a deleted bed leaves the list");

		LF_FirstSpawn.ApplyAll(before);
	}
}
