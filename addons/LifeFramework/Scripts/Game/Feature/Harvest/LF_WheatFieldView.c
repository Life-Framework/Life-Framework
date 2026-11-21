// LF_WheatFieldView.c: the local meshes of one wheat field (#3287, harvester spike). One patch per standing cell,
// one stubble per cut cell, all `SpawnEntityPrefabLocal` entities (never replicated; each machine draws its own
// from the cut bitset, like LF_PlantReplication). A dedicated server draws nothing and never builds one.

//------------------------------------------------------------------------------------------------
//! The drawn side of a field. It holds an entity id per cell for the patch and for the stubble and keeps
//! exactly one of the two alive for each cell. It knows the grid only to place a cell's middle.
class LF_WheatFieldView
{
	protected ref array<EntityID> m_aPatch = {};
	protected ref array<EntityID> m_aStubble = {};
	protected LF_WheatGrid m_Grid;
	protected ResourceName m_PatchPrefab;
	protected ResourceName m_StubblePrefab;
	protected float m_fGroundY;
	protected int m_iPatches;
	protected int m_iStubbles;

	//------------------------------------------------------------------------------------------------
	int PatchCount()
	{
		return m_iPatches;
	}

	//------------------------------------------------------------------------------------------------
	int StubbleCount()
	{
		return m_iStubbles;
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the whole field from the grid: a patch for every standing cell, stubble for every cut one.
	//! `fallbackY` is the height used when the terrain has no surface under a cell. Any earlier drawing goes first.
	void Build(LF_WheatGrid grid, ResourceName patchPrefab, ResourceName stubblePrefab, float fallbackY)
	{
		Clear();
		if (!grid)
			return;

		m_Grid = grid;
		m_PatchPrefab = patchPrefab;
		m_StubblePrefab = stubblePrefab;
		m_fGroundY = fallbackY;
		int total = grid.CellCount();
		for (int cell = 0; cell < total; cell++)
		{
			m_aPatch.Insert(EntityID.INVALID);
			m_aStubble.Insert(EntityID.INVALID);
		}

		for (int draw = 0; draw < total; draw++)
		{
			if (grid.IsCut(draw))
				DrawStubble(draw);
			else
				DrawPatch(draw);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A cell was cut: its patch goes and stubble takes its place. False when the cell has no patch to swap.
	bool Cut(int cell)
	{
		if (cell < 0 || cell >= m_aPatch.Count() || m_aPatch[cell] == EntityID.INVALID)
			return false;

		RemoveEntity(m_aPatch[cell]);
		m_aPatch[cell] = EntityID.INVALID;
		m_iPatches--;
		DrawStubble(cell);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The field grew back: every stubble goes and every cell has its patch again.
	void Regrow()
	{
		for (int cell = 0; cell < m_aPatch.Count(); cell++)
		{
			if (m_aStubble[cell] != EntityID.INVALID)
			{
				RemoveEntity(m_aStubble[cell]);
				m_aStubble[cell] = EntityID.INVALID;
				m_iStubbles--;
			}

			if (m_aPatch[cell] == EntityID.INVALID)
				DrawPatch(cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every local entity this view spawned.
	void Clear()
	{
		for (int cell = 0; cell < m_aPatch.Count(); cell++)
		{
			RemoveEntity(m_aPatch[cell]);
			RemoveEntity(m_aStubble[cell]);
		}

		m_aPatch.Clear();
		m_aStubble.Clear();
		m_iPatches = 0;
		m_iStubbles = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPatch(int cell)
	{
		EntityID id = Spawn(m_PatchPrefab, cell);
		m_aPatch[cell] = id;
		if (id != EntityID.INVALID)
			m_iPatches++;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawStubble(int cell)
	{
		EntityID id = Spawn(m_StubblePrefab, cell);
		m_aStubble[cell] = id;
		if (id != EntityID.INVALID)
			m_iStubbles++;
	}

	//------------------------------------------------------------------------------------------------
	//! One local entity at a cell's middle, on the ground; EntityID.INVALID when the prefab fails (logged).
	protected EntityID Spawn(ResourceName prefab, int cell)
	{
		float x;
		float z;
		m_Grid.CellCentre(cell, x, z);
		float y = m_fGroundY;
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetWorld())
			y = game.GetWorld().GetSurfaceY(x, z);

		// A cell's patch faces a stable pseudo-random way so a field does not look combed.
		int turn = cell * 137;
		turn = turn % 360;
		float yaw = turn;
		IEntity entity = LF_Utils.SpawnEntityPrefab(prefab, Vector(x, y, z), Vector(yaw, 0, 0), false);
		if (!entity)
		{
			LF_Debug.Error("Harvest", string.Format("wheat field: cell %1: prefab failed to spawn locally: %2", cell, prefab));
			return EntityID.INVALID;
		}

		return entity.GetID();
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveEntity(EntityID id)
	{
		if (id == EntityID.INVALID)
			return;

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
			return;

		IEntity entity = game.GetWorld().FindEntityByID(id);
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
