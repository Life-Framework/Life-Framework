// LF_WheatFieldRules.c: the pure math of a wheat field (#3285, spike docs/process harvester-spike section 3 and 4).
// No entity, no world, no clock. The server checks a hitched reaper against these rules every 200 ms; the field
// entity (#3287), the cut replication (#3288) and the reaper (#3289) call them and add the world.

//------------------------------------------------------------------------------------------------
//! Why a reaper cuts nothing this tick. OK cuts.
enum LF_EHarvestRefusal
{
	OK,
	TOO_FAST,
	STOPPED,
	HOPPER_FULL
}

//------------------------------------------------------------------------------------------------
//! A field's cut state: a grid of square cells over an axis-aligned box and one bit per cell in `array<int>`
//! words (32 cells a word, so 260 cells are 9 ints and a late joiner's snapshot stays tiny). A cell index is
//! `row * columns + column`, a column runs along world X from the box origin and a row along world Z.
//! Only whole cells count: a box that is not a multiple of the cell size leaves its remainder uncut margin.
class LF_WheatGrid
{
	protected float m_fOriginX;
	protected float m_fOriginZ;
	protected float m_fCell = LF_WheatFieldRules.CELL_SIZE;
	protected int m_iColumns;
	protected int m_iRows;
	protected int m_iCutCount;
	protected ref array<int> m_aWords = {};

	//------------------------------------------------------------------------------------------------
	//! A field whose south-west (lowest X and Z) corner is (originX, originZ) and whose box is sizeX by sizeZ
	//! metres. A box smaller than one cell still gets one cell; a zero or negative cell size falls back to 3 m.
	static LF_WheatGrid Create(float originX, float originZ, float sizeX, float sizeZ, float cellSize = 3)
	{
		LF_WheatGrid grid = new LF_WheatGrid();
		if (cellSize <= 0)
			cellSize = LF_WheatFieldRules.CELL_SIZE;

		grid.m_fOriginX = originX;
		grid.m_fOriginZ = originZ;
		grid.m_fCell = cellSize;
		grid.m_iColumns = LF_Utils.MaxInt(1, FloorInt(sizeX / cellSize));
		grid.m_iRows = LF_Utils.MaxInt(1, FloorInt(sizeZ / cellSize));
		int words = (grid.m_iColumns * grid.m_iRows + 31) / 32;
		for (int i = 0; i < words; i++)
		{
			grid.m_aWords.Insert(0);
		}
		return grid;
	}

	//------------------------------------------------------------------------------------------------
	protected static int FloorInt(float value)
	{
		int whole = Math.Floor(value);
		return whole;
	}

	//------------------------------------------------------------------------------------------------
	int Columns()
	{
		return m_iColumns;
	}

	//------------------------------------------------------------------------------------------------
	int Rows()
	{
		return m_iRows;
	}

	//------------------------------------------------------------------------------------------------
	int CellCount()
	{
		return m_iColumns * m_iRows;
	}

	//------------------------------------------------------------------------------------------------
	float CellSize()
	{
		return m_fCell;
	}

	//------------------------------------------------------------------------------------------------
	int CutCount()
	{
		return m_iCutCount;
	}

	//------------------------------------------------------------------------------------------------
	int CellIndex(int column, int row)
	{
		if (column < 0 || column >= m_iColumns || row < 0 || row >= m_iRows)
			return -1;
		return row * m_iColumns + column;
	}

	//------------------------------------------------------------------------------------------------
	//! The cell holding a world point (X and Z only), or -1 outside the grid.
	int CellAt(float x, float z)
	{
		float fx = (x - m_fOriginX) / m_fCell;
		float fz = (z - m_fOriginZ) / m_fCell;
		if (fx < 0 || fz < 0)
			return -1;
		return CellIndex(FloorInt(fx), FloorInt(fz));
	}

	//------------------------------------------------------------------------------------------------
	//! World X and Z of a cell's middle.
	void CellCentre(int cell, out float x, out float z)
	{
		int row = cell / m_iColumns;
		int column = cell - row * m_iColumns;
		x = m_fOriginX + (column + 0.5) * m_fCell;
		z = m_fOriginZ + (row + 0.5) * m_fCell;
	}

	//------------------------------------------------------------------------------------------------
	bool IsCut(int cell)
	{
		if (cell < 0 || cell >= CellCount())
			return false;
		return (m_aWords[cell / 32] & (1 << (cell % 32))) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Cuts one cell. True only when it was standing, so cutting twice counts once; false for a cut or
	//! out-of-range cell.
	bool Cut(int cell)
	{
		if (cell < 0 || cell >= CellCount() || IsCut(cell))
			return false;

		int word = cell / 32;
		m_aWords[word] = m_aWords[word] | (1 << (cell % 32));
		m_iCutCount++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The field grows back: every cell stands again.
	void Reset()
	{
		for (int i = 0; i < m_aWords.Count(); i++)
		{
			m_aWords[i] = 0;
		}
		m_iCutCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A copy of the bitset words: the late-join snapshot.
	void ExportWords(out array<int> words)
	{
		words = {};
		for (int i = 0; i < m_aWords.Count(); i++)
		{
			words.Insert(m_aWords[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the cut state with a snapshot. A short snapshot leaves the missing words standing, a long one
	//! is cut at the grid's size, and bits past the last cell are dropped so the count stays true.
	void ImportWords(array<int> words)
	{
		m_iCutCount = 0;
		for (int i = 0; i < m_aWords.Count(); i++)
		{
			int value = 0;
			if (words && i < words.Count())
				value = words[i];
			m_aWords[i] = value;
		}

		int total = CellCount();
		for (int cell = 0; cell < total; cell++)
		{
			if (IsCut(cell))
				m_iCutCount++;
		}
		int spare = m_aWords.Count() * 32 - total;
		if (spare > 0)
		{
			int last = m_aWords.Count() - 1;
			for (int bit = 32 - spare; bit < 32; bit++)
			{
				m_aWords[last] = m_aWords[last] & ~(1 << bit);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every cell a rectangle touches, in cell order. The rectangle is the reaper's bar: centred on
	//! (centreX, centreZ), `width` wide across the heading and `depth` long along it. The heading is a yaw in
	//! degrees, 0 facing +Z and 90 facing +X. A cell counts when the bar overlaps it by more than a sliver
	//! (1 cm), so a 3 m bar centred on a cell takes that cell and not its neighbours, and a bar astride two
	//! cells takes both.
	void Footprint(float centreX, float centreZ, float headingDegrees, float width, float depth, out array<int> cells)
	{
		cells = {};
		if (width <= 0 || depth <= 0)
			return;

		float yaw = headingDegrees * Math.DEG2RAD;
		float fx = Math.Sin(yaw);
		float fz = Math.Cos(yaw);
		float rx = fz;
		float rz = -fx;
		float halfWidth = width * 0.5;
		float halfDepth = depth * 0.5;
		float halfCell = m_fCell * 0.5;

		float reachX = halfWidth * Math.AbsFloat(rx) + halfDepth * Math.AbsFloat(fx);
		float reachZ = halfWidth * Math.AbsFloat(rz) + halfDepth * Math.AbsFloat(fz);
		int column0 = LF_Utils.MaxInt(0, FloorInt((centreX - reachX - m_fOriginX) / m_fCell));
		int column1 = LF_Utils.MinInt(m_iColumns - 1, FloorInt((centreX + reachX - m_fOriginX) / m_fCell));
		int row0 = LF_Utils.MaxInt(0, FloorInt((centreZ - reachZ - m_fOriginZ) / m_fCell));
		int row1 = LF_Utils.MinInt(m_iRows - 1, FloorInt((centreZ + reachZ - m_fOriginZ) / m_fCell));

		for (int row = row0; row <= row1; row++)
		{
			for (int column = column0; column <= column1; column++)
			{
				float dx = m_fOriginX + (column + 0.5) * m_fCell - centreX;
				float dz = m_fOriginZ + (row + 0.5) * m_fCell - centreZ;

				// Separating-axis overlap of the bar and the cell square: the shallowest of the four axes.
				float along = dx * fx + dz * fz;
				float across = dx * rx + dz * rz;
				float depthX = halfCell + reachX - Math.AbsFloat(dx);
				float depthZ = halfCell + reachZ - Math.AbsFloat(dz);
				float depthAcross = halfWidth + halfCell * (Math.AbsFloat(rx) + Math.AbsFloat(rz)) - Math.AbsFloat(across);
				float depthAlong = halfDepth + halfCell * (Math.AbsFloat(fx) + Math.AbsFloat(fz)) - Math.AbsFloat(along);
				float shallowest = Math.Min(Math.Min(depthX, depthZ), Math.Min(depthAcross, depthAlong));
				if (shallowest > LF_WheatFieldRules.FOOTPRINT_SLIVER)
					cells.Insert(row * m_iColumns + column);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the bar onto the grid: cuts every standing cell it touches and lists them in `newlyCut`, the
	//! batch the server broadcasts and the cells whose yield goes into the hopper. Already cut cells are
	//! skipped, so a second pass over the same ground returns an empty list.
	int CutFootprint(float centreX, float centreZ, float headingDegrees, float width, float depth, out array<int> newlyCut)
	{
		array<int> touched;
		Footprint(centreX, centreZ, headingDegrees, width, depth, touched);
		newlyCut = {};
		for (int i = 0; i < touched.Count(); i++)
		{
			if (Cut(touched[i]))
				newlyCut.Insert(touched[i]);
		}
		return newlyCut.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! The harvest rules a hitched reaper obeys: the speed gate, the yield of a cell and the hopper (spike
//! sections 4 and 6). Pure logic; the caller logs a refusal with `LF_Debug.Info("Harvest", ...)`.
class LF_WheatFieldRules
{
	//! Metres across one cell of a field's grid.
	static const float CELL_SIZE = 3;
	//! The reaper cuts between these speeds, in metres a second (about 2 to 22 km/h).
	static const float MIN_SPEED = 0.5;	// lf-fixed: the harvester spike's cutting speed band, moves to config with the reaper (#3289)
	static const float MAX_SPEED = 6;	// lf-fixed: as MIN_SPEED
	//! Kilograms of grain one cell gives: 260 cells make 2,600 kg.
	static const int CELL_YIELD_KG = 10;
	//! The hopper takes this many kilograms before it must be unloaded at the granary.
	static const int HOPPER_CAP_KG = 600;	// lf-fixed: the spike's default hopper; the reaper's own config row overrides it (#3289)
	//! A bar must overlap a cell by more than this many metres to cut it.
	static const float FOOTPRINT_SLIVER = 0.01;

	//------------------------------------------------------------------------------------------------
	//! OK from MIN_SPEED to MAX_SPEED inclusive; STOPPED below, TOO_FAST above.
	static LF_EHarvestRefusal CheckSpeed(float metresPerSecond)
	{
		if (metresPerSecond < MIN_SPEED)
			return LF_EHarvestRefusal.STOPPED;
		if (metresPerSecond > MAX_SPEED)
			return LF_EHarvestRefusal.TOO_FAST;
		return LF_EHarvestRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Grain for `cells` freshly cut cells.
	static int YieldKg(int cells)
	{
		if (cells <= 0)
			return 0;
		return cells * CELL_YIELD_KG;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts `wantedKg` into a hopper holding `hopperKg`. `takenKg` is what fit. OK when all of it fit;
	//! HOPPER_FULL when some or all of it did not (a full hopper takes 0).
	static LF_EHarvestRefusal FillHopper(int hopperKg, int wantedKg, out int takenKg)
	{
		takenKg = 0;
		if (wantedKg <= 0)
			return LF_EHarvestRefusal.OK;

		int room = LF_Utils.MaxInt(0, HOPPER_CAP_KG - hopperKg);
		takenKg = LF_Utils.MinInt(wantedKg, room);
		if (takenKg < wantedKg)
			return LF_EHarvestRefusal.HOPPER_FULL;
		return LF_EHarvestRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The hint the driver sees for a refusal. STOPPED has none: standing still is not a fault.
	static string RefusalKey(LF_EHarvestRefusal refusal)
	{
		if (refusal == LF_EHarvestRefusal.TOO_FAST)
			return "#LF-Harvest_TooFast";
		if (refusal == LF_EHarvestRefusal.HOPPER_FULL)
			return "#LF-Harvest_HopperFull";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The log word for a refusal.
	static string RefusalName(LF_EHarvestRefusal refusal)
	{
		if (refusal == LF_EHarvestRefusal.TOO_FAST)
			return "TOO_FAST";
		if (refusal == LF_EHarvestRefusal.STOPPED)
			return "STOPPED";
		if (refusal == LF_EHarvestRefusal.HOPPER_FULL)
			return "HOPPER_FULL";
		return "OK";
	}
}
