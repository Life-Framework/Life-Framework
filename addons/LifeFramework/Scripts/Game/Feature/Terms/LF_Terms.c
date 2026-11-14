//------------------------------------------------------------------------------------------------
//! The core's terms file (#958): Configs/Terms/LF_Terms.conf. Core text names a term in a
//! placeholder ({police}); this file says which string keys carry its words.
[BaseContainerProps(configRoot: true)]
class LF_TermsConfig
{
	[Attribute("", UIWidgets.Object, "The terms. A pack overlay of this class patches them by id")]
	ref array<ref LF_TermRecord> m_aTerms;
}

//------------------------------------------------------------------------------------------------
//! The terms list as the shared override model sees it (LF_ConfigOverlay): a pack overlay adds,
//! replaces, patches or removes a term by id; the forms it writes patch the ones the core has.
class LF_TermList : LF_ConfigList
{
	protected array<ref LF_TermRecord> m_aRecords;

	void LF_TermList(notnull array<ref LF_TermRecord> records)
	{
		m_aRecords = records;
	}

	override string GetName()
	{
		return "terms.terms";
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
		return LF_TermRecord.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a term id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_TermRecord record = new LF_TermRecord();
		record.m_sId = id;
		return record;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_TermRecord term = LF_TermRecord.Cast(record);
		if (index < m_aRecords.Count())
			m_aRecords.Set(index, term);
		else
			m_aRecords.Insert(term);
	}

	override void RemoveAt(int index)
	{
		m_aRecords.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_TermRecord term = LF_TermRecord.Cast(record);
		BaseContainerList forms = container.GetObjectArray("m_aForms");
		if (!forms)
			return 0;

		int changed;
		for (int i = 0; i < forms.Count(); i++)
		{
			BaseContainer entry = forms.Get(i);
			if (!entry)
				continue;

			string form;
			string value;
			entry.Get("m_sForm", form);
			entry.Get("m_sValue", value);
			if (form.IsEmpty())
			{
				LF_Debug.Error(LF_TermBook.FEATURE, string.Format("%1: a form of term %2 has no name; skipped", GetName(), term.m_sId));
				continue;
			}

			term.SetForm(LF_TermBook.CanonicalForm(form), value);
			changed++;
		}

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_TermRecord term = LF_TermRecord.Cast(record);
		if (term.FormValue(LF_TermBook.FORM_SINGULAR).IsEmpty())
			return "a term needs a singular form";
		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The terms the running game has, and the one text path that turns a core string into words
//! (#958). Layers, lowest first: the core file, each pack overlay, the owner's `terms` section
//! (server) or the server's stream (client). `Text` is what a screen, a toast, a caption or a sign
//! calls instead of WidgetManager.Translate when its string names a term.
class LF_Terms
{
	static const ResourceName CONFIG_PATH = "{2B07C452DF3ED525}Configs/Terms/LF_Terms.conf";

	//! The owner config.json section this feature reads (LF_ConfigService).
	static const string SECTION = "terms";

	protected static ref LF_TermBook s_Book;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The layered book, built once. An empty book (logged) when the core file does not resolve.
	static LF_TermBook Get()
	{
		if (s_bLoadAttempted && s_Book)
			return s_Book;

		s_bLoadAttempted = true;
		s_Book = new LF_TermBook();
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_TermBook.FEATURE, string.Format("terms file does not resolve, text keeps its {placeholders}: %1", CONFIG_PATH));
			return s_Book;
		}

		LF_TermsConfig core = LF_TermsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!core)
		{
			LF_Debug.Error(LF_TermBook.FEATURE, string.Format("terms file has an unexpected root class: %1", CONFIG_PATH));
			return s_Book;
		}

		if (core.m_aTerms)
		{
			foreach (LF_TermRecord term : core.m_aTerms)
			{
				if (term)
					s_Book.GetRecords().Insert(term);
			}
		}

		ApplyLayers(s_Book);
		return s_Book;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays, the owner section and the server's stream over a book that holds
	//! the core terms.
	static void ApplyLayers(notnull LF_TermBook book)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_TermsConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(new LF_TermList(book.GetRecords()), resource.GetResource().ToBaseContainer(), "m_aTerms", names[i], claims);
			book.ClearCache();
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("terms: pack overlay %1 changed %2 terms", names[i], changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			book.ApplyOwner(reader, LF_ConfigService.GetOwnerSource());
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("terms: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		// Client: the server's owner text (WriteStream) is the owner layer here.
		LF_ConfigReader streamed = LF_ConfigStream.OpenReceivedSection(SECTION);
		if (streamed)
		{
			book.ApplyOwner(streamed, "the server's stream");
			LF_Debug.Info(LF_ConfigService.FEATURE, "terms: using the server's owner text");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached book so the next read layers everything again (/configreload, a new stream).
	static void Reload()
	{
		s_Book = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `book` the one Get returns until Reload.
	static void UseForTest(LF_TermBook book)
	{
		s_Book = book;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The language the game runs in, in the owner file's spelling ("en_us").
	static string Language()
	{
		string code;
		WidgetManager.GetLanguage(code);
		code = LF_TermBook.NormalizeLanguage(code);
		if (code.IsEmpty())
			return "en_us";
		return code;
	}

	//------------------------------------------------------------------------------------------------
	//! The words for one form of one term now, for script that builds text itself (a number
	//! formatter, a sign). Empty when the term has no words; the caller keeps its own fallback.
	static string Form(string id, string form = LF_TermBook.FORM_SINGULAR)
	{
		return Get().Resolve(id, form, Language());
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the `{term}` placeholders of already translated text.
	static string Expand(string text)
	{
		return Get().Expand(text, Language());
	}

	//------------------------------------------------------------------------------------------------
	//! The one text path for a core string that names a term: translates the key, expands its
	//! placeholders, then fills %1..%4. Terms expand before the parameters go in, so a player's name
	//! that reads "{police}" stays as typed.
	//! \param[in] key The string key, "#LF-Ticket_Title".
	static string Text(string key, string p1 = string.Empty, string p2 = string.Empty, string p3 = string.Empty, string p4 = string.Empty)
	{
		string raw = WidgetManager.Translate(key);
		string expanded = Expand(raw);
		if (p1.IsEmpty() && p2.IsEmpty() && p3.IsEmpty() && p4.IsEmpty())
			return expanded;

		return string.Format(expanded, p1, p2, p3, p4);
	}
}
