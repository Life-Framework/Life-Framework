//------------------------------------------------------------------------------------------------
//! One form of a term: "singular", "plural" or any other name a sentence needs ("sym" for a
//! currency symbol, "acc" for a German accusative). The value is a string key (`#LF-Term_Police`,
//! translated at read time) or literal text (one word for every language).
[BaseContainerProps()]
class LF_TermForm
{
	[Attribute("", UIWidgets.EditBox, "Form name: singular, plural, or another form a sentence names ({police:acc}). Lower case, letters and digits")]
	string m_sForm;

	[Attribute("", UIWidgets.EditBox, "A string key (#LF-Term_Police) so every language has its own word, or literal text for one word in every language")]
	string m_sValue;
}

//------------------------------------------------------------------------------------------------
//! One term (#958): a stable id and its forms. The id is what core text names in a placeholder
//! (`{police}`), so never rename one. A pack patches a term by id; the owner writes plain text.
[BaseContainerProps()]
class LF_TermRecord : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable term id, lower case (police, medic, town, currency). Core text writes {id}, packs and the owner patch by it")]
	string m_sId;

	[Attribute("", UIWidgets.Object, "The forms this term has. A form a sentence asks for and the term lacks falls back to the singular")]
	ref array<ref LF_TermForm> m_aForms;

	//------------------------------------------------------------------------------------------------
	//! \return The value of one form, or empty when the term lacks it.
	string FormValue(string form)
	{
		if (!m_aForms)
			return string.Empty;

		foreach (LF_TermForm entry : m_aForms)
		{
			if (entry && entry.m_sForm == form)
				return entry.m_sValue;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one form, adding it when the term lacks it.
	void SetForm(string form, string value)
	{
		if (!m_aForms)
			m_aForms = new array<ref LF_TermForm>();

		foreach (LF_TermForm entry : m_aForms)
		{
			if (entry && entry.m_sForm == form)
			{
				entry.m_sValue = value;
				return;
			}
		}

		LF_TermForm added = new LF_TermForm();
		added.m_sForm = form;
		added.m_sValue = value;
		m_aForms.Insert(added);
	}
}

//------------------------------------------------------------------------------------------------
//! The terms the running game has (#958): core defaults, a pack's patches over them, the owner's
//! text over both. Core text names a term in a placeholder and one call (Expand) turns it into
//! words; Terms are localised (a form's value is a string key) so translators translate "Sheriff"
//! like any other string. The book holds no engine state beyond WidgetManager.Translate, so a
//! test builds one in script and asks it.
//!
//! Placeholder: `{id}` is the singular, `{id:form}` another form (`{org:pl}`, `{currency:sym}`),
//! `{Id}` the same with a capital first letter. `pl` is shorthand for the form `plural`.
//! Anything else in braces, and any `{` that never closes, stays as written.
//------------------------------------------------------------------------------------------------
class LF_TermBook
{
	static const string FEATURE = "Terms";
	static const string FORM_SINGULAR = "singular";
	static const string FORM_PLURAL = "plural";
	protected static const string NAME_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
	protected static const string LOWER_CHARS = "abcdefghijklmnopqrstuvwxyz";
	protected static const string UPPER_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	protected static const string ALL_LANGUAGES = "";

	protected ref array<ref LF_TermRecord> m_aTerms = {};
	//! Owner text: term id -> "language|form" -> text. The language is empty for every language.
	protected ref map<string, ref map<string, string>> m_mOwner = new map<string, ref map<string, string>>();
	protected ref map<string, bool> m_mWarned = new map<string, bool>();
	protected ref map<string, string> m_mCache = new map<string, string>();
	protected int m_iWarnCount;

	//------------------------------------------------------------------------------------------------
	//! \return The form name a placeholder spelling means: "" is the singular, "pl" the plural.
	static string CanonicalForm(string form)
	{
		if (form.IsEmpty())
			return FORM_SINGULAR;
		if (form == "pl")
			return FORM_PLURAL;
		return form;
	}

	//------------------------------------------------------------------------------------------------
	//! A language code the way the owner file spells it: lower case, underscore ("de_de").
	static string NormalizeLanguage(string code)
	{
		string result = code;
		result.ToLower();
		result.Replace("-", "_");
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an owner key such as "de_de" or "pt_br" is a language code, not a form name.
	static bool IsLanguageCode(string key)
	{
		if (key.Length() != 5 || key.Get(2) != "_")
			return false;

		string letters = key.Substring(0, 2) + key.Substring(3, 2);
		for (int i = 0; i < letters.Length(); i++)
		{
			if (!LOWER_CHARS.Contains(letters.Get(i)))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the owner's language key covers the language the game runs in. A bare
	//! language ("de") covers every region of it.
	static bool LanguageMatches(string ownerLanguage, string current)
	{
		if (ownerLanguage == current)
			return true;
		return current.Length() == 2 && ownerLanguage.StartsWith(current + "_");
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a term (core or pack). An id the book has gets the missing forms added.
	LF_TermRecord Add(string id, string singular, string plural = string.Empty)
	{
		LF_TermRecord term = Find(id);
		if (!term)
		{
			term = new LF_TermRecord();
			term.m_sId = id;
			m_aTerms.Insert(term);
		}

		term.SetForm(FORM_SINGULAR, singular);
		if (!plural.IsEmpty())
			term.SetForm(FORM_PLURAL, plural);
		m_mCache.Clear();
		return term;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one form of a term (core or pack layer).
	void SetForm(string id, string form, string value)
	{
		LF_TermRecord term = Find(id);
		if (!term)
		{
			term = new LF_TermRecord();
			term.m_sId = id;
			m_aTerms.Insert(term);
		}

		term.SetForm(CanonicalForm(form), value);
		m_mCache.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Owner text for one form. An empty language applies to every language.
	//! \return False when the term does not exist (the owner named a term no layer defines).
	bool SetOwner(string id, string form, string language, string text)
	{
		if (!Find(id))
			return false;

		map<string, string> forms;
		if (!m_mOwner.Find(id, forms))
		{
			forms = new map<string, string>();
			m_mOwner.Set(id, forms);
		}

		forms.Set(NormalizeLanguage(language) + "|" + CanonicalForm(form), text);
		m_mCache.Clear();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	LF_TermRecord Find(string id)
	{
		foreach (LF_TermRecord term : m_aTerms)
		{
			if (term && term.m_sId == id)
				return term;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aTerms.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The term list itself, which the override model (LF_TermList) patches in place.
	array<ref LF_TermRecord> GetRecords()
	{
		return m_aTerms;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets resolved words; call after the list changed behind the book's back.
	void ClearCache()
	{
		m_mCache.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many distinct problems (an unknown term, a form that fell back) were logged.
	int GetWarnCount()
	{
		return m_iWarnCount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the owner (or the server's stream) set any text.
	bool HasOwnerText()
	{
		return m_mOwner.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The words for one form of one term in one language. Owner text for the language, then owner
	//! text for every language, then the pack or core value (a string key is translated now). A form
	//! the term lacks falls back to its singular and logs once.
	//! \param[in] language The normalised language the game runs in ("de_de").
	//! \return The words, or empty when the term does not exist (logged once).
	string Resolve(string id, string form, string language)
	{
		string cacheKey = id + "|" + form + "|" + language;
		string cached;
		if (m_mCache.Find(cacheKey, cached))
			return cached;

		LF_TermRecord term = Find(id);
		if (!term)
		{
			WarnOnce("unknown:" + id, string.Format("term %1 does not exist: no pack, owner or core file defines it", id));
			return string.Empty;
		}

		string canonical = CanonicalForm(form);
		string text = Lookup(term, canonical, language);
		if (text.IsEmpty() && canonical != FORM_SINGULAR)
		{
			WarnOnce("form:" + id + "|" + canonical, string.Format("term %1 has no form %2; using its singular", id, canonical));
			text = Lookup(term, FORM_SINGULAR, language);
		}

		if (text.IsEmpty())
		{
			WarnOnce("empty:" + id, string.Format("term %1 has no words: its singular is missing or its string key does not translate", id));
			return string.Empty;
		}

		m_mCache.Set(cacheKey, text);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected string Lookup(LF_TermRecord term, string form, string language)
	{
		map<string, string> owner;
		if (m_mOwner.Find(term.m_sId, owner))
		{
			string text;
			// A language key beats the every-language key; a bare "de" covers "de_de".
			foreach (string key, string value : owner)
			{
				int bar = key.IndexOf("|");
				if (bar <= 0 || key.Substring(bar + 1, key.Length() - bar - 1) != form)
					continue;
				if (LanguageMatches(key.Substring(0, bar), language))
					return value;
			}

			if (owner.Find(ALL_LANGUAGES + "|" + form, text))
				return text;
		}

		string value = term.FormValue(form);
		if (value.StartsWith("#"))
		{
			string translated = WidgetManager.Translate(value);
			if (translated == value)
				return string.Empty;
			return translated;
		}

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces every `{term}` placeholder in the text with the term's words in `language`.
	string Expand(string text, string language)
	{
		if (!text.Contains("{"))
			return text;

		string result;
		int length = text.Length();
		int i;
		while (i < length)
		{
			string ch = text.Get(i);
			if (ch != "{")
			{
				result += ch;
				i++;
				continue;
			}

			int close = text.IndexOfFrom(i, "}");
			if (close < 0)
			{
				result += text.Substring(i, length - i);
				break;
			}

			string inner = text.Substring(i + 1, close - i - 1);
			string replacement;
			if (Placeholder(inner, language, replacement))
			{
				result += replacement;
				i = close + 1;
				continue;
			}

			result += ch;
			i++;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `inner` ({inner}) is a placeholder this book knows; `replacement` holds its words.
	protected bool Placeholder(string inner, string language, out string replacement)
	{
		string name = inner;
		string form;
		int colon = inner.IndexOf(":");
		if (colon >= 0)
		{
			name = inner.Substring(0, colon);
			form = inner.Substring(colon + 1, inner.Length() - colon - 1);
		}

		if (!IsName(name) || (colon >= 0 && !IsName(form)))
			return false;

		string id = name;
		bool capital = UPPER_CHARS.Contains(name.Get(0));
		if (capital)
		{
			string head = name.Substring(0, 1);
			head.ToLower();
			id = head + name.Substring(1, name.Length() - 1);
		}

		string words = Resolve(id, form, language);
		if (words.IsEmpty())
			return false;

		if (capital)
			words = Capitalise(words);
		replacement = words;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsName(string name)
	{
		if (name.IsEmpty())
			return false;

		for (int i = 0; i < name.Length(); i++)
		{
			if (!NAME_CHARS.Contains(name.Get(i)))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Upper-cases a leading a-z. Any other first character stays: a translator writes it as needed.
	static string Capitalise(string text)
	{
		if (text.IsEmpty())
			return text;

		string first = text.Substring(0, 1);
		if (!LOWER_CHARS.Contains(first))
			return text;

		first.ToUpper();
		return first + text.Substring(1, text.Length() - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void WarnOnce(string key, string message)
	{
		if (m_mWarned.Contains(key))
			return;

		m_mWarned.Set(key, true);
		m_iWarnCount++;
		LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "terms" section (or the server's stream, which has the same shape). Each
	//! key is a term id. Its value is plain text (the singular and the plural, in every language), an
	//! object of forms (`{"singular": "Gang", "plural": "Gangs"}`), or an object keyed by language
	//! (`{"de_de": "Bande"}` or `{"de_de": {"singular": "Bande", "plural": "Banden"}}`). A language
	//! key and form keys can sit in one object.
	//! \param[in] source Names the file in log lines.
	//! \return Forms set.
	int ApplyOwner(notnull LF_ConfigReader reader, string source = "config.json")
	{
		array<string> ids = {};
		reader.GetKeys(ids);
		int applied;
		foreach (string id : ids)
		{
			if (!Find(id))
			{
				LF_Debug.Error(FEATURE, string.Format("%1: terms.%2 is not a term; known terms: %3", source, id, ListIds()));
				continue;
			}

			LF_EConfigJsonKind kind = reader.KindOfKey(id);
			if (kind == LF_EConfigJsonKind.STRING)
			{
				string text;
				if (reader.ReadString(id, text))
					applied += SetBoth(id, ALL_LANGUAGES, text);
			}
			else if (kind == LF_EConfigJsonKind.OBJECT)
			{
				if (reader.BeginRecord(id))
				{
					applied += ReadTermObject(reader, id, ALL_LANGUAGES, source);
					reader.EndRecord();
				}
			}
			else
			{
				LF_Debug.Error(FEATURE, string.Format("%1: terms.%2 must be text or an object of forms or languages; the shipped words stay", source, id));
			}
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the keys of one open object: form names set that form, language codes recurse.
	protected int ReadTermObject(LF_ConfigReader reader, string id, string language, string source)
	{
		array<string> keys = {};
		reader.GetKeys(keys);
		int applied;
		foreach (string key : keys)
		{
			LF_EConfigJsonKind kind = reader.KindOfKey(key);
			if (IsLanguageCode(key))
			{
				if (!language.IsEmpty())
				{
					LF_Debug.Error(FEATURE, string.Format("%1: terms.%2.%3: a language sits inside another language; skipped", source, id, key));
					continue;
				}

				if (kind == LF_EConfigJsonKind.STRING)
				{
					string text;
					if (reader.ReadString(key, text))
						applied += SetBoth(id, key, text);
				}
				else if (kind == LF_EConfigJsonKind.OBJECT && reader.BeginRecord(key))
				{
					applied += ReadTermObject(reader, id, key, source);
					reader.EndRecord();
				}
				continue;
			}

			string form;
			if (kind != LF_EConfigJsonKind.STRING || !reader.ReadString(key, form))
			{
				LF_Debug.Error(FEATURE, string.Format("%1: terms.%2.%3 must be text (a form) or a language code such as de_de; skipped", source, id, key));
				continue;
			}

			if (SetOwner(id, key, language, form))
				applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Plain text names the term: it is the singular and the plural.
	protected int SetBoth(string id, string language, string text)
	{
		int applied;
		if (SetOwner(id, FORM_SINGULAR, language, text))
			applied++;
		if (SetOwner(id, FORM_PLURAL, language, text))
			applied++;
		return applied;
	}

	//------------------------------------------------------------------------------------------------
	protected string ListIds()
	{
		string ids;
		foreach (int i, LF_TermRecord term : m_aTerms)
		{
			if (i > 0)
				ids += ", ";
			ids += term.m_sId;
		}

		return ids;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the owner text for the client stream, in the shape ApplyOwner reads back.
	void WriteStream(notnull SCR_JsonSaveContext ctx)
	{
		foreach (string id, map<string, string> forms : m_mOwner)
		{
			ctx.StartObject(id);
			array<string> languages = {};
			foreach (string key, string text : forms)
			{
				int bar = key.IndexOf("|");
				string language = key.Substring(0, bar);
				if (language.IsEmpty())
					ctx.WriteValue(key.Substring(bar + 1, key.Length() - bar - 1), text);
				else if (languages.Find(language) < 0)
					languages.Insert(language);
			}

			foreach (string language : languages)
			{
				ctx.StartObject(language);
				foreach (string key, string text : forms)
				{
					int bar = key.IndexOf("|");
					if (key.Substring(0, bar) == language)
						ctx.WriteValue(key.Substring(bar + 1, key.Length() - bar - 1), text);
				}
				ctx.EndObject();
			}

			ctx.EndObject();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every term the book holds, for tooling and tests.
	void GetTerms(notnull array<LF_TermRecord> terms)
	{
		terms.Clear();
		foreach (LF_TermRecord term : m_aTerms)
			terms.Insert(term);
	}
}
