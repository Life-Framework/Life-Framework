//------------------------------------------------------------------------------------------------
//! One step of the owner-config migration chain (#303): rewrites a document written for schema
//! FromVersion() into the shape of FromVersion() + 1. A step works on JSON text and returns the
//! new text, or an empty string when it cannot (the chain stops and the original file stays).
//! It does not touch "schemaVersion"; LF_ConfigMigrator stamps it after each step. A step that
//! reads the document must follow the KindOf-first rule (LF_ConfigReader.KindOf) before opening
//! anything: the engine crashes on StartObject over a number (Core/Config/Feature.md).
//------------------------------------------------------------------------------------------------
class LF_ConfigMigration
{
	//------------------------------------------------------------------------------------------------
	//! \return The schema version this step reads; it produces the next one.
	int FromVersion()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A short name for log lines.
	string GetName()
	{
		return "migration";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] json The document at FromVersion().
	//! \return The document in the next shape, or an empty string on failure.
	string Apply(string json)
	{
		return json;
	}
};

//------------------------------------------------------------------------------------------------
enum LF_EConfigMigrationResult
{
	NOT_NEEDED,	//!< No schemaVersion below the build's, or an unreadable version.
	MIGRATED,	//!< Backed up, migrated and written.
	FAILED		//!< The original file is untouched; the caller boots on shipped values.
};

//------------------------------------------------------------------------------------------------
//! Runs the migration chain on the owner's config.json at boot. The order is fixed: run the chain
//! in memory, back the file up (never over an existing backup), write the migrated text. A
//! failure at any point leaves the original file as it was and the server boots on shipped
//! values; it never refuses to boot.
//------------------------------------------------------------------------------------------------
class LF_ConfigMigrator
{
	static const string FEATURE = "Config";

	//------------------------------------------------------------------------------------------------
	//! The migrations this build ships, filled in version order. Empty while the schema is 1.
	//! A schema bump adds its step here in the same change.
	static void BuildShipped(notnull array<ref LF_ConfigMigration> steps)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! \return The document's schemaVersion, or -1 when it is absent, not a whole number or the
	//! document does not parse. Uses only calls that never crash (KindOf first).
	static int PeekVersion(string json)
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString(json))
			return -1;
		if (LF_ConfigReader.KindOf(ctx, "schemaVersion") != LF_EConfigJsonKind.NUMBER)
			return -1;
		int version;
		if (!ctx.ReadValue("schemaVersion", version))
			return -1;
		return version;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the document's schemaVersion in the text, adding it as the first key when missing.
	//! \return The new text, or an empty string when the value is not a plain number.
	static string StampVersion(string json, int version)
	{
		string ws = " \t\r\n";
		string digits = "0123456789-";
		int key = json.IndexOf("\"schemaVersion\"");
		if (key < 0)
		{
			int open = json.IndexOf("{");
			if (open < 0)
				return string.Empty;
			string rest = json.Substring(open + 1, json.Length() - open - 1);
			string sep = ",";
			if (rest.Trim().StartsWith("}"))
				sep = string.Empty;
			return json.Substring(0, open + 1) + string.Format("\"schemaVersion\":%1%2", version, sep) + rest;
		}

		int i = key + 15;
		int len = json.Length();
		while (i < len && ws.Contains(json.Get(i)))
		{
			i++;
		}
		if (i >= len || json.Get(i) != ":")
			return string.Empty;
		i++;
		while (i < len && ws.Contains(json.Get(i)))
		{
			i++;
		}
		int j = i;
		while (j < len && digits.Contains(json.Get(j)))
		{
			j++;
		}
		if (j == i)
			return string.Empty;
		return json.Substring(0, i) + version.ToString() + json.Substring(j, len - j);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The step that reads this version, or null.
	protected static LF_ConfigMigration FindStep(array<ref LF_ConfigMigration> steps, int fromVersion)
	{
		if (!steps)
			return null;
		foreach (LF_ConfigMigration step : steps)
		{
			if (step && step.FromVersion() == fromVersion)
				return step;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when some step reads this version (so a version below 1 is a real old schema).
	static bool HasStepFrom(array<ref LF_ConfigMigration> steps, int fromVersion)
	{
		return FindStep(steps, fromVersion) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the chain in memory. Pure: no file is touched. Logs each step.
	//! \param[in] json The document.
	//! \param[in] steps The available steps.
	//! \param[in] target The schema the build reads.
	//! \param[out] migrated The document at the target on success; unchanged otherwise.
	//! \return True when the document now reads at the target.
	static bool RunChain(string json, array<ref LF_ConfigMigration> steps, int target, out string migrated, string source = "config")
	{
		int version = PeekVersion(json);
		string current = json;
		while (version < target)
		{
			LF_ConfigMigration step = FindStep(steps, version);
			if (!step)
			{
				LF_Debug.Error(FEATURE, string.Format("%1: no migration from schemaVersion %2 to %3", source, version, version + 1));
				return false;
			}

			string next = step.Apply(current);
			if (next.IsEmpty())
			{
				LF_Debug.Error(FEATURE, string.Format("%1: migration %2 (v%3 to v%4) failed", source, step.GetName(), version, version + 1));
				return false;
			}

			next = StampVersion(next, version + 1);
			if (next.IsEmpty() || PeekVersion(next) != version + 1)
			{
				LF_Debug.Error(FEATURE, string.Format("%1: migration %2 (v%3 to v%4) did not leave a valid document", source, step.GetName(), version, version + 1));
				return false;
			}

			LF_Debug.Info(FEATURE, string.Format("%1: migration %2 applied (v%3 to v%4)", source, step.GetName(), version, version + 1));
			current = next;
			version++;
		}

		migrated = current;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A backup path for this file and version that nothing exists at yet.
	//! \param[in] stamp The timestamp in the name; empty uses the current UTC time. Tests pass a fixed one.
	static string BackupPath(string path, int version, string stamp = string.Empty)
	{
		if (stamp.IsEmpty())
		{
			int year, month, day, hour, minute, second;
			System.GetYearMonthDayUTC(year, month, day);
			System.GetHourMinuteSecondUTC(hour, minute, second);
			stamp = string.Format("%1%2%3-%4%5%6", year, Pad2(month), Pad2(day), Pad2(hour), Pad2(minute), Pad2(second));
		}
		string candidate = string.Format("%1.bak-v%2-%3", path, version, stamp);
		int n = 2;
		while (FileIO.FileExists(candidate))
		{
			candidate = string.Format("%1.bak-v%2-%3-%4", path, version, stamp, n);
			n++;
		}
		return candidate;
	}

	protected static string Pad2(int value)
	{
		if (value < 10)
			return "0" + value.ToString();
		return value.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Migrates the file at path when its schemaVersion is below target.
	//! \param[in] path The owner's config.json.
	//! \param[in,out] text The file's text; replaced by the migrated text on MIGRATED.
	//! \return NOT_NEEDED, MIGRATED, or FAILED (original file untouched).
	static LF_EConfigMigrationResult MigrateFile(string path, inout string text, array<ref LF_ConfigMigration> steps, int target, string backupStamp = string.Empty)
	{
		int version = PeekVersion(text);
		if (version < 0 || version >= target)
			return LF_EConfigMigrationResult.NOT_NEEDED;
		// A version below 1 is only a real old schema when a step reads it.
		if (version < 1 && !HasStepFrom(steps, version))
			return LF_EConfigMigrationResult.NOT_NEEDED;

		LF_Debug.Info(FEATURE, string.Format("%1 is schemaVersion %2, this build reads %3; migrating", path, version, target));

		string migrated;
		if (!RunChain(text, steps, target, migrated, path))
		{
			LF_Debug.Error(FEATURE, string.Format("migration of %1 failed; the file is unchanged and every section uses the shipped defaults", path));
			return LF_EConfigMigrationResult.FAILED;
		}

		string backup = BackupPath(path, version, backupStamp);
		if (!FileIO.CopyFile(path, backup))
		{
			LF_Debug.Error(FEATURE, string.Format("cannot back up %1 to %2; not migrating, every section uses the shipped defaults", path, backup));
			return LF_EConfigMigrationResult.FAILED;
		}
		LF_Debug.Info(FEATURE, string.Format("backed up %1 to %2", path, backup));

		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot write the migrated %1; the file is unchanged and every section uses the shipped defaults", path));
			return LF_EConfigMigrationResult.FAILED;
		}
		handle.Write(migrated);
		handle.Close();

		LF_Debug.Info(FEATURE, string.Format("%1 migrated to schemaVersion %2", path, target));
		text = migrated;
		return LF_EConfigMigrationResult.MIGRATED;
	}
};
