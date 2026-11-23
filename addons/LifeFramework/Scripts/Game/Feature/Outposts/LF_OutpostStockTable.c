//------------------------------------------------------------------------------------------------
//! One outpost's stock table (0.3.0 gangs O6, decision N29): the quartermaster's rows for the outpost
//! with this id. The pack lists one table per outpost (`north` now, `south` when that place lands), so
//! each crate sells its own stock and the core ships none (the core's `LF_OutpostStock.DefaultStock`
//! stays the fallback for a crate whose outpost has no table).
[BaseContainerProps()]
class LF_OutpostStockTable
{
	[Attribute("", UIWidgets.EditBox, "Id of the outpost (LF_OutpostComponent), for example north")]
	string m_sOutpostId;

	[Attribute("", UIWidgets.Object, "The crate's rows: prefab, price and lowest underworld tier. A prefab slot may stand in for the prefab")]
	ref array<ref LF_OutpostStockItem> m_aRows;
}

//------------------------------------------------------------------------------------------------
//! The stock tables: a pack overlay file of this root class lists one table per outpost id. A later
//! overlay's table for the same id replaces an earlier one.
[BaseContainerProps(configRoot: true)]
class LF_OutpostStockTableConfig
{
	[Attribute("", UIWidgets.Object, "One table per outpost id")]
	ref array<ref LF_OutpostStockTable> m_aTables;
}

//------------------------------------------------------------------------------------------------
//! Reads the pack's outpost stock tables (O6). Server and client read the same overlays; nothing here
//! is saved or replicated.
class LF_OutpostStockTables
{
	static const string FEATURE = "Outposts";

	protected static ref array<ref LF_OutpostStockTable> s_aTables;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aTables = null;
		s_bLoaded = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the loaded tables so the next read loads the overlays again.
	static void Reload()
	{
		ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `tables` the tables until Reload.
	static void UseTablesForTest(array<ref LF_OutpostStockTable> tables)
	{
		s_aTables = tables;
		s_bLoaded = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The rows for outpost `outpostId`, or null when the pack gives that outpost no table.
	static array<ref LF_OutpostStockItem> RowsFor(string outpostId)
	{
		if (outpostId.IsEmpty())
			return null;

		foreach (LF_OutpostStockTable table : Tables())
		{
			if (table.m_sOutpostId == outpostId && table.m_aRows && !table.m_aRows.IsEmpty())
				return table.m_aRows;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id of the outpost whose safe circle holds `position` (the zone `outpost:<id>`), or empty.
	static string OutpostIdAt(vector position)
	{
		array<ref LF_ZoneArea> zones = LF_Zones.GetAll();
		foreach (LF_ZoneArea zone : zones)
		{
			if (zone.m_sName.StartsWith(LF_Outpost.ZONE_PREFIX) && zone.Contains(position))
				return zone.m_sName.Substring(LF_Outpost.ZONE_PREFIX.Length(), zone.m_sName.Length() - LF_Outpost.ZONE_PREFIX.Length()); // lf-bounded: a zone name
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_OutpostStockTable> Tables()
	{
		if (s_bLoaded && s_aTables)
			return s_aTables;

		s_bLoaded = true;
		s_aTables = new array<ref LF_OutpostStockTable>();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_OutpostStockTableConfig, FEATURE, resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_OutpostStockTableConfig overlay = LF_OutpostStockTableConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!overlay || !overlay.m_aTables)
				continue;

			foreach (LF_OutpostStockTable table : overlay.m_aTables)
			{
				if (table && !table.m_sOutpostId.IsEmpty() && table.m_aRows)
					Put(s_aTables, table);
			}

			LF_Debug.Info(FEATURE, string.Format("outpost stock: overlay %1 gave %2 tables", names[i], overlay.m_aTables.Count()));
		}

		if (s_aTables.IsEmpty())
			LF_Debug.Info(FEATURE, "outpost stock: no pack tables, crates sell the default stock");

		return s_aTables;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds `table`, or replaces the earlier table for the same outpost.
	protected static void Put(notnull array<ref LF_OutpostStockTable> tables, notnull LF_OutpostStockTable table)
	{
		foreach (int i, LF_OutpostStockTable existing : tables)
		{
			if (existing.m_sOutpostId == table.m_sOutpostId)
			{
				tables[i] = table;
				return;
			}
		}

		tables.Insert(table);
	}
}

//------------------------------------------------------------------------------------------------
//! The crate reads its outpost's pack table first. A prefab that lists its own rows keeps them; a crate
//! outside any outpost, or whose outpost has no table, sells the default stock.
modded class LF_OutpostStockComponent
{
	protected ref array<ref LF_OutpostStockItem> m_aTableRows;
	protected bool m_bTableRead;

	//------------------------------------------------------------------------------------------------
	override array<ref LF_OutpostStockItem> GetStock()
	{
		if (m_aStock && !m_aStock.IsEmpty())
			return m_aStock;

		if (!m_bTableRead)
		{
			IEntity owner = GetOwner();
			if (owner)
			{
				m_bTableRead = true;
				m_aTableRows = LF_OutpostStockTables.RowsFor(LF_OutpostStockTables.OutpostIdAt(owner.GetOrigin()));
			}
		}

		if (m_aTableRows && !m_aTableRows.IsEmpty())
			return m_aTableRows;

		return super.GetStock();
	}
}

//------------------------------------------------------------------------------------------------
class LF_OutpostStockTables_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OutpostStockTables";
	}

	override void Apply()
	{
		LF_OutpostStockTables.ResetStatics();
	}
}
