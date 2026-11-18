//------------------------------------------------------------------------------------------------
//! A test aid for what a test leaves standing (#2762, #2792). Describe() names what stands within
//! `RADIUS` metres of a spot, with its height, so a placement refused for no visible reason says what
//! was in the way. Watch() and Leftovers() find what a run added at the gate: the fixture characters
//! and loose props a test spawned and never deleted.
class LF_TestDebrisProbe
{
	protected static const float RADIUS = 9;

	protected ref array<string> m_aLines = {};
	protected ref array<EntityID> m_aBaseline = {};
	protected bool m_bLeftoversOnly;
	protected bool m_bBaselining;
	//! Entities a test keeps standing on purpose (Keep()); no static initializer, filled on first use.
	protected static ref array<EntityID> s_aKept;

	//------------------------------------------------------------------------------------------------
	//! A test that must leave an entity standing (e2e/pass1-write-save holds its bodies through the shutdown save
	//! the reload boot loads) declares it here, so the end-of-run check does not list it as a leak (#2792). Only
	//! a test whose assertions need the entity alive past the run may call this; every other test deletes what it spawns.
	static void Keep(IEntity entity)
	{
		if (!entity)
			return;

		if (!s_aKept)
			s_aKept = {};

		if (s_aKept.Find(entity.GetID()) == -1)
			s_aKept.Insert(entity.GetID());
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsKept(IEntity entity)
	{
		return s_aKept && s_aKept.Find(entity.GetID()) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The fixture spawn point (the gate): every LF_TestCharacterFixture character stands here, so test
	//! leftovers pile up here. vector.Zero when the world has no spawn point.
	static vector Gate()
	{
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		if (!spawnPoint)
			return vector.Zero;

		return spawnPoint.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Remembers what already stands near `at` (map props, the spawn point), so Leftovers() lists only what a test added.
	void Watch(vector at)
	{
		m_aBaseline.Clear();
		m_bBaselining = true;
		m_bLeftoversOnly = true;
		Describe(at);
		m_bLeftoversOnly = false;
		m_bBaselining = false;
	}

	//------------------------------------------------------------------------------------------------
	//! What stands near `at` that was not there at Watch(): one " | prefab@origin h=height" entry each, "" when nothing.
	string Leftovers(vector at)
	{
		m_bLeftoversOnly = true;
		string all = Describe(at);
		m_bLeftoversOnly = false;
		return all;
	}

	//------------------------------------------------------------------------------------------------
	//! How many entries the last Leftovers() or Describe() listed.
	int Count()
	{
		return m_aLines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! One entry per entity: prefab (or class), origin, bounds height.
	string Describe(vector at)
	{
		m_aLines.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(at, RADIUS, Visit);
		string all;
		foreach (string line : m_aLines)
			all = all + " | " + line;

		return all;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Visit(IEntity entity)
	{
		if (m_bLeftoversOnly && !IsLeftover(entity))
			return true;

		if (m_bBaselining)
		{
			m_aBaseline.Insert(entity.GetID());
			return true;
		}

		if (m_bLeftoversOnly && m_aBaseline.Find(entity.GetID()) != -1)
			return true;

		string name = NameOf(entity);
		if (name.IsEmpty())
			name = entity.ClassName();

		vector mins;
		vector maxs;
		entity.GetWorldBounds(mins, maxs);
		m_aLines.Insert(string.Format("%1@%2 h=%3", name, entity.GetOrigin(), maxs[1] - mins[1]));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The file name of the prefab the entity was spawned from; "" for an entity spawned by code with none (a ghost, a booth part).
	static string NameOf(IEntity entity)
	{
		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return "";

		return FilePath.StripPath(data.GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	//! A character, or a prop spawned by code with no prefab (a GenericEntity with a mesh): what a test spawns
	//! and forgets. What a character wears or carries goes with it, so only the top of each tree counts.
	protected static bool IsLeftover(IEntity entity)
	{
		if (entity.GetParent() || IsKept(entity))
			return false;

		if (ChimeraCharacter.Cast(entity))
			return true;

		return entity.ClassName() == "GenericEntity" && NameOf(entity).IsEmpty();
	}
}
