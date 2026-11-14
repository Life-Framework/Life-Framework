//------------------------------------------------------------------------------------------------
//! A pack's own owner-config section (#553). The pack lists its sections in its manifest `.conf`
//! (LF_PackManifest.m_aConfigSections), each with its fields, as data: a pack has no script. The
//! core reads the declaration generically; it never reads a pack's values itself.
//!
//! A section name is namespaced by the pack: the pack id without `LF_`, lower case
//! (`LF_Example` is `example`), alone or followed by an upper-case word
//! (`exampleChains`). That keeps a pack off the core's section names and off another pack's.
//! The owner's `config.json` sets it like a core section:
//!   "example": { "festivalDay": 3 }
//! Field types are int, float, bool and string. Each field has a default; min and max (empty =
//! none) bound an int or float. The same fields feed config.schema.json (tools/config/schema.mjs).
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_PackConfigField
{
	[Attribute("", desc: "JSON key, lower camel case, e.g. festivalDay.")]
	string m_sKey;

	[Attribute("int", desc: "int, float, bool or string.")]
	string m_sType;

	[Attribute("", desc: "Default as text: 3, 0.5, true, hello.")]
	string m_sDefault;

	[Attribute("", desc: "Lowest accepted value for an int or float; empty for none.")]
	string m_sMin;

	[Attribute("", desc: "Highest accepted value for an int or float; empty for none.")]
	string m_sMax;

	[Attribute("", desc: "What the field does, for the owner's reference page.")]
	string m_sDesc;
};

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_PackConfigSection
{
	[Attribute("", desc: "Owner JSON key: the pack namespace, alone or followed by an upper-case word.")]
	string m_sName;

	[Attribute("", desc: "What the section is for, for the owner's reference page.")]
	string m_sDesc;

	[Attribute("", UIWidgets.Object, desc: "The section's fields.")]
	ref array<ref LF_PackConfigField> m_aFields;
};

//------------------------------------------------------------------------------------------------
//! The values of one pack section after the owner layer: each field the owner set and the config
//! accepted, else its declared default. A field of the wrong type or outside its range keeps the
//! default and counts as rejected; a key the section does not declare is warned and kept in the file.
class LF_PackConfigValues
{
	string m_sSection;
	int m_iRejected;
	int m_iUnknown;
	protected ref map<string, int> m_mInts = new map<string, int>();
	protected ref map<string, float> m_mFloats = new map<string, float>();
	protected ref map<string, bool> m_mBools = new map<string, bool>();
	protected ref map<string, string> m_mStrings = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	void SetInt(string key, int value) { m_mInts.Set(key, value); }
	void SetFloat(string key, float value) { m_mFloats.Set(key, value); }
	void SetBool(string key, bool value) { m_mBools.Set(key, value); }
	void SetString(string key, string value) { m_mStrings.Set(key, value); }

	//------------------------------------------------------------------------------------------------
	int GetInt(string key, int fallback = 0)
	{
		int value;
		if (m_mInts.Find(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	float GetFloat(string key, float fallback = 0)
	{
		float value;
		if (m_mFloats.Find(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	bool GetBool(string key, bool fallback = false)
	{
		bool value;
		if (m_mBools.Find(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	string GetString(string key, string fallback = "")
	{
		string value;
		if (m_mStrings.Find(key, value))
			return value;
		return fallback;
	}
};

//------------------------------------------------------------------------------------------------
//! The sections the running pack declares, validated, and the reader for their owner values.
//! The core never names a pack: the declaration arrives through LF_ConfigService.GetPackSource().
class LF_PackConfigSections
{
	//------------------------------------------------------------------------------------------------
	//! \return The namespace of a pack id: `LF_Example` is `example`. Empty for an empty id.
	static string NamespaceOf(string packId)
	{
		string ns = packId;
		if (ns.StartsWith("LF_"))
			ns = ns.Substring(3, ns.Length() - 3);
		ns.ToLower();
		return ns;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsUpper(string ch)
	{
		string lower = ch;
		lower.ToLower();
		string upper = ch;
		upper.ToUpper();
		return ch == upper && ch != lower;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `type` is int, float, bool or string.
	static bool IsFieldType(string type)
	{
		return type == "int" || type == "float" || type == "bool" || type == "string";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `text` reads as a whole number.
	static bool IsWhole(string text)
	{
		if (text.IsEmpty())
			return false;
		for (int i = 0; i < text.Length(); i++)
		{
			string ch = text.Substring(i, 1);
			if (ch == "-" && i == 0 && text.Length() > 1)
				continue;
			if (ch.ToInt() == 0 && ch != "0")
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `text` reads as a number: whole, or with one decimal point.
	static bool IsNumber(string text)
	{
		int dot = text.IndexOf(".");
		if (dot < 0)
			return IsWhole(text);
		string whole = text.Substring(0, dot);
		string frac = text.Substring(dot + 1, text.Length() - dot - 1);
		if (whole == "-" || whole.IsEmpty())
			whole += "0";
		if (frac.IsEmpty() || frac.Contains("-") || frac.Contains("."))
			return false;
		return IsWhole(whole) && IsWhole(frac);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Why a section cannot be used, or empty when it holds. `ns` is the pack namespace;
	//! `coreKey` is true when the name is one of the core's own top-level keys.
	static string Problem(notnull LF_PackConfigSection section, string ns, bool coreKey)
	{
		string name = section.m_sName;
		if (name.IsEmpty())
			return "a section has no name";
		if (coreKey)
			return string.Format("section %1 is a core section name", name);
		if (ns.IsEmpty() || !name.StartsWith(ns))
			return string.Format("section %1 does not start with the pack namespace %2", name, ns);
		if (name.Length() > ns.Length() && !IsUpper(name.Substring(ns.Length(), 1)))
			return string.Format("section %1 must be the namespace %2 or continue it with an upper-case word", name, ns);
		if (!section.m_aFields || section.m_aFields.IsEmpty())
			return string.Format("section %1 declares no fields", name);

		array<string> seen = {};
		foreach (LF_PackConfigField field : section.m_aFields)
		{
			if (!field || field.m_sKey.IsEmpty())
				return string.Format("section %1 has a field with no key", name);
			if (seen.Contains(field.m_sKey))
				return string.Format("section %1 declares field %2 twice", name, field.m_sKey);
			seen.Insert(field.m_sKey);
			if (!IsFieldType(field.m_sType))
				return string.Format("%1.%2 has type %3; use int, float, bool or string", name, field.m_sKey, field.m_sType);
			bool numeric = field.m_sType == "int" || field.m_sType == "float";
			if (field.m_sType == "int" && !IsWhole(field.m_sDefault))
				return string.Format("%1.%2 default %3 is not a whole number", name, field.m_sKey, field.m_sDefault);
			if (field.m_sType == "float" && !IsNumber(field.m_sDefault))
				return string.Format("%1.%2 default %3 is not a number", name, field.m_sKey, field.m_sDefault);
			if (field.m_sType == "bool" && field.m_sDefault != "true" && field.m_sDefault != "false")
				return string.Format("%1.%2 default %3 is not true or false", name, field.m_sKey, field.m_sDefault);
			if (!numeric && (!field.m_sMin.IsEmpty() || !field.m_sMax.IsEmpty()))
				return string.Format("%1.%2 has a min or max but is not a number", name, field.m_sKey);
			if (numeric && ((!field.m_sMin.IsEmpty() && !IsNumber(field.m_sMin)) || (!field.m_sMax.IsEmpty() && !IsNumber(field.m_sMax))))
				return string.Format("%1.%2 has a min or max that is not a number", name, field.m_sKey);
			if (numeric && !field.m_sMin.IsEmpty() && !field.m_sMax.IsEmpty() && field.m_sMin.ToFloat() > field.m_sMax.ToFloat())
				return string.Format("%1.%2 has a min above its max", name, field.m_sKey);
			if (numeric && !field.m_sMin.IsEmpty() && field.m_sDefault.ToFloat() < field.m_sMin.ToFloat())
				return string.Format("%1.%2 default %3 is below its min", name, field.m_sKey, field.m_sDefault);
			if (numeric && !field.m_sMax.IsEmpty() && field.m_sDefault.ToFloat() > field.m_sMax.ToFloat())
				return string.Format("%1.%2 default %3 is above its max", name, field.m_sKey, field.m_sDefault);
		}
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The valid sections of the running pack. A section that clashes with a core section, breaks
	//! the namespace rule or has a bad field logs LF_Debug.Error and is left out; the rest stay.
	//! \param[in] log False for a quiet lookup (the boot path logs once).
	static void Collect(notnull array<ref LF_PackConfigSection> accepted, bool log = true)
	{
		accepted.Clear();
		LF_PackManifest manifest = LF_ConfigService.GetPackSource().GetManifest();
		if (!manifest || !manifest.m_aConfigSections)
			return;

		string ns = NamespaceOf(manifest.m_sPackId);
		array<string> names = {};
		foreach (LF_PackConfigSection section : manifest.m_aConfigSections)
		{
			if (!section)
				continue;
			string why = Problem(section, ns, LF_ConfigService.IsCoreTopLevelKey(section.m_sName));
			if (why.IsEmpty() && names.Contains(section.m_sName))
				why = string.Format("section %1 is declared twice", section.m_sName);
			if (!why.IsEmpty())
			{
				if (log)
					LF_Debug.Error("Packs", string.Format("%1 config: %2; the section is ignored", manifest.m_sPackId, why));
				continue;
			}
			names.Insert(section.m_sName);
			accepted.Insert(section);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `key` names a valid pack section; the owner file may carry it.
	static bool IsPackSection(string key)
	{
		array<ref LF_PackConfigSection> sections = {};
		Collect(sections, false);
		foreach (LF_PackConfigSection section : sections)
		{
			if (section.m_sName == key)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one pack section from the owner layer over its declared defaults. Per field: a value of
	//! the wrong type or outside min..max is refused with LF_Debug.Error and the default stays.
	//! \return The values, or null when no valid pack section has that name.
	static LF_PackConfigValues Read(string sectionName)
	{
		array<ref LF_PackConfigSection> sections = {};
		Collect(sections, false);
		LF_PackConfigSection section;
		foreach (LF_PackConfigSection candidate : sections)
		{
			if (candidate.m_sName == sectionName)
				section = candidate;
		}
		if (!section)
			return null;

		LF_PackConfigValues values = new LF_PackConfigValues();
		values.m_sSection = sectionName;
		foreach (LF_PackConfigField field : section.m_aFields)
		{
			if (field.m_sType == "int")
				values.SetInt(field.m_sKey, field.m_sDefault.ToInt());
			else if (field.m_sType == "float")
				values.SetFloat(field.m_sKey, field.m_sDefault.ToFloat());
			else if (field.m_sType == "bool")
				values.SetBool(field.m_sKey, field.m_sDefault == "true");
			else
				values.SetString(field.m_sKey, field.m_sDefault);
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(sectionName);
		if (!reader)
			return values;

		array<string> known = {};
		foreach (LF_PackConfigField field : section.m_aFields)
		{
			known.Insert(field.m_sKey);
		}
		reader.WarnUnknownKeys(known);

		foreach (LF_PackConfigField field : section.m_aFields)
		{
			if (field.m_sType == "int")
			{
				int whole = values.GetInt(field.m_sKey);
				int lo = int.MIN;
				int hi = int.MAX;
				if (!field.m_sMin.IsEmpty())
					lo = field.m_sMin.ToInt();
				if (!field.m_sMax.IsEmpty())
					hi = field.m_sMax.ToInt();
				if (reader.ReadInt(field.m_sKey, whole, lo, hi))
					values.SetInt(field.m_sKey, whole);
			}
			else if (field.m_sType == "float")
			{
				float number = values.GetFloat(field.m_sKey);
				float flo = -1000000000.0;
				float fhi = 1000000000.0;
				if (!field.m_sMin.IsEmpty())
					flo = field.m_sMin.ToFloat();
				if (!field.m_sMax.IsEmpty())
					fhi = field.m_sMax.ToFloat();
				if (reader.ReadFloat(field.m_sKey, number, flo, fhi))
					values.SetFloat(field.m_sKey, number);
			}
			else if (field.m_sType == "bool")
			{
				bool flag = values.GetBool(field.m_sKey);
				if (reader.ReadBool(field.m_sKey, flag))
					values.SetBool(field.m_sKey, flag);
			}
			else
			{
				string text = values.GetString(field.m_sKey);
				if (reader.ReadString(field.m_sKey, text))
					values.SetString(field.m_sKey, text);
			}
		}

		values.m_iRejected = reader.GetRejectedCount();
		values.m_iUnknown = reader.GetUnknownCount();
		return values;
	}
};
