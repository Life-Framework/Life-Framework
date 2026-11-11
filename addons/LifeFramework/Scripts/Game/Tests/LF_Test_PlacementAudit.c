// tier: WORLD
// red-proof: 2026-11-11, LF_PlacementAudit.Flags with the floating test at GAP + 5: `--only LF_Test_PlacementAudit` failed "a crate 1 m above the ground is flagged floating (flags=)" while the log measured gap=0.999. Restored, green.
//! The placement audit (LF_PlacementAudit, `tools\cli audit placement`) on fixtures spawned on open
//! ground south of the DebugWorld town (300, 100): a crate on the ground and one stacked on it are clean, a crate 1 m up is floating, a crate
//! at three times its size is scaled, and a crate pushed half off a table overhangs without floating.
class LF_Test_PlacementAudit : LF_Test
{
	protected static const ResourceName CRATE = "{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et";
	protected static const ResourceName TABLE = "{622F9989DDC4865B}PrefabLibrary/Props/Civilian/Recreation/TableRecreation_01/TableRecreation_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "agent-harness/placement-audit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		vector spot = Vector(300, world.GetSurfaceY(300, 100), 100);
		array<IEntity> spawned = {};

		IEntity grounded = LF_Utils.SpawnEntityPrefab(CRATE, spot);
		IEntity floating = LF_Utils.SpawnEntityPrefab(CRATE, Ground(world, spot + "4 0 0") + "0 1 0");
		IEntity scaled = LF_Utils.SpawnEntityPrefab(CRATE, Ground(world, spot + "8 0 0"));
		IEntity table = LF_Utils.SpawnEntityPrefab(TABLE, Ground(world, spot + "0 0 6"));
		spawned.Insert(grounded);
		spawned.Insert(floating);
		spawned.Insert(scaled);
		spawned.Insert(table);
		ctx.NotNull(grounded, "the grounded crate spawned");
		ctx.NotNull(floating, "the floating crate spawned");
		ctx.NotNull(scaled, "the scaled crate spawned");
		ctx.NotNull(table, "the table spawned");
		if (!grounded || !floating || !scaled || !table)
		{
			Cleanup(spawned);
			return;
		}

		scaled.SetScale(3);
		Physics scaledPhysics = scaled.GetPhysics();
		if (scaledPhysics)
			scaledPhysics.UpdateGeometries();

		LF_PlacementMeasure ground = LF_PlacementAudit.Measure(grounded, false);
		string groundFlags = LF_PlacementAudit.Flags(ground);
		LF_Debug.Info("PlaceAudit", "grounded crate " + LF_PlacementAudit.Format(ground));
		ctx.True(groundFlags.IsEmpty(), "a crate on the ground is not flagged (flags=" + groundFlags + ")");

		LF_PlacementMeasure up = LF_PlacementAudit.Measure(floating, false);
		string upFlags = LF_PlacementAudit.Flags(up);
		LF_Debug.Info("PlaceAudit", "floating crate " + LF_PlacementAudit.Format(up));
		ctx.True(upFlags.Contains("floating"), "a crate 1 m above the ground is flagged floating (flags=" + upFlags + ")");
		ctx.EqualFloat(1.0, up.m_fGap, 0.1, "its gap is the 1 m it was lifted");

		LF_PlacementMeasure big = LF_PlacementAudit.Measure(scaled, false);
		string bigFlags = LF_PlacementAudit.Flags(big);
		LF_Debug.Info("PlaceAudit", "scaled crate " + LF_PlacementAudit.Format(big));
		ctx.True(bigFlags.Contains("scaled"), "a crate at three times its size is flagged scaled (flags=" + bigFlags + ")");
		ctx.EqualFloat(ground.m_vHalf[0] * 3, big.m_vHalf[0], 0.05, "the oriented box carries the scale");

		// A crate stacked on the grounded one rests on it: the crate's top is a surface (2026-11-11: the
		// projectile-layer trace went through BoxWooden_01 and called every stacked crate floating).
		IEntity stacked = LF_Utils.SpawnEntityPrefab(CRATE, grounded.GetOrigin() + Vector(0, ground.m_vMaxs[1] - ground.m_vMins[1], 0));
		spawned.Insert(stacked);
		ctx.NotNull(stacked, "the stacked crate spawned");
		if (stacked)
		{
			LF_PlacementMeasure top = LF_PlacementAudit.Measure(stacked, false);
			string topFlags = LF_PlacementAudit.Flags(top);
			LF_Debug.Info("PlaceAudit", "stacked crate " + LF_PlacementAudit.Format(top));
			ctx.True(topFlags.IsEmpty(), "a crate stacked on another is not flagged (flags=" + topFlags + ", on=" + top.m_sSupport + ")");
		}

		// Push a crate a third of its width past the table's long edge.
		string topLabel;
		vector tmin;
		vector tmax;
		table.GetWorldBounds(tmin, tmax);
		float cz = (tmin[2] + tmax[2]) * 0.5;
		float topY = LF_PlacementAudit.SupportAt(null, (tmin[0] + tmax[0]) * 0.5, cz, tmax[1] + 0.5, topLabel);
		float cx = tmax[0] - ground.m_vHalf[0] * 0.35;
		IEntity edge = LF_Utils.SpawnEntityPrefab(CRATE, Vector(cx, topY, cz) + Vector(0, grounded.GetOrigin()[1] - ground.m_vMins[1], 0));
		spawned.Insert(edge);
		ctx.NotNull(edge, "the edge crate spawned");
		if (edge)
		{
			LF_PlacementMeasure over = LF_PlacementAudit.Measure(edge, false);
			string overFlags = LF_PlacementAudit.Flags(over);
			LF_Debug.Info("PlaceAudit", string.Format("edge crate on %1 (top %2) %3", topLabel, topY, LF_PlacementAudit.Format(over)));
			ctx.True(overFlags.Contains("overhang"), "a crate half off the table is flagged overhang (flags=" + overFlags + ")");
			ctx.True(!overFlags.Contains("floating"), "it is not floating: its centre rests on the table");
		}

		Cleanup(spawned);
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Ground(BaseWorld world, vector p)
	{
		return Vector(p[0], world.GetSurfaceY(p[0], p[2]), p[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(array<IEntity> spawned)
	{
		foreach (IEntity entity : spawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}
}
