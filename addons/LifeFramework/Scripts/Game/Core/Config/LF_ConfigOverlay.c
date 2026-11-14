//------------------------------------------------------------------------------------------------
//! The one override model for content lists (#962). A list section (shop rules, catalogue items,
//! outlet offers, crimes) is a list of records with a stable id. Each layer above the core file
//! (each pack overlay in load order, then the owner's config.json, then on a client the server's
//! stream) names records by id and says what to do with each: add, replace, patch or remove.
//! `LF_ConfigOverlay` is the merge; a section only describes its list (`LF_ConfigList`).
//! docs/config/override-model.md has one example per verb.

//------------------------------------------------------------------------------------------------
//! Base of every record in a list section. `m_sOp` is the verb a pack overlay gives the entry; the
//! owner file writes it as "op". The core file leaves it empty.
[BaseContainerProps()]
class LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Overlay verb: add, replace, patch or remove the entry with this id. Empty patches an id that exists and adds one that does not. Ignored in a core file")]
	string m_sOp;
}

//------------------------------------------------------------------------------------------------
//! One list section as the merge sees it. A section subclasses it once per list and says how to
//! find, make, patch and check its records; the verbs, the id checks and the pack conflict
//! warning live in LF_ConfigOverlay only.
class LF_ConfigList
{
	//! "section.list", for log lines (catalogue.items).
	string GetName()
	{
		return "list";
	}

	int Count()
	{
		return 0;
	}

	LF_ConfigRecord Get(int index)
	{
		return null;
	}

	//! The record's id.
	string IdOf(notnull LF_ConfigRecord record)
	{
		return string.Empty;
	}

	//! The id a pack entry carries, written the way IdOf writes it.
	string IdOfContainer(notnull BaseContainer container)
	{
		return string.Empty;
	}

	//! An owner key in the form IdOf returns ("type:64" is "type:64,mode:2"), or empty when the key
	//! cannot be an id of this list (IdHint says what it should look like).
	string CanonicalId(string key)
	{
		return key;
	}

	string IdHint()
	{
		return "a non-empty id";
	}

	//! A record with this id and the shipped defaults, for an owner add or replace.
	LF_ConfigRecord NewRecord(string id)
	{
		return null;
	}

	//! Puts the record at index; index == Count() appends.
	void Set(int index, notnull LF_ConfigRecord record)
	{
	}

	void RemoveAt(int index)
	{
	}

	//! Copies the fields a pack entry writes directly (IsVariableSetDirectly). \return Fields set.
	int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		return 0;
	}

	//! Reads the fields an owner record gives. Warns its unknown keys (its key list holds "op").
	void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
	}

	//! Why a new or replaced record cannot stand (a new item needs a prefab), or empty.
	string Incomplete(notnull LF_ConfigRecord record)
	{
		return string.Empty;
	}

	//! Why the record at index may not be removed (a police rank a save points at by index, a zone
	//! rule every zone names), or empty when a remove is fine.
	string RemoveRefusal(int index)
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The index of the record with this id, or -1.
	int IndexOf(string id)
	{
		int count = Count();
		for (int i = 0; i < count; i++)
		{
			LF_ConfigRecord record = Get(i);
			if (record && IdOf(record) == id)
				return i;
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Which pack last changed each id during one layering pass. Two packs on one id: the later pack
//! wins and a warning names both. The owner layer passes no claims; overriding packs is its job.
class LF_ConfigClaims
{
	protected ref map<string, string> m_mSources = new map<string, string>();
	protected int m_iConflicts;

	//------------------------------------------------------------------------------------------------
	//! Records that `source` changed `id` of `list`. \return The other pack that changed it first, or empty.
	string Claim(string list, string id, string source)
	{
		string key = list + "|" + id;
		string previous;
		if (m_mSources.Find(key, previous) && previous != source)
		{
			m_mSources.Set(key, source);
			m_iConflicts++;
			return previous;
		}

		m_mSources.Set(key, source);
		return string.Empty;
	}

	int GetConflictCount()
	{
		return m_iConflicts;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ConfigOverlay
{
	static const string OP_ADD = "add";
	static const string OP_REPLACE = "replace";
	static const string OP_PATCH = "patch";
	static const string OP_REMOVE = "remove";

	//------------------------------------------------------------------------------------------------
	//! The pack overlays of one core file whose root class is `type`, loaded, in load order. A file
	//! that does not resolve, or the core file listed as its own overlay, is logged and skipped; a
	//! file of another class belongs to another section and is skipped quietly.
	//! \param[out] resources The loaded overlays (keep them while their containers are read).
	//! \param[out] names Their resource names, the source each log line names.
	static void LoadPackOverlays(ResourceName coreConf, typename type, string section, notnull array<ref Resource> resources, notnull array<ResourceName> names)
	{
		array<ResourceName> overlays = {};
		LF_ConfigService.GetPackOverlays(coreConf, overlays);
		foreach (ResourceName overlay : overlays)
		{
			Resource resource = BaseContainerTools.LoadContainer(overlay);
			if (!resource || !resource.IsValid() || !resource.GetResource())
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1: pack overlay does not resolve, skipped: %2", section, overlay));
				continue;
			}

			BaseContainer container = resource.GetResource().ToBaseContainer();
			typename overlayType;
			if (container)
				overlayType = container.GetClassName().ToType();
			if (!overlayType || !overlayType.IsInherited(type))
				continue;

			if (overlay == coreConf)
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1: the pack lists the core file as an overlay, skipped: %2", section, overlay));
				continue;
			}

			resources.Insert(resource);
			names.Insert(overlay);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Merges one list of a pack overlay. \return Entries changed.
	//! \param[in] root The overlay's root container.
	//! \param[in] field The list member (m_aItems).
	//! \param[in] source The overlay file, for log lines and claims.
	static int ApplyContainerList(notnull LF_ConfigList list, notnull BaseContainer root, string field, string source, LF_ConfigClaims claims)
	{
		BaseContainerList entries = root.GetObjectArray(field);
		if (!entries)
			return 0;

		int changed;
		for (int i = 0; i < entries.Count(); i++)
		{
			BaseContainer entry = entries.Get(i);
			if (!entry)
				continue;

			string op;
			entry.Get("m_sOp", op);
			string id = list.IdOfContainer(entry);
			if (Apply(list, id, op, source, claims, entry, null))
				changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges one keyed-records object of an owner section (or the stream): `key` holds records
	//! keyed by id, each with an optional "op". \return Records applied.
	static int ApplyRecords(notnull LF_ConfigReader reader, string key, notnull LF_ConfigList list, string source, LF_ConfigClaims claims = null)
	{
		int count = reader.BeginRecords(key);
		int applied;
		for (int i = 0; i < count; i++)
		{
			string recordKey = reader.GetRecordKey(i);
			if (!reader.BeginRecord(recordKey))
				continue;

			string op;
			bool opRead = !reader.Has("op") || reader.ReadString("op", op);
			string id = list.CanonicalId(recordKey);
			if (!opRead)
				reader.RejectRecord(string.Format("op is not a string (%1)", source));
			else if (id.IsEmpty())
				reader.RejectRecord(string.Format("the key is not %1 (%2)", list.IdHint(), source));
			else if (Apply(list, id, op, source, claims, null, reader))
				applied++;

			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! The merge: one entry of one layer, from a pack container or an owner reader (the other null).
	//! - add: a new id; an id the list has is refused.
	//! - replace: the entry is rebuilt from this layer alone, in its place in the list.
	//! - patch: only the fields given change.
	//! - remove: the entry leaves the list; a later layer may add it again.
	//! - no verb: patch an id the list has, add one it lacks (the model before #962).
	//! replace, patch and remove of an id no lower layer has are refused with the source and id; the
	//! rest of the overlay still applies. \return True when the entry was applied.
	static bool Apply(notnull LF_ConfigList list, string id, string op, string source, LF_ConfigClaims claims, BaseContainer container, LF_ConfigReader reader)
	{
		if (id.IsEmpty())
			return Refuse(list, id, source, reader, "the entry has no id");

		if (!op.IsEmpty() && op != OP_ADD && op != OP_REPLACE && op != OP_PATCH && op != OP_REMOVE)
			return Refuse(list, id, source, reader, string.Format("op \"%1\" is not add, replace, patch or remove", op));

		int index = list.IndexOf(id);
		if (op.IsEmpty())
		{
			if (index >= 0)
				op = OP_PATCH;
			else
				op = OP_ADD;
		}
		else if (op == OP_ADD && index >= 0)
		{
			return Refuse(list, id, source, reader, "add names an id the list already has; use patch or replace");
		}
		else if (op != OP_ADD && index < 0)
		{
			return Refuse(list, id, source, reader, string.Format("%1 names an id no lower layer has", op));
		}

		if (op == OP_REMOVE)
		{
			string kept = list.RemoveRefusal(index);
			if (!kept.IsEmpty())
				return Refuse(list, id, source, reader, kept);

			list.RemoveAt(index);
		}
		else if (op == OP_PATCH)
		{
			LF_ConfigRecord current = list.Get(index);
			if (container)
				list.PatchFromContainer(current, container);
			else
				list.PatchFromReader(current, reader);
		}
		else
		{
			LF_ConfigRecord record;
			if (container)
			{
				record = LF_ConfigRecord.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
			}
			else
			{
				record = list.NewRecord(id);
				if (record)
					list.PatchFromReader(record, reader);
			}

			if (!record)
				return Refuse(list, id, source, reader, "the entry does not build");

			string missing = list.Incomplete(record);
			if (!missing.IsEmpty())
				return Refuse(list, id, source, reader, missing);

			record.m_sOp = string.Empty;
			if (index < 0)
				index = list.Count();
			list.Set(index, record);
		}

		if (claims)
		{
			string other = claims.Claim(list.GetName(), id, source);
			if (!other.IsEmpty())
				LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("%1 %2: pack %3 changes an id pack %4 already changed; %3 wins (later in load order)", list.GetName(), id, source, other));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Refuse(LF_ConfigList list, string id, string source, LF_ConfigReader reader, string reason)
	{
		if (reader)
		{
			reader.RejectRecord(string.Format("%1 (%2)", reason, source));
			return false;
		}

		LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("%1: %2 %3: %4; entry skipped", source, list.GetName(), id, reason));
		return false;
	}
}
