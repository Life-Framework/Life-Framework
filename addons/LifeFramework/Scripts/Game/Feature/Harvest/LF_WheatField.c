// LF_WheatField.c: a placed wheat field (#3287, harvester spike). The entity keeps the cut bitset (LF_WheatGrid,
// #3285) and, on every machine that renders, one local patch per standing cell (LF_WheatFieldView). A server
// `CutCells` swaps each cut patch for stubble and `Regrow` stands the field up again. Replicating the cuts to
// clients is #3288 and the reaper that calls this is #3289; nothing here is persisted.

//------------------------------------------------------------------------------------------------
//! The look of every wheat field: the patch and stubble prefabs and the cell size.
//! Core file Configs/Harvest/LF_WheatFields.conf; the patch is a vanilla stand-in until the wheat asset lands (H5a).
[BaseContainerProps(configRoot: true)]
class LF_WheatFieldsConfig
{
	static const ResourceName CONFIG_PATH = "{231D30A51A15952A}Configs/Harvest/LF_WheatFields.conf";

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "One standing 3 m cell of wheat", "et")]
	ResourceName m_PatchPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The same cell after the reaper cut it", "et")]
	ResourceName m_StubblePrefab;

	[Attribute("3", UIWidgets.EditBox, "Metres across one cell")]
	float m_fCellSize;

	//------------------------------------------------------------------------------------------------
	//! The core file, or null (logged): the field then keeps its grid and draws nothing.
	static LF_WheatFieldsConfig Load()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error("Harvest", "wheat field config does not resolve, fields draw nothing: " + CONFIG_PATH);
			return null;
		}

		LF_WheatFieldsConfig config = LF_WheatFieldsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("Harvest", "wheat field config has an unexpected root class: " + CONFIG_PATH);

		return config;
	}
}

//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "LifeFramework/Harvest", description: "A wheat field the reaper cuts cell by cell", color: "220 190 60 255", dynamicBox: true)]
class LF_WheatFieldClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
//! The box is axis-aligned and centred on the entity (its yaw is ignored): `m_fSizeX` along world X and
//! `m_fSizeZ` along world Z, cut into cells of the config's size. A 60 x 40 m box is 260 cells.
class LF_WheatField : GenericEntity
{
	static const string FEATURE = "Harvest";

	[Attribute("60", UIWidgets.EditBox, "Metres along world X", category: "Wheat Field")]
	float m_fSizeX;

	[Attribute("40", UIWidgets.EditBox, "Metres along world Z", category: "Wheat Field")]
	float m_fSizeZ;

	protected ref LF_WheatGrid m_Grid;
	protected ref LF_WheatFieldView m_View;
	protected ref LF_WheatFieldsConfig m_Config;
	protected bool m_bReady;

	//------------------------------------------------------------------------------------------------
	void LF_WheatField(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_WheatField()
	{
		if (m_View)
			m_View.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A dedicated server renders nothing, so it keeps the bitset and draws no patch.
	static bool DrawsHere()
	{
		return RplSession.Mode() != RplMode.Dedicated && !System.IsConsoleApp();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		EnsureReady();
		if (DrawsHere())
			ShowLocal();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the grid once, from the box and the config.
	protected void EnsureReady()
	{
		if (m_bReady)
			return;

		m_bReady = true;
		m_Config = LF_WheatFieldsConfig.Load();
		float cell = LF_WheatFieldRules.CELL_SIZE;
		if (m_Config && m_Config.m_fCellSize > 0)
			cell = m_Config.m_fCellSize;

		vector centre = GetOrigin();
		m_Grid = LF_WheatGrid.Create(centre[0] - m_fSizeX * 0.5, centre[2] - m_fSizeZ * 0.5, m_fSizeX, m_fSizeZ, cell);
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: %2 x %3 cells of %4 m", GetName(), m_Grid.Columns(), m_Grid.Rows(), cell.ToString(-1, 1)));
	}

	//------------------------------------------------------------------------------------------------
	//! Draws this machine's local patches and stubble from the current cut state. `force` draws on a machine
	//! that normally would not (a dedicated test server), so a test can count the meshes.
	void ShowLocal(bool force = false)
	{
		EnsureReady();
		if (!force && !DrawsHere())
			return;

		if (!m_Config || m_Config.m_PatchPrefab.IsEmpty() || m_Config.m_StubblePrefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("wheat field %1: the config has no patch or stubble prefab, nothing is drawn", GetName()));
			return;
		}

		if (!m_View)
			m_View = new LF_WheatFieldView();

		m_View.Build(m_Grid, m_Config.m_PatchPrefab, m_Config.m_StubblePrefab, GetOrigin()[1]);
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: drew %2 patches and %3 stubbles", GetName(), m_View.PatchCount(), m_View.StubbleCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every local mesh. The entity's destructor does the same; a test calls this first.
	void ClearLocal()
	{
		if (m_View)
			m_View.Clear();
	}

	//------------------------------------------------------------------------------------------------
	LF_WheatGrid Grid()
	{
		EnsureReady();
		return m_Grid;
	}

	//------------------------------------------------------------------------------------------------
	int CellCount()
	{
		EnsureReady();
		return m_Grid.CellCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Cells cut so far: the bitset's count, the same on every machine.
	int CutCount()
	{
		EnsureReady();
		return m_Grid.CutCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Local patches standing now; 0 where nothing is drawn.
	int PatchCount()
	{
		if (!m_View)
			return 0;

		return m_View.PatchCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Local stubble standing now; 0 where nothing is drawn.
	int StubbleCount()
	{
		if (!m_View)
			return 0;

		return m_View.StubbleCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Cuts these cells: each standing one is marked in the bitset and its patch becomes stubble. A cell that is
	//! already cut or out of range is skipped. Returns how many were newly cut (the reaper's yield).
	int CutCells(array<int> cells)
	{
		EnsureReady();
		if (!cells)
			return 0;

		int newlyCut = 0;
		foreach (int cell : cells)
		{
			if (!m_Grid.Cut(cell))
				continue;

			newlyCut++;
			if (m_View)
				m_View.Cut(cell);
		}

		if (newlyCut > 0)
			LF_Debug.Info(FEATURE, string.Format("wheat field %1: cut %2 cells, %3 of %4 now cut", GetName(), newlyCut, m_Grid.CutCount(), m_Grid.CellCount()));
		else
			LF_Debug.Info(FEATURE, string.Format("wheat field %1: nothing to cut in %2 cells (already cut or out of range)", GetName(), cells.Count()));

		return newlyCut;
	}

	//------------------------------------------------------------------------------------------------
	//! The field grows back: the bitset clears and every cell has its patch again.
	void Regrow()
	{
		EnsureReady();
		int was = m_Grid.CutCount();
		m_Grid.Reset();
		if (m_View)
			m_View.Regrow();

		LF_Debug.Info(FEATURE, string.Format("wheat field %1: regrown, %2 cut cells stand again", GetName(), was));
	}
}
