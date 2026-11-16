//------------------------------------------------------------------------------------------------
//! One page of the handbook (#1772): a stable id, a title key, body keys and a sort order. Text
//! fields take a #key or plain text; core pages use keys so the pack owns the words.
[BaseContainerProps()]
class LF_HandbookPage : LF_ConfigRecord
{
	[Attribute("", UIWidgets.Auto, "Stable id of the page, the key a pack overlay patches it by")]
	string m_sId;

	[Attribute("", UIWidgets.Auto, "The page title: a #key or plain text")]
	string m_sTitleKey;

	[Attribute("", UIWidgets.Auto, "The body paragraphs in reading order: a #key or plain text each")]
	ref array<string> m_aBodyKeys;

	[Attribute("-1", UIWidgets.Auto, "Sort order, lowest first. -1 sorts after every numbered page in file order", params: "-1 100000")]
	int m_iOrder;
}

//------------------------------------------------------------------------------------------------
//! The handbook's pages, Configs/Rules/LF_Handbook.conf. A pack overlay of this class adds,
//! replaces, patches or removes a page by id (LF_ConfigOverlay). Not an owner section yet.
[BaseContainerProps(configRoot: true)]
class LF_HandbookConfig
{
	static const ResourceName CONFIG_PATH = "{FA23671E9C86DC8E}Configs/Rules/LF_Handbook.conf";

	[Attribute("", UIWidgets.Object, "The pages. A pack overlay of this class patches them by id")]
	ref array<ref LF_HandbookPage> m_aPages;
}

//------------------------------------------------------------------------------------------------
//! The pages list as the shared override model sees it (LF_ConfigOverlay).
class LF_HandbookPageList : LF_ConfigList
{
	protected array<ref LF_HandbookPage> m_aRecords;

	void LF_HandbookPageList(notnull array<ref LF_HandbookPage> records)
	{
		m_aRecords = records;
	}

	override string GetName()
	{
		return "handbook.pages";
	}

	override int Count()
	{
		return m_aRecords.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_aRecords[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_HandbookPage.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a page id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_HandbookPage page = new LF_HandbookPage();
		page.m_sId = id;
		page.m_iOrder = -1;
		return page;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_HandbookPage page = LF_HandbookPage.Cast(record);
		if (index < m_aRecords.Count())
			m_aRecords.Set(index, page);
		else
			m_aRecords.Insert(page);
	}

	override void RemoveAt(int index)
	{
		m_aRecords.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_HandbookPage page = LF_HandbookPage.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sTitleKey"))
		{
			container.Get("m_sTitleKey", page.m_sTitleKey);
			changed++;
		}

		if (container.IsVariableSetDirectly("m_aBodyKeys"))
		{
			array<string> bodies = {};
			container.Get("m_aBodyKeys", bodies);
			page.m_aBodyKeys = new array<string>();
			page.m_aBodyKeys.Copy(bodies);
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iOrder"))
		{
			container.Get("m_iOrder", page.m_iOrder);
			changed++;
		}

		return changed;
	}
}
