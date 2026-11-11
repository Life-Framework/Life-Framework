//------------------------------------------------------------------------------------------------
//! Server cost budget at the benchmark's player count (#335). Loaded from
//! Configs/Perf/LF_PerfBudget.conf; LF_Test_PerfBudget fails when a measured cost passes a limit
//! here. Times are wall-clock milliseconds for the whole synthetic population, not per player,
//! so one limit reads "this pass costs at most N ms of one server frame budget". The numbers
//! came from `tools\cli perf` (docs/architecture/performance.md); tighten them when a system
//! gets faster, raise one only with a line in that doc saying why.
//!
//! Core, not pack: the budget says what the engine side must hold, not what a server is.
[BaseContainerProps(configRoot: true)]
class LF_PerfBudget
{
	static const ResourceName CONFIG_PATH = "{3E83A67A95065694}Configs/Perf/LF_PerfBudget.conf";

	[Attribute("128", UIWidgets.Auto, "Synthetic characters the benchmark builds")]
	int m_iPlayers;

	[Attribute("40", UIWidgets.Auto, "Money events published per synthetic character")]
	int m_iMoneyEventsPerPlayer;

	[Attribute("3", UIWidgets.Auto, "Crime events published per synthetic character")]
	int m_iCrimeEventsPerPlayer;

	[Attribute("100", UIWidgets.Auto, "Most ms to export every account (one save pass)")]
	int m_iMaxAccountExportMs;

	[Attribute("1000", UIWidgets.Auto, "Most ms for the whole store write (SerializeStore, JSON)")]
	int m_iMaxSaveWriteMs;

	[Attribute("1000", UIWidgets.Auto, "Most ms for the whole store read (DeserializeStore, JSON)")]
	int m_iMaxSaveReadMs;

	[Attribute("1000", UIWidgets.Auto, "Most bytes of save per character with the shared logs empty (JSON text; binary is smaller)")]
	int m_iMaxSaveBytesPerPlayer;

	[Attribute("6000000", UIWidgets.Auto, "Most bytes of the whole save with the ledger and incident log at their caps (JSON text)")]
	int m_iMaxSaveBytesAtCap;

	[Attribute("200", UIWidgets.Auto, "Most microseconds to publish one money event through every subscriber, log below its cap")]
	int m_iMaxMoneyEventUs;

	[Attribute("500", UIWidgets.Auto, "Most microseconds to publish one money event with the ledger at its retention cap")]
	int m_iMaxMoneyEventAtCapUs;

	[Attribute("100", UIWidgets.Auto, "Most microseconds to publish one crime event")]
	int m_iMaxCrimeEventUs;

	[Attribute("200", UIWidgets.Auto, "Most ms for one query per character against a full ledger (the incident view)")]
	int m_iMaxLedgerQueryAllMs;

	protected static ref LF_PerfBudget s_Budget;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded budget, or a default-valued one (logged once) when the file does not resolve.
	static LF_PerfBudget Get()
	{
		if (s_bLoadAttempted)
			return s_Budget;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Budget = LF_PerfBudget.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Budget)
		{
			LF_Debug.Error(LF_Perf.FEATURE, string.Format("budget config does not resolve, using the defaults: %1", CONFIG_PATH));
			s_Budget = new LF_PerfBudget();
		}

		return s_Budget;
	}
}
