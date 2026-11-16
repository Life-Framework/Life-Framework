//------------------------------------------------------------------------------------------------
//! The handbook's page list (#1772, refs #642): pages are read from Configs/Rules/LF_Handbook.conf,
//! then each pack overlay adds, replaces, patches or removes a page by id. Text is keys the pack's
//! string table owns; the core ships two neutral pages. A page with a blank id, title or body
//! paragraph is logged (`[LFDebug:Rules]`) and left out, the rest still load. No sheet reads this
//! yet; the loader is the data half.
class LF_Handbook
{
	static const string FEATURE = "Rules";

	protected static ref LF_HandbookConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! \return "" when the page can be shown, else why not.
	static string RefusePage(LF_HandbookPage page)
	{
		if (!page)
			return "empty entry";

		if (page.m_sId.IsEmpty())
			return "the page has no id";

		if (page.m_sTitleKey.IsEmpty())
			return string.Format("page '%1' has no title key", page.m_sId);

		if (!page.m_aBodyKeys || page.m_aBodyKeys.IsEmpty())
			return string.Format("page '%1' has no body key", page.m_sId);

		foreach (string body : page.m_aBodyKeys)
		{
			if (body.IsEmpty())
				return string.Format("page '%1' has a blank body key", page.m_sId);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The core file with every pack overlay over it, or null (logged once) when it does not resolve.
	static LF_HandbookConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(LF_HandbookConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("handbook config does not resolve, the handbook is empty: %1", LF_HandbookConfig.CONFIG_PATH));
			return null;
		}

		s_Config = LF_HandbookConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("handbook config has an unexpected root class, the handbook is empty: %1", LF_HandbookConfig.CONFIG_PATH));
			return null;
		}

		if (!s_Config.m_aPages)
			s_Config.m_aPages = new array<ref LF_HandbookPage>();

		ApplyPackOverlays(s_Config);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers each pack overlay over `config`, in load order.
	static void ApplyPackOverlays(notnull LF_HandbookConfig config)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_HandbookConfig.CONFIG_PATH, LF_HandbookConfig, "handbook", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			ApplyOverlay(config, resource.GetResource().ToBaseContainer(), names[i], claims);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One overlay container over `config`. \return Pages changed.
	static int ApplyOverlay(notnull LF_HandbookConfig config, notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!config.m_aPages)
			config.m_aPages = new array<ref LF_HandbookPage>();

		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_HandbookPageList(config.m_aPages), root, "m_aPages", source, claims);
		LF_Debug.Info(FEATURE, string.Format("handbook: pack overlay %1 changed %2 page(s)", source, changed));
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! The pages a player can read, lowest order first (ties and unnumbered pages keep file order).
	//! A page that cannot stand is logged once and left out. \return How many pages were skipped.
	static int GetPages(notnull array<LF_HandbookPage> outPages)
	{
		outPages.Clear();
		LF_HandbookConfig config = Get();
		if (!config)
			return 0;

		return Collect(config, outPages);
	}

	//------------------------------------------------------------------------------------------------
	//! GetPages over a given config (tests).
	static int Collect(notnull LF_HandbookConfig config, notnull array<LF_HandbookPage> outPages)
	{
		outPages.Clear();
		int skipped;
		if (!config.m_aPages)
			return 0;

		foreach (LF_HandbookPage page : config.m_aPages)
		{
			string refusal = RefusePage(page);
			if (!refusal.IsEmpty())
			{
				LF_Debug.Error(FEATURE, string.Format("handbook page skipped: %1", refusal));
				skipped++;
				continue;
			}

			// Stable insertion: after every page that sorts at or before this one.
			int at = outPages.Count();
			while (at > 0 && SortKey(outPages[at - 1]) > SortKey(page))
			{
				at--;
			}

			outPages.InsertAt(page, at);
		}

		return skipped;
	}

	//------------------------------------------------------------------------------------------------
	protected static int SortKey(LF_HandbookPage page)
	{
		if (page.m_iOrder < 0)
			return int.MAX;

		return page.m_iOrder;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The page with this id among those a player can read, or null.
	static LF_HandbookPage Find(string id)
	{
		array<LF_HandbookPage> pages = {};
		GetPages(pages);
		foreach (LF_HandbookPage page : pages)
		{
			if (page.m_sId == id)
				return page;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached config so the next read layers the files again (/configreload, world end).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_HandbookConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Handbook's statics (registered at script load; Core/LF_StaticReset.c).
class LF_Handbook_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Handbook_StaticResetHook());

	override string Owner()
	{
		return "LF_Handbook";
	}

	override void Apply()
	{
		LF_Handbook.ResetStatics();
	}
}
