//------------------------------------------------------------------------------------------------
//! Config service: the owner's `$profile:LifeFramework/config.json` merged over the shipped
//! `.conf` defaults, field by field. Precedence is owner JSON, then pack `.conf`, then core `.conf`.
//!
//! The service is generic. It parses the owner file, hands a feature a reader positioned on its
//! section, and asks the pack source which `.conf` overlays sit on a core resource. Each feature
//! applies the layers to its own config instance (shops: LF_ShopPricesConfig.ApplyLayers).
//!
//! The engine parses the JSON (SCR_JsonLoadContext); LF_ConfigReader reads it key by key. The
//! engine's class reader is not used: it cannot tell a missing field from a default, silently
//! drops a value of the wrong type, and crashes on a list element that is not an object
//! (LF_Test_ConfigJsonProbe and Core/Config/Feature.md).
//!
//! The owner file is server-side. A client never reads its own profile's config.json; clients
//! receive owner values through the stream in #285.
//------------------------------------------------------------------------------------------------
//! How an owner file's schemaVersion compares with the schema this build reads (#302).
enum LF_EConfigSchemaStatus
{
	NONE,		//!< No owner file, or the client.
	CURRENT,	//!< Same version, or no schemaVersion (version 1).
	OLDER,		//!< Written for an older schema; read as-is, migrations are #303.
	NEWER,		//!< Written for a newer schema; known fields still apply, the rest warns.
	INVALID		//!< Not a whole number of at least 1; read as version 1.
};

//------------------------------------------------------------------------------------------------
class LF_ConfigService
{
	static const string FEATURE = "Config";
	static const string OWNER_PATH = "$profile:LifeFramework/config.json";
	static const string EXAMPLE_PATH = "$profile:LifeFramework/config.example.json";

	//! The schema this build reads. A file without schemaVersion is version 1.
	static const int SCHEMA_VERSION = 1;

	//! Owner file text, kept so every section read starts from a fresh cursor. Empty: no owner layer.
	protected static string s_sOwnerJson;
	protected static string s_sOwnerSource;
	protected static int s_iOwnerSchemaVersion;
	protected static LF_EConfigSchemaStatus s_eSchemaStatus;
	protected static bool s_bLoaded;

	//! D4 seam: where pack `.conf` overlays come from. Null means the default source (none).
	protected static ref LF_ConfigPackSource s_PackSource;

	//------------------------------------------------------------------------------------------------
	//! The boot hook (LF_GameModeRoleplay.OnGameStart). Server: reads the owner file, or writes the
	//! example beside it when there is none. Client: no owner layer.
	static void Boot()
	{
		if (!Replication.IsServer())
		{
			s_sOwnerJson = string.Empty;
			s_sOwnerSource = string.Empty;
			s_iOwnerSchemaVersion = 0;
			s_eSchemaStatus = LF_EConfigSchemaStatus.NONE;
			s_bLoaded = true;
			LF_Debug.Info(FEATURE, "client: no owner layer; shipped defaults until the server streams owner values (#285)");
			return;
		}

		LoadOwnerFile(OWNER_PATH, EXAMPLE_PATH);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads an owner file. A missing file boots on shipped defaults and writes the example at
	//! examplePath when nothing is there yet; an existing file is never written.
	//! \return True when an owner layer is active.
	static bool LoadOwnerFile(string path, string examplePath)
	{
		s_sOwnerJson = string.Empty;
		s_sOwnerSource = path;
		s_iOwnerSchemaVersion = 0;
		s_eSchemaStatus = LF_EConfigSchemaStatus.NONE;
		s_bLoaded = true;

		if (!FileIO.FileExists(path))
		{
			LF_Debug.Info(FEATURE, string.Format("no owner config at %1; every section uses the shipped defaults", path));
			WriteExample(examplePath);
			return false;
		}

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot open %1; every section uses the shipped defaults", path));
			return false;
		}

		string text;
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			text += line + "\n";
		}
		handle.Close();

		return LoadOwnerString(text, path);
	}

	//------------------------------------------------------------------------------------------------
	//! Installs an owner layer from JSON text. Tests use it directly; LoadOwnerFile ends here.
	//! A document that does not parse logs an error and leaves no owner layer.
	//! \param[in] json The whole owner document.
	//! \param[in] source Where it came from, for log lines.
	//! \param[in] buildSchema The schema this build reads; a test passes another to reach the older path.
	//! \return True when an owner layer is active.
	static bool LoadOwnerString(string json, string source, int buildSchema = SCHEMA_VERSION)
	{
		s_sOwnerJson = string.Empty;
		s_sOwnerSource = source;
		s_iOwnerSchemaVersion = 0;
		s_eSchemaStatus = LF_EConfigSchemaStatus.NONE;
		s_bLoaded = true;

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString(json))
		{
			LF_Debug.Error(FEATURE, string.Format("%1 is not valid JSON; every section uses the shipped defaults", source));
			return false;
		}

		int schemaVersion = 1;
		bool invalid;
		if (ctx.DoesKeyExist("schemaVersion"))
		{
			if (!ctx.ReadValue("schemaVersion", schemaVersion))
			{
				LF_Debug.Error(FEATURE, string.Format("%1: schemaVersion is not a whole number; reading it as version 1", source));
				schemaVersion = 1;
				invalid = true;
			}
			else if (schemaVersion < 1)
			{
				LF_Debug.Error(FEATURE, string.Format("%1: schemaVersion %2 is below 1; reading it as version 1", source, schemaVersion));
				schemaVersion = 1;
				invalid = true;
			}
		}

		s_sOwnerJson = json;
		s_iOwnerSchemaVersion = schemaVersion;
		s_eSchemaStatus = ClassifySchema(schemaVersion, buildSchema);
		if (invalid)
			s_eSchemaStatus = LF_EConfigSchemaStatus.INVALID;

		if (s_eSchemaStatus == LF_EConfigSchemaStatus.NEWER)
			LF_Debug.Error(FEATURE, string.Format("%1 is written for schemaVersion %2 but this build reads up to %3; update the server. Fields this build knows still apply, anything else is ignored with a warning", source, schemaVersion, buildSchema));
		else if (s_eSchemaStatus == LF_EConfigSchemaStatus.OLDER)
			LF_Debug.Info(FEATURE, string.Format("%1 is written for schemaVersion %2, older than this build's %3; read as-is (migrations are #303)", source, schemaVersion, buildSchema));
		else
			LF_Debug.Info(FEATURE, string.Format("owner config loaded from %1 (schemaVersion %2, this build reads %3)", source, schemaVersion, buildSchema));

		WarnUnknownSections(json, source);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Compares a file's schemaVersion with the build's.
	static LF_EConfigSchemaStatus ClassifySchema(int fileVersion, int buildVersion)
	{
		if (fileVersion > buildVersion)
			return LF_EConfigSchemaStatus.NEWER;
		if (fileVersion < buildVersion)
			return LF_EConfigSchemaStatus.OLDER;
		return LF_EConfigSchemaStatus.CURRENT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How the loaded owner file's schemaVersion compares with this build; NONE without one.
	static LF_EConfigSchemaStatus GetSchemaStatus()
	{
		EnsureLoaded();
		return s_eSchemaStatus;
	}

	//------------------------------------------------------------------------------------------------
	//! The top-level keys this build reads: schemaVersion and one key per section. A new section
	//! adds its key here when its feature reads it (#282, #283).
	static bool IsKnownTopLevelKey(string key)
	{
		return key == "schemaVersion" || key == LF_ShopPricesConfig.SECTION;
	}

	//------------------------------------------------------------------------------------------------
	//! Warns once per top-level key no section reads, in file order. \return How many.
	//! The engine cannot list the root's keys (an anonymous map fails), so the document is wrapped
	//! under one name and the wrapper's keys are listed.
	//! \param[in] json The whole owner document.
	static int WarnUnknownSections(string json, string source)
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString("{\"doc\":" + json + "}"))
			return 0;

		array<string> keys = {};
		if (LF_ConfigReader.KindOf(ctx, "doc") != LF_EConfigJsonKind.OBJECT)
			return 0;
		LF_ConfigReader.ListKeys(ctx, "doc", keys);
		int unknown;
		foreach (string key : keys)
		{
			if (IsKnownTopLevelKey(key))
				continue;
			unknown++;
			LF_Debug.Warn(FEATURE, string.Format("%1: unknown top-level key %2; ignored", source, key));
		}
		return unknown;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an owner file is loaded and parsed.
	static bool HasOwnerLayer()
	{
		EnsureLoaded();
		return !s_sOwnerJson.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The owner file's schemaVersion, or 0 without an owner layer.
	static int GetOwnerSchemaVersion()
	{
		EnsureLoaded();
		return s_iOwnerSchemaVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens one owner section for reading. The caller applies fields through the reader.
	//! \param[in] section Top-level key, e.g. "shops".
	//! \return A reader on the section, or null when there is no owner layer, no such section, or
	//! the section is not an object (logged).
	static LF_ConfigReader OpenOwnerSection(string section)
	{
		EnsureLoaded();
		if (s_sOwnerJson.IsEmpty())
			return null;

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString(s_sOwnerJson))
			return null;

		LF_EConfigJsonKind kind = LF_ConfigReader.KindOf(ctx, section);
		if (kind == LF_EConfigJsonKind.ABSENT)
			return null;

		if (kind != LF_EConfigJsonKind.OBJECT)
		{
			LF_Debug.Error(FEATURE, string.Format("%1: section %2 is not an object; the section uses the shipped defaults", s_sOwnerSource, section));
			return null;
		}

		return new LF_ConfigReader(ctx, section);
	}

	//------------------------------------------------------------------------------------------------
	//! The pack `.conf` overlays for one core resource, lowest precedence first.
	//! \param[in] coreConf The core `.conf` the feature loaded.
	//! \param[out] overlays Filled with overlay resources; empty when the pack adds none.
	static void GetPackOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		overlays.Clear();
		GetPackSource().GetOverlays(coreConf, overlays);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The active pack source; the default source supplies no overlays.
	static LF_ConfigPackSource GetPackSource()
	{
		if (!s_PackSource)
			s_PackSource = new LF_ConfigPackSource();
		return s_PackSource;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the pack source. D4 decides the real one; tests install a stub.
	//! \param[in] source The new source, or null for the default.
	static void SetPackSource(LF_ConfigPackSource source)
	{
		s_PackSource = source;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the owner layer and the pack source. Tests call it before and after they install a
	//! layer; the next read loads the server's owner file again.
	static void Reset()
	{
		s_sOwnerJson = string.Empty;
		s_sOwnerSource = string.Empty;
		s_iOwnerSchemaVersion = 0;
		s_eSchemaStatus = LF_EConfigSchemaStatus.NONE;
		s_bLoaded = false;
		s_PackSource = null;
	}

	//------------------------------------------------------------------------------------------------
	//! A feature can ask for a section before the boot hook ran (a lazily built catalog); load then.
	protected static void EnsureLoaded()
	{
		if (s_bLoaded)
			return;

		Boot();
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the example owner file when nothing exists at that path.
	protected static void WriteExample(string examplePath)
	{
		if (examplePath.IsEmpty() || FileIO.FileExists(examplePath))
			return;

		string parent = FilePath.StripFileName(examplePath);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileHandle handle = FileIO.OpenFile(examplePath, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot write the example owner config at %1", examplePath));
			return;
		}

		handle.WriteLine("{");
		handle.WriteLine(string.Format("  \"schemaVersion\": %1,", SCHEMA_VERSION));
		handle.WriteLine("  \"shops\": {");
		handle.WriteLine("    \"defaultBuyPrice\": 50,");
		handle.WriteLine("    \"defaultSellPrice\": 25,");
		handle.WriteLine("    \"priceRules\": {");
		handle.WriteLine("      \"Prefabs/Items/Food/\": { \"buyPrice\": 10, \"sellPrice\": 5 },");
		handle.WriteLine("      \"Prefabs/Weapons/\": { \"hidden\": true }");
		handle.WriteLine("    },");
		handle.WriteLine("    \"extraItemPrefabs\": []");
		handle.WriteLine("  }");
		handle.WriteLine("}");
		handle.Close();

		LF_Debug.Info(FEATURE, string.Format("wrote the example owner config to %1; copy it to config.json to use it", examplePath));
	}
};

//------------------------------------------------------------------------------------------------
//! D4 seam: the source of pack `.conf` overlays. The core never names the pack, so the pack layer
//! arrives through this one class. The default supplies none, which is also correct under D4
//! option (a), where the pack ships a file at the core path and the engine loads it in its place.
//! Option (b) is a subclass that reads an overlay array from the pack-owned LF_MissionHeader;
//! option (c) is a subclass that reads a manifest. Either installs itself with
//! LF_ConfigService.SetPackSource, and the field-by-field merge below it does not change.
//------------------------------------------------------------------------------------------------
class LF_ConfigPackSource
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] coreConf The core `.conf` a feature loaded.
	//! \param[out] overlays Pack `.conf` resources of the same class, lowest precedence first.
	void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! \return A short name for log lines.
	string GetName()
	{
		return "none";
	}
};

//------------------------------------------------------------------------------------------------
//! What a JSON key holds, found with calls that never crash. StartObject is not a type check: it
//! returns true on a string, bool, null or list and crashes the process on a number, so the
//! reader calls it only after KindOf says OBJECT (LF_Test_ConfigJsonProbe).
enum LF_EConfigJsonKind
{
	ABSENT,
	NUMBER,
	BOOL,
	STRING,		//!< A string or null.
	LIST,
	OBJECT
};

//------------------------------------------------------------------------------------------------
//! Per-field reader over one owner section. A key that is absent leaves the lower layer's value;
//! a key of the wrong kind logs LF_Debug.Error, names the field, and leaves the lower layer's
//! value. Never VMEs or crashes on bad input.
//!
//! JSON keys are the script member name without its Hungarian prefix, lower camel case:
//! m_iDefaultBuyPrice is "defaultBuyPrice", m_aPriceRules is "priceRules".
//!
//! Records are an object keyed by each record's stable key, not a list: the engine cannot read an
//! anonymous list element safely (a number or string where a record should be crashes both the
//! cursor and the class reader), while a named value can be checked first with KindOf.
//------------------------------------------------------------------------------------------------
class LF_ConfigReader
{
	protected ref SCR_JsonLoadContext m_Ctx;
	protected ref array<string> m_aPath = {};
	protected ref array<string> m_aRecordKeys = {};
	//! The keys of each open object, innermost last, listed before it was entered.
	protected ref array<ref array<string>> m_aKeyStack = {};
	protected int m_iApplied;
	protected int m_iRejected;
	protected int m_iUnknown;

	//------------------------------------------------------------------------------------------------
	//! Opens the named section of a document whose cursor is at the root. The caller has checked
	//! with KindOf that it is an object.
	void LF_ConfigReader(SCR_JsonLoadContext ctx, string section)
	{
		m_Ctx = ctx;
		m_aPath.Insert(section);
		PushKeys(section);
		m_Ctx.StartObject(section);
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the keys of an object in file order without entering it; an empty name lists the root.
	//! Call it only on an object (KindOf says OBJECT): the engine crashes on some other kinds.
	//! \return True when the engine listed them.
	static bool ListKeys(notnull SCR_JsonLoadContext ctx, string name, notnull array<string> keys)
	{
		keys.Clear();
		int count;
		if (!ctx.StartMap(name, count))
			return false;

		for (int i = 0; i < count; i++)
		{
			string key;
			if (ctx.ReadMapKey(i, key))
				keys.Insert(key);
		}
		ctx.EndMap();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void PushKeys(string name)
	{
		array<string> keys = {};
		ListKeys(m_Ctx, name, keys);
		m_aKeyStack.Insert(keys);
	}

	//------------------------------------------------------------------------------------------------
	protected void PopKeys()
	{
		if (m_aKeyStack.Count() > 0)
			m_aKeyStack.Remove(m_aKeyStack.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Warns for every key of the current object that is not in `known`, in file order. The key
	//! stays in the file and is ignored.
	//! \return How many keys it warned about.
	int WarnUnknownKeys(notnull array<string> known)
	{
		if (m_aKeyStack.Count() == 0)
			return 0;

		int warned;
		array<string> keys = m_aKeyStack[m_aKeyStack.Count() - 1];
		foreach (string key : keys)
		{
			if (known.Contains(key))
				continue;
			warned++;
			m_iUnknown++;
			LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("%1.%2 is not a known key; ignored", GetPath(), key));
		}
		return warned;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Unknown keys this reader warned about.
	int GetUnknownCount()
	{
		return m_iUnknown;
	}

	//------------------------------------------------------------------------------------------------
	//! Classifies one key of the open object without moving the cursor.
	static LF_EConfigJsonKind KindOf(notnull SCR_JsonLoadContext ctx, string key)
	{
		if (!ctx.DoesKeyExist(key))
			return LF_EConfigJsonKind.ABSENT;

		int asInt;
		float asFloat;
		if (ctx.ReadValue(key, asInt) || ctx.ReadValue(key, asFloat))
			return LF_EConfigJsonKind.NUMBER;

		bool asBool;
		if (ctx.ReadValue(key, asBool))
			return LF_EConfigJsonKind.BOOL;

		string asString;
		if (ctx.ReadValue(key, asString))
			return LF_EConfigJsonKind.STRING;

		int count;
		if (ctx.StartArray(key, count))
		{
			ctx.EndArray();
			return LF_EConfigJsonKind.LIST;
		}

		return LF_EConfigJsonKind.OBJECT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fields this reader took from the owner file.
	int GetAppliedCount()
	{
		return m_iApplied;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Fields and records this reader refused.
	int GetRejectedCount()
	{
		return m_iRejected;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The dotted path of the current object, e.g. "shops.priceRules.Prefabs/Weapons/".
	string GetPath()
	{
		string path;
		foreach (int i, string part : m_aPath)
		{
			if (i > 0)
				path += ".";
			path += part;
		}
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the current object has this key.
	bool Has(string key)
	{
		return m_Ctx.DoesKeyExist(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads an int when present. A value outside min..max is refused: it logs the value and the
	//! limit it broke, and the lower layer's value stays. The limits are the ones the config's
	//! `[Attribute(..., params: "min max")]` declares.
	//! \return True when the value was taken.
	bool ReadInt(string key, inout int value, int min = int.MIN, int max = int.MAX)
	{
		if (!m_Ctx.DoesKeyExist(key))
			return false;

		int read;
		if (!m_Ctx.ReadValue(key, read))
			return Reject(key, "a whole number", value.ToString());

		if (read < min)
			return RejectValue(key, read.ToString(), string.Format("below the minimum %1", min), value.ToString());
		if (read > max)
			return RejectValue(key, read.ToString(), string.Format("above the maximum %1", max), value.ToString());

		value = read;
		m_iApplied++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a bool when present. \return True when the value was taken.
	bool ReadBool(string key, inout bool value)
	{
		if (!m_Ctx.DoesKeyExist(key))
			return false;

		bool read;
		if (!m_Ctx.ReadValue(key, read))
			return Reject(key, "true or false", value.ToString());

		value = read;
		m_iApplied++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a string when present; null reads as an empty string. \return True when the value was taken.
	bool ReadString(string key, inout string value)
	{
		if (!m_Ctx.DoesKeyExist(key))
			return false;

		string read;
		if (!m_Ctx.ReadValue(key, read))
			return Reject(key, "a string", "\"" + value + "\"");

		value = read;
		m_iApplied++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a list of strings when present. \return True when the list was taken.
	bool ReadStrings(string key, notnull array<string> values)
	{
		LF_EConfigJsonKind kind = KindOf(m_Ctx, key);
		if (kind == LF_EConfigJsonKind.ABSENT)
			return false;

		if (kind != LF_EConfigJsonKind.LIST)
			return Reject(key, "a list of strings", "the shipped list");

		// The engine fills a string list up to its first bad element and then reports failure.
		array<string> read = {};
		if (!m_Ctx.ReadValue(key, read) || !read)
			return Reject(key, "a list of strings", "the shipped list");

		values.Copy(read);
		m_iApplied++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens an object of records keyed by their stable key. Pair with EndRecords when the return
	//! is not -1, and read the keys with GetRecordKey.
	//! \return The record count, or -1 when the key is absent or not an object.
	int BeginRecords(string key)
	{
		LF_EConfigJsonKind kind = KindOf(m_Ctx, key);
		if (kind == LF_EConfigJsonKind.ABSENT)
			return -1;

		if (kind != LF_EConfigJsonKind.OBJECT)
		{
			Reject(key, "an object of records keyed by their key", "the shipped records");
			return -1;
		}

		ListKeys(m_Ctx, key, m_aRecordKeys);
		PushKeys(key);
		m_Ctx.StartObject(key);
		m_aPath.Insert(key);
		return m_aRecordKeys.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The key of one record in the open records object, in file order.
	string GetRecordKey(int index)
	{
		if (!m_aRecordKeys.IsIndexValid(index))
			return string.Empty;
		return m_aRecordKeys[index];
	}

	//------------------------------------------------------------------------------------------------
	void EndRecords()
	{
		PopKeys();
		m_Ctx.EndObject();
		m_aPath.Remove(m_aPath.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Enters one record of the open records object. Pair with EndRecord when it returns true.
	bool BeginRecord(string recordKey)
	{
		if (KindOf(m_Ctx, recordKey) != LF_EConfigJsonKind.OBJECT)
		{
			m_iRejected++;
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1.%2 is not a record (an object); record skipped", GetPath(), recordKey));
			return false;
		}

		PushKeys(recordKey);
		m_Ctx.StartObject(recordKey);
		m_aPath.Insert(recordKey);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void EndRecord()
	{
		PopKeys();
		m_Ctx.EndObject();
		m_aPath.Remove(m_aPath.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs a record the feature cannot use and counts it. Call inside the record.
	void RejectRecord(string reason)
	{
		m_iRejected++;
		LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1: %2; record skipped", GetPath(), reason));
	}

	//------------------------------------------------------------------------------------------------
	protected bool RejectValue(string key, string value, string reason, string kept)
	{
		m_iRejected++;
		LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1.%2 = %3 is %4; keeping %5", GetPath(), key, value, reason, kept));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Reject(string key, string expected, string kept)
	{
		m_iRejected++;
		LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1.%2 is not %3; keeping %4", GetPath(), key, expected, kept));
		return false;
	}
};
