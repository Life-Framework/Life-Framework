//------------------------------------------------------------------------------------------------
//! Shared WORLD-tier fixture: a character spawned the way LF_SpawnLogic does (WORLD transform at
//! a spawn point) wearing a jacket, so MoneyStack has a deposit storage whatever the prefab's
//! default loadout is. Same shape as the setup LF_Test_E2E_Narrative proved.
//! Every step that does not resolve is a test FAIL with its reason, never a silent skip.
class LF_TestCharacterFixture
{
	static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	//! A spot is taken when a character stands this close (a body is about 0.6 m wide).
	static const float CROWD_M = 1.0;
	static const float RING_NEAR_M = 1.25;
	static const float RING_FAR_M = 2.5;
	static const int RING_STEPS = 8;

	//------------------------------------------------------------------------------------------------
	//! \return The character, or null after recording the FAIL reason on ctx.
	static IEntity Spawn(LF_TestContext ctx)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		if (!res.IsValid())
		{
			ctx.Fail("fixture: character prefab does not load: " + CHARACTER_PREFAB);
			return null;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		if (!spawnPoint)
		{
			ctx.Fail("fixture: no spawn point resolves for the character");
			return null;
		}

		// The point's own origin, not GetPositionAndRotation: the spawn point prefab sets m_fRandomSpawnRadius 2, so
		// that call returns a different spot within 2 m on every call. Two fixture characters landed 1 to 4 m apart
		// by chance, and a test that reached from one to the other flaked (lockers/lost-and-found, #4323).
		vector position = spawnPoint.GetOrigin();
		Math3D.AnglesToMatrix(spawnPoint.GetAngles(), params.Transform);
		params.Transform[3] = FreeSpot(position) + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!character)
		{
			ctx.Fail("fixture: character prefab did not spawn an entity");
			return null;
		}

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!storage)
		{
			ctx.Fail("fixture: character inventory storage manager did not resolve");
			Delete(character);
			return null;
		}

		IEntity jacket = LF_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		if (!jacket)
		{
			ctx.Fail("fixture: jacket did not spawn: " + JACKET_PREFAB);
			Delete(character);
			return null;
		}

		if (!storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
		{
			ctx.Fail("fixture: jacket did not insert into the character loadout");
			Delete(jacket);
			Delete(character);
			return null;
		}

		LF_TestFixtureCrowd.Note(character);
		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a new character stands: the spawn point itself when no character is within CROWD_M of it,
	//! else the first free spot on a ring around it, so a second body stands a known 1.25 m away and not on
	//! top of the first (#4323). The first character stands exactly on the point.
	static vector FreeSpot(vector spawnPoint)
	{
		LF_TestFixtureCrowd crowd = new LF_TestFixtureCrowd();
		if (!crowd.Near(spawnPoint, CROWD_M))
			return spawnPoint;

		array<float> radii = {RING_NEAR_M, RING_FAR_M};
		foreach (float radius : radii)
		{
			for (int step = 0; step < RING_STEPS; step++)
			{
				float angle = step * Math.PI2 / RING_STEPS;
				vector spot = spawnPoint + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
				if (!crowd.Near(spot, CROWD_M))
					return spot;
			}
		}

		LF_Debug.Warn("TestFixture", "no free spot around the spawn point; the engine will push the character: " + spawnPoint.ToString());
		return spawnPoint;
	}

	//------------------------------------------------------------------------------------------------
	static void Delete(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the character holds the prefab anywhere. A weapon lands in the equipped-weapon
	//! storage, which the deposit-count helpers skip, so that storage is checked too.
	static bool HasItem(IEntity character, ResourceName prefab)
	{
		SCR_InventoryStorageManagerComponent manager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (manager && LF_InventoryUtils.FindItemsByPrefab(manager, prefab).Count() > 0)
			return true;

		EquipedWeaponStorageComponent weaponStorage = LF_Component<EquipedWeaponStorageComponent>.Find(character);
		if (!weaponStorage)
			return false;

		array<IEntity> weapons = {};
		weaponStorage.GetAll(weapons);
		foreach (IEntity weapon : weapons)
		{
			if (LF_Utils.GetPrefabName(weapon) == prefab)
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Whether a character stands near a spot (the fixture's crowd check). The sphere query does not
//! list a body spawned earlier in the same frame, and a test run is one frame, so the fixture also
//! remembers the characters it spawned.
class LF_TestFixtureCrowd
{
	//! Characters the fixture spawned; filled on first use (no static initializer).
	protected static ref array<EntityID> s_aSpawned;
	//! The query matches bounding volumes, so it asks with a small sphere; the CROWD_M distance check is on the remembered bodies.
	protected static const float QUERY_M = 0.3;
	protected bool m_bFound;

	//------------------------------------------------------------------------------------------------
	static void Note(IEntity character)
	{
		if (!character)
			return;

		if (!s_aSpawned)
			s_aSpawned = {};

		s_aSpawned.Insert(character.GetID());
	}

	//------------------------------------------------------------------------------------------------
	bool Near(vector at, float radius)
	{
		m_bFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(at, QUERY_M, Visit);
		if (m_bFound || !s_aSpawned)
			return m_bFound;

		for (int i = s_aSpawned.Count() - 1; i >= 0; i--)
		{
			IEntity spawned = GetGame().GetWorld().FindEntityByID(s_aSpawned[i]);
			if (!spawned)
			{
				s_aSpawned.Remove(i);
				continue;
			}

			if (vector.Distance(spawned.GetOrigin(), at) < radius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Visit(IEntity entity)
	{
		if (!entity.Type().IsInherited(SCR_ChimeraCharacter))
			return true;

		m_bFound = true;
		return false;
	}
}
