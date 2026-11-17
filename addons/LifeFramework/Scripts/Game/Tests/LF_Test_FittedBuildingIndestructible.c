// red-proof: observed 2026-11-17 with `--red LF_Test_FittedBuildingIndestructible` on DebugWorld (spawned garages): `RED PROOF OK`; with the fit skipped, `explosive and true damage past full health leave the fitted building's health alone (expected 10000, got 0)`, `the fitted building is not destroyed` and `the fitted building takes no damage` failed. Green: 10 assertions.
// tier: WORLD
//! D15: a fitted vanilla building is indestructible, its unfitted neighbour is not. On a world with
//! vanilla buildings (CoreOnEveron) the test takes the two destructible buildings nearest the map's
//! middle; on DebugWorld it spawns two vanilla garages. It fits the first through LF_FittedBuildings
//! from a point inside it (what LF_FittedBuildingComponent does from the fit layer's marker), hits it
//! with explosive and true damage past its full health, and hits the neighbour with true damage:
//! the fitted one keeps its health and stays standing, the neighbour loses health.
//!   tools\cli test --no-build --only LF_Test_FittedBuildingIndestructible --world Worlds/CoreOnEveron/CoreOnEveron.ent
class LF_Test_FittedBuildingIndestructible : LF_Test
{
	static const ResourceName GARAGE = "{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et";
	protected static const string PLACE_ID = "test.fitted-building";

	protected ref array<IEntity> m_aFound = {};

	override string GetName()
	{
		return "world/fitted-building-indestructible";
	}

	override void Run(LF_TestContext ctx)
	{
		array<IEntity> spawned = {};
		IEntity fitted;
		IEntity neighbour;
		if (!PickBuildings(fitted, neighbour, spawned))
		{
			ctx.Fail("no two destructible buildings found or spawned");
			Cleanup(null, spawned);
			return;
		}

		LF_Debug.Info(LF_FittedBuildings.FEATURE, string.Format("fitted-building test: fitting %1 at %2, neighbour %3 at %4 (spawned %5)", fitted.GetPrefabData().GetPrefabName(), fitted.GetOrigin(), neighbour.GetPrefabData().GetPrefabName(), neighbour.GetOrigin(), spawned.Count()));

		vector mins;
		vector maxs;
		fitted.GetWorldBounds(mins, maxs);
		vector inside = (mins + maxs) * 0.5;

		IEntity got;
		if (!RedProof())
			got = LF_FittedBuildings.Fit(inside, 25, PLACE_ID);

		ctx.True(got == fitted, "the marker inside the building fits that building");
		ctx.True(LF_FittedBuildings.IsFitted(fitted), "the fitted building is registered");
		ctx.False(LF_FittedBuildings.IsFitted(neighbour), "the neighbour is not registered");

		DamageManagerComponent fittedDamage = DamageManagerComponent.Cast(fitted.FindComponent(SCR_DestructibleBuildingComponent));
		DamageManagerComponent neighbourDamage = DamageManagerComponent.Cast(neighbour.FindComponent(SCR_DestructibleBuildingComponent));
		float fittedBefore = fittedDamage.GetHealth();
		float neighbourBefore = neighbourDamage.GetHealth();
		ctx.True(fittedBefore > 0 && neighbourBefore > 0, "both buildings start standing");

		Hit(fittedDamage, fitted, EDamageType.EXPLOSIVE, fittedDamage.GetMaxHealth() * 2);
		Hit(fittedDamage, fitted, EDamageType.TRUE, fittedDamage.GetMaxHealth() * 2);
		ctx.EqualFloat(fittedBefore, fittedDamage.GetHealth(), 0.01, "explosive and true damage past full health leave the fitted building's health alone");
		ctx.False(fittedDamage.IsDestroyed(), "the fitted building is not destroyed");
		ctx.False(fittedDamage.IsDamageHandlingEnabled(), "the fitted building takes no damage");

		Hit(neighbourDamage, neighbour, EDamageType.TRUE, neighbourDamage.GetMaxHealth() * 0.4);
		ctx.True(neighbourDamage.GetHealth() < neighbourBefore, string.Format("the unfitted neighbour still takes damage (%1 -> %2)", neighbourBefore, neighbourDamage.GetHealth()));
		ctx.False(neighbourDamage.IsDestroyed(), "40% damage leaves the neighbour standing");
		neighbourDamage.SetHealthScaled(1);

		Cleanup(fitted, spawned);
		ctx.False(LF_FittedBuildings.IsFitted(fitted), "release clears the registration");
	}

	//------------------------------------------------------------------------------------------------
	protected void Hit(DamageManagerComponent damage, IEntity target, EDamageType type, float amount)
	{
		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = target.GetOrigin() + Vector(0, 1, 0);
		hitPosDirNorm[1] = vector.Up * -1;
		hitPosDirNorm[2] = vector.Up;
		SCR_DamageContext context = new SCR_DamageContext(type, amount, hitPosDirNorm, target, damage.GetDefaultHitZone(), Instigator.CreateInstigator(null), null, -1, -1);
		damage.HandleDamage(context);
	}

	//------------------------------------------------------------------------------------------------
	//! Two standing destructible buildings near the map's middle, or two spawned garages.
	protected bool PickBuildings(out IEntity fitted, out IEntity neighbour, notnull array<IEntity> spawned)
	{
		BaseWorld world = GetGame().GetWorld();
		vector mins;
		vector maxs;
		world.GetBoundBox(mins, maxs);
		vector middle = (mins + maxs) * 0.5;
		for (int ring = 0; ring <= 3 && m_aFound.Count() < 2; ring++)
		{
			for (int x = -ring; x <= ring && m_aFound.Count() < 2; x++)
			{
				for (int z = -ring; z <= ring && m_aFound.Count() < 2; z++)
				{
					if (Math.AbsInt(x) != ring && Math.AbsInt(z) != ring)
						continue;

					m_aFound.Clear();
					world.QueryEntitiesBySphere(middle + Vector(x * 400, 0, z * 400), 200, CollectStanding, null, EQueryEntitiesFlags.STATIC);
				}
			}
		}

		if (m_aFound.Count() < 2)
		{
			m_aFound.Clear();
			array<vector> spots = {"60 0 140", "100 0 140"};
			for (int i = 0; i < spots.Count(); i++)
			{
				vector at = spots[i];
				at[1] = world.GetSurfaceY(at[0], at[2]);
				IEntity garage = LF_Utils.SpawnEntityPrefab(GARAGE, at);
				if (garage)
				{
					spawned.Insert(garage);
					m_aFound.Insert(garage);
				}
			}
		}

		if (m_aFound.Count() < 2)
			return false;

		fitted = m_aFound[0];
		neighbour = m_aFound[1];
		m_aFound.Clear();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectStanding(IEntity entity)
	{
		if (m_aFound.Count() >= 2 || !entity || entity.GetParent())
			return true;

		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(SCR_DestructibleBuildingComponent));
		if (!damage || damage.IsDestroyed() || !damage.IsDamageHandlingEnabled() || LF_FittedBuildings.IsFitted(entity))
			return true;

		foreach (IEntity other : m_aFound)
		{
			if (vector.Distance(other.GetOrigin(), entity.GetOrigin()) < 30)
				return true;
		}

		m_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity fitted, array<IEntity> spawned)
	{
		if (fitted)
			LF_FittedBuildings.Release(fitted);

		foreach (IEntity e : spawned)
		{
			if (e)
				SCR_EntityHelper.DeleteEntityAndChildren(e);
		}
	}
}
