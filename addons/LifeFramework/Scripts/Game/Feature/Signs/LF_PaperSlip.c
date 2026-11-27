// LF_PaperSlip.c: the printed paper a trade hands over (WO-W2.6, #4465). Three kinds, listed in
// Configs/Paper/LF_PaperKinds.conf: a weigh slip (a sale at a buyer), a payslip (a paid shift) and a
// delivery note (a parcel hand-in). Each is the receipt paper item: it prints the title, one line
// (item, amount, price paid) and the in-game date, and Read shows it through the existing receipt Read.
// The printed text is session state, like the till receipt's: a slip kept over a restart reads blank.

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomStringTitleField("m_sId")]
class LF_PaperKindDef
{
	[Attribute("", desc: "Kind id: weigh, payslip or delivery")]
	string m_sId;

	[Attribute("", UIWidgets.ResourceNamePicker, "The paper prefab (the receipt asset)", "et")]
	ResourceName m_sPrefab;

	[Attribute("", desc: "Title line (localization key)")]
	string m_sTitle;

	[Attribute("", desc: "Detail line (localization key): %1 item, %2 amount, %3 price paid")]
	string m_sLine;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_PaperKindsConfig
{
	[Attribute("", UIWidgets.Object, "Paper kinds")]
	ref array<ref LF_PaperKindDef> m_aKinds;

	static const ResourceName CONFIG_PATH = "{9807AA6AAF28F67B}Configs/Paper/LF_PaperKinds.conf";

	protected static ref LF_PaperKindsConfig s_Config;

	//------------------------------------------------------------------------------------------------
	//! The loaded kinds, or an empty config when the file is missing (logged once).
	static LF_PaperKindsConfig Get()
	{
		if (s_Config)
			return s_Config;

		Resource resource = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (resource && resource.IsValid())
			s_Config = LF_PaperKindsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error(LF_PaperSlip.FEATURE, "paper kinds config failed to load: " + CONFIG_PATH);
			s_Config = new LF_PaperKindsConfig();
		}

		if (!s_Config.m_aKinds)
			s_Config.m_aKinds = new array<ref LF_PaperKindDef>();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PaperKindsConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PaperKindsConfig";
	}

	override void Apply()
	{
		LF_PaperKindsConfig.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The text a printed slip carries, kept on the receipt paper item. The receipt's own Read shows
//! Describe(), so a slip reads through the existing paper Read and opens no screen.
modded class LF_ReceiptComponent
{
	protected string m_sSlipText;

	//------------------------------------------------------------------------------------------------
	void PrintSlip(string text)
	{
		m_sSlipText = text;
	}

	//------------------------------------------------------------------------------------------------
	string GetSlipText()
	{
		return m_sSlipText;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsPrinted()
	{
		if (!m_sSlipText.IsEmpty())
			return true;

		return super.IsPrinted();
	}

	//------------------------------------------------------------------------------------------------
	override string Describe()
	{
		if (!m_sSlipText.IsEmpty())
			return m_sSlipText;

		return super.Describe();
	}
}

//------------------------------------------------------------------------------------------------
class LF_PaperSlip
{
	static const string FEATURE = "PaperSlip";

	static const string KIND_WEIGH = "weigh";
	static const string KIND_PAYSLIP = "payslip";
	static const string KIND_DELIVERY = "delivery";

	//------------------------------------------------------------------------------------------------
	//! \return The kind row for an id, or null when the config has none (logged).
	static LF_PaperKindDef Kind(string kindId)
	{
		foreach (LF_PaperKindDef def : LF_PaperKindsConfig.Get().m_aKinds)
		{
			if (def.m_sId == kindId)
				return def;
		}

		LF_Debug.Warn(FEATURE, "no paper kind " + kindId);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The paper prefab of a kind, or empty when the kind is unknown.
	static ResourceName PrefabOf(string kindId)
	{
		LF_PaperKindDef def = Kind(kindId);
		if (!def)
			return "";

		return def.m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! The lines a slip prints: the title, the item with its amount and price, and the date.
	//! \param item A plain name or a #key
	//! \param date The in-game date and time as printed on the paper
	static void Lines(string kindId, string item, int amount, int price, string date, notnull array<string> outLines)
	{
		outLines.Clear();
		LF_PaperKindDef def = Kind(kindId);
		if (!def)
			return;

		string itemText = item;
		if (item.StartsWith("#"))
			itemText = WidgetManager.Translate(item);

		outLines.Insert(WidgetManager.Translate(def.m_sTitle));
		outLines.Insert(LF_Terms.Text(def.m_sLine, itemText, amount.ToString(), price.ToString()));
		outLines.Insert(WidgetManager.Translate("#LF-Paper_DateLine", date));
	}

	//------------------------------------------------------------------------------------------------
	//! The slip text for a Read: the lines joined by newlines.
	static string Text(string kindId, string item, int amount, int price, string date)
	{
		array<string> lines = {};
		Lines(kindId, item, amount, price, date, lines);
		string text;
		foreach (int i, string line : lines)
		{
			if (i > 0)
				text += "\n";

			text += line;
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: prints a slip dated now and puts it in the character's inventory. With no room it lies
	//! at their feet. A bad kind, a missing prefab or a null character logs and degrades to no slip.
	//! \return The slip, or null when it could not be made.
	static IEntity Give(IEntity character, string kindId, string item, int amount, int price)
	{
		if (!Replication.IsServer() || !character)
		{
			LF_Debug.Error(FEATURE, "slip not given: not the server, or no character");
			return null;
		}

		ResourceName prefab = PrefabOf(kindId);
		if (prefab.IsEmpty())
			return null;

		IEntity slip = LF_Utils.SpawnEntityPrefab(prefab, character.GetOrigin());
		if (!slip)
		{
			LF_Debug.Error(FEATURE, "slip did not spawn: " + prefab);
			return null;
		}

		LF_ReceiptComponent paper = LF_Component<LF_ReceiptComponent>.Find(slip);
		if (!paper)
		{
			LF_Debug.Error(FEATURE, prefab + " has no LF_ReceiptComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(slip);
			return null;
		}

		paper.PrintSlip(Text(kindId, item, amount, price, LF_HospitalBill.InGameDate()));

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!storage || !storage.TryInsertItem(slip))
			LF_Debug.Warn(FEATURE, string.Format("%1 slip: no room in the pockets; it lies at the feet", kindId));
		else
			LF_Debug.Info(FEATURE, string.Format("%1 slip given: %2 x%3, %4", kindId, item, amount, price));

		return slip;
	}
}
