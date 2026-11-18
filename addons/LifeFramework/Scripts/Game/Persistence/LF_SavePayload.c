//------------------------------------------------------------------------------------------------
//! One save section (#2841): a payload with its own name and its own small version, written as a
//! named sub-object after the frozen positional payloads of LF_PersistenceComponentSerializer.
//!
//! ADDING A PAYLOAD. Subclass this in the feature's own folder (LF_<Area>SavePayload.c). Nothing
//! else is registered: LF_SaveSections finds every subclass by reflection, the way LF_StaticReset
//! finds its hooks. Two PRs that add different sections touch no shared line and no shared number.
//!
//! CONTRACT.
//!  - Name(): lowercase letters, digits and '_', unique, never one of the legacy payload names.
//!    Saved in every save; never rename it.
//!  - Version(): this section's own version, from 1. Raise it when the written shape changes and
//!    read every older version in ReadSection. A save whose section is newer than Version() is not
//!    read (the load counts as damaged, so autosave keeps the save on disk).
//!  - WriteSection(): write the rows with context.WriteValue("rows", rows). The key only has to be
//!    unique inside this section's own object.
//!  - ReadSection(): read and apply. Replacement, never accumulation: a load also runs when saved
//!    data is re-applied to a live session, and the second pass must end in the same state. Return
//!    false when the read failed; the live state is then left untouched.
//!  - OnAbsent(): the save has no such section (it predates it, or the section was off when it was
//!    written). Seed what an older save implies; the default does nothing.
//!  - Order(): sections are written and applied in (Order(), Name()) order, after every legacy
//!    payload. A section that must apply after another one returns a higher number.
//!  - IsActive(): false keeps the section out of the save (test fixtures).
//!
//! Records follow the append-only rule (docs/persistence.md "Record shapes"); the section's lock is
//! tools/validation/record-shapes/<name>.json.
//------------------------------------------------------------------------------------------------
class LF_SavePayload : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \return The section's saved name; empty skips the class (an abstract base).
	string Name()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This section's own version, from 1.
	int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Sort key before the name; higher applies later.
	int Order()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return False keeps the section out of the save.
	bool IsActive()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes this section's rows into its own object.
	//! \param[in] context Save context, positioned inside the section's object.
	void WriteSection(notnull SaveContext context)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Reads and applies this section's rows.
	//! \param[in] context Load context, positioned inside the section's object.
	//! \param[in] savedVersion The section version the save was written at, 1 to Version().
	//! \return False when the rows failed to read.
	bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded save has no such section.
	void OnAbsent()
	{
	}
}

//------------------------------------------------------------------------------------------------
//! The section registry and the section half of the store (#2841). Discovered by reflection over
//! LF_SavePayload; nothing lists the sections.
//------------------------------------------------------------------------------------------------
class LF_SaveSections
{
	//! The manifest key: the section names in written order.
	static const string MANIFEST = "sections";
	//! The version key inside each section's object.
	static const string VERSION_KEY = "v";
	//! Each section's object key is this prefix and its name, so no section can shadow a legacy key.
	static const string OBJECT_PREFIX = "sec_";

	protected static ref array<ref LF_SavePayload> s_aPayloads;
	//! Sections the last store read applied.
	protected static ref array<string> s_aLoaded;
	//! Sections the first store read of this world applied (the restart a test checks).
	protected static ref array<string> s_aBoot;
	protected static bool s_bBootCaptured;

	//------------------------------------------------------------------------------------------------
	//! World teardown (LF_StaticReset).
	static void ResetStatics()
	{
		s_aPayloads = null;
		s_aLoaded = null;
		s_aBoot = null;
		s_bBootCaptured = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the name is a legal section name.
	static bool IsValidName(string name)
	{
		if (name.IsEmpty())
			return false;

		string allowed = "abcdefghijklmnopqrstuvwxyz0123456789_";
		for (int i = 0; i < name.Length(); i++)
		{
			if (!allowed.Contains(name.Get(i)))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every section class, sorted by (Order(), Name()). A duplicate or illegal name is logged and skipped.
	//! \return The shared list; never null.
	static array<ref LF_SavePayload> All()
	{
		if (s_aPayloads)
			return s_aPayloads;

		s_aPayloads = new array<ref LF_SavePayload>();
		array<string> legacy = new array<string>();
		LF_PersistenceComponentSerializer.GetPayloadOrder(legacy);

		array<typename> types = {};
		typename base = LF_SavePayload;
		base.GetInheritedTypes(types);
		foreach (typename t : types)
		{
			if (t == base)
				continue;

			LF_SavePayload payload = LF_SavePayload.Cast(t.Spawn());
			if (!payload)
			{
				LF_Debug.Error("Persistence", "could not spawn save section " + t.ToString());
				continue;
			}

			string name = payload.Name();
			if (name.IsEmpty())
				continue;

			if (!IsValidName(name) || legacy.Contains(name))
			{
				LF_Debug.Error("Persistence", string.Format("save section %1 has an illegal name '%2' (lowercase, digits, '_', not a legacy payload); skipped", t.ToString(), name));
				continue;
			}

			if (Find(name))
			{
				LF_Debug.Error("Persistence", string.Format("save section '%1' is declared twice (%2); the second is skipped", name, t.ToString()));
				continue;
			}

			InsertSorted(payload);
		}

		return s_aPayloads;
	}

	//------------------------------------------------------------------------------------------------
	protected static void InsertSorted(notnull LF_SavePayload payload)
	{
		for (int i = 0; i < s_aPayloads.Count(); i++)
		{
			LF_SavePayload other = s_aPayloads.Get(i);
			if (payload.Order() < other.Order() || (payload.Order() == other.Order() && payload.Name().Compare(other.Name()) < 0))
			{
				s_aPayloads.InsertAt(payload, i);
				return;
			}
		}

		s_aPayloads.Insert(payload);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The section with that name, or null.
	static LF_SavePayload Find(string name)
	{
		if (!s_aPayloads)
			return null;

		foreach (LF_SavePayload payload : s_aPayloads)
		{
			if (payload.Name() == name)
				return payload;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The registered section with that name (discovering them first), or null.
	static LF_SavePayload Get(string name)
	{
		All();
		return Find(name);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the manifest and every active section.
	//! \param[in] context Save context, after the legacy payloads.
	//! \param[in] omit Section names to leave out (a test writing "a save from before this section").
	static void WriteAll(notnull SaveContext context, array<string> omit = null)
	{
		array<LF_SavePayload> written = {};
		array<string> names = {};
		foreach (LF_SavePayload payload : All())
		{
			if (!payload.IsActive())
				continue;

			if (omit && omit.Contains(payload.Name()))
				continue;

			written.Insert(payload);
			names.Insert(payload.Name());
		}

		context.WriteValue(MANIFEST, names);
		foreach (LF_SavePayload section : written)
		{
			context.StartObject(OBJECT_PREFIX + section.Name());
			context.WriteValue(VERSION_KEY, section.Version());
			int start = LF_SaveTiming.Now();
			section.WriteSection(context);
			LF_SaveTiming.Part(section.Name(), start);
			context.EndObject();
		}

		LF_Debug.Log("Persistence", string.Format("saved %1 section(s)", names.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the manifest and every section in it, then tells each registered section the save lacks
	//! that it is absent. A section that fails to read does not stop the ones after it: each sits in
	//! its own skippable object.
	//! \param[in] context Load context, after the legacy payloads.
	//! \param[out] failed Sections (or the manifest) that failed to read.
	//! \param[out] unread Sections this build cannot read (unknown name, or a newer section version).
	static void ReadAll(notnull LoadContext context, out int failed, out int unread)
	{
		failed = 0;
		unread = 0;
		s_aLoaded = new array<string>();
		array<string> names = {};
		if (!context.ReadValue(MANIFEST, names))
		{
			failed++;
			LF_Debug.Warn("Persistence", "save section manifest failed to read: no section is applied, each keeps its live state");
			return;
		}

		foreach (string name : names)
		{
			if (!context.StartObject(OBJECT_PREFIX + name))
			{
				failed++;
				LF_Debug.Warn("Persistence", string.Format("save section '%1' is in the manifest but its object failed to open; it keeps its live state", name));
				continue;
			}

			int savedVersion;
			context.ReadValue(VERSION_KEY, savedVersion);
			LF_SavePayload payload = Get(name);
			if (!payload)
			{
				unread++;
				LF_Debug.Warn("Persistence", string.Format("save section '%1' (version %2) is unknown to this build and is not read; autosave pauses so it is not dropped", name, savedVersion));
			}
			else if (savedVersion < 1 || savedVersion > payload.Version())
			{
				unread++;
				LF_Debug.Warn("Persistence", string.Format("save section '%1' is version %2, this build reads 1 to %3; not read, autosave pauses so it is not dropped", name, savedVersion, payload.Version()));
			}
			else if (payload.ReadSection(context, savedVersion))
			{
				s_aLoaded.Insert(name);
			}
			else
			{
				failed++;
				LF_Debug.Warn("Persistence", string.Format("save section '%1' (version %2) failed to read; it keeps its live state", name, savedVersion));
			}

			context.EndObject();
		}

		foreach (LF_SavePayload registered : All())
		{
			if (!names.Contains(registered.Name()))
				registered.OnAbsent();
		}

		LF_Debug.Info("Persistence", string.Format("read %1 of %2 save section(s), %3 failed, %4 not readable by this build", s_aLoaded.Count(), names.Count(), failed, unread));
	}

	//------------------------------------------------------------------------------------------------
	//! A save with no sections (the positional format): every registered section is absent.
	static void MarkAllAbsent()
	{
		s_aLoaded = new array<string>();
		foreach (LF_SavePayload payload : All())
		{
			payload.OnAbsent();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called once at the end of every store read: the first read of this world is the boot's.
	static void CaptureBoot()
	{
		if (s_bBootCaptured)
			return;

		s_bBootCaptured = true;
		s_aBoot = new array<string>();
		if (s_aLoaded)
			s_aBoot.Copy(s_aLoaded);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the last store read applied that section.
	static bool WasLoaded(string name)
	{
		return s_aLoaded && s_aLoaded.Contains(name);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the first store read of this world (the restart) applied that section. The
	//! section form of `BootSaveVersion() >= PayloadSinceVersion(x)` (#2812).
	static bool BootHadSection(string name)
	{
		return s_aBoot && s_aBoot.Contains(name);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SaveSections's statics (LF_StaticReset finds it by reflection).
class LF_SaveSections_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SaveSections";
	}

	override void Apply()
	{
		LF_SaveSections.ResetStatics();
	}
}
