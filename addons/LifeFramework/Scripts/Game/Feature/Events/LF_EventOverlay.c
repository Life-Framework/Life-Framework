//------------------------------------------------------------------------------------------------
//! The events list as the shared override model sees it (LF_ConfigOverlay, #962). A pack's event
//! config (root class LF_EventsConfig, listed in the mission header's `m_aLF_ConfigOverlays`)
//! names an event by id and gives it a verb in `m_sOp`:
//! - add: a new event (a pack's own convoy). An id the core already has is refused.
//! - replace: the event is rebuilt from the pack entry alone, in its place in the list.
//! - patch: only what the entry sets changes. The cooldown and m_bEnabled if set; each listed phase is matched by
//!   its NAME, patched field by field when the event has it (an unset field keeps the core value)
//!   and appended when it does not. A phase is never removed by a patch; use replace for that.
//! - remove: the event is disabled (it leaves the list; the shell never starts it).
//! No verb patches an id the list has and adds one it lacks. replace, patch and remove of an id
//! no lower layer has are refused and logged, the rest of the overlay applies.
class LF_EventDefList : LF_ConfigList
{
	protected array<ref LF_EventDef> m_aRecords;

	void LF_EventDefList(notnull array<ref LF_EventDef> records)
	{
		m_aRecords = records;
	}

	override string GetName()
	{
		return "events.events";
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
		return LF_EventDef.Cast(record).m_sEventId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sEventId", id);
		return id;
	}

	override string IdHint()
	{
		return "an event id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_EventDef def = new LF_EventDef();
		def.m_sEventId = id;
		def.m_aPhases = {};
		def.m_fCooldownSeconds = 600;
		return def;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_EventDef def = LF_EventDef.Cast(record);
		if (index < m_aRecords.Count())
			m_aRecords.Set(index, def);
		else
			m_aRecords.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_aRecords.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_EventPhaseRules.Validate(LF_EventDef.Cast(record));
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_EventDef def = LF_EventDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_fCooldownSeconds"))
		{
			container.Get("m_fCooldownSeconds", def.m_fCooldownSeconds);
			changed++;
		}

		if (container.IsVariableSetDirectly("m_bEnabled"))
		{
			container.Get("m_bEnabled", def.m_bEnabled);
			changed++;
		}

		BaseContainerList phases = container.GetObjectArray("m_aPhases");
		if (!phases)
			return changed;

		if (!def.m_aPhases)
			def.m_aPhases = new array<ref LF_EventPhase>();

		for (int i = 0; i < phases.Count(); i++)
		{
			BaseContainer phaseContainer = phases.Get(i);
			if (!phaseContainer)
				continue;

			string name;
			phaseContainer.Get("m_sName", name);
			int index = def.FindPhase(name);
			if (index < 0)
			{
				LF_EventPhase added = LF_EventPhase.Cast(BaseContainerTools.CreateInstanceFromContainer(phaseContainer));
				if (added)
				{
					def.m_aPhases.Insert(added);
					changed++;
				}
				continue;
			}

			changed += PatchPhase(def.m_aPhases[index], phaseContainer);
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies the fields a pack phase entry sets onto the core phase of the same name.
	protected static int PatchPhase(notnull LF_EventPhase phase, notnull BaseContainer container)
	{
		int changed;
		if (container.IsVariableSetDirectly("m_fCadenceSeconds"))
		{
			container.Get("m_fCadenceSeconds", phase.m_fCadenceSeconds);
			changed++;
		}

		if (container.IsVariableSetDirectly("m_fIdleTimeoutSeconds"))
		{
			container.Get("m_fIdleTimeoutSeconds", phase.m_fIdleTimeoutSeconds);
			changed++;
		}

		if (container.IsVariableSetDirectly("m_aModules"))
		{
			array<string> modules = {};
			container.Get("m_aModules", modules);
			phase.m_aModules = new array<string>();
			phase.m_aModules.Copy(modules);
			changed++;
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! The event list every system reads: the core file Configs/Events/LF_Events.conf with each pack
//! overlay over it in load order (#1952). An event that fails LF_EventPhaseRules.Validate after
//! the overlays (a patch that left a negative time, say) is logged `[LFDebug:Events]` and left
//! out; the rest still load. Server and client both read it; nothing here is replicated.
class LF_EventOverlay
{
	static const string FEATURE = "Events";
	static const ResourceName CONFIG_PATH = "{6ACA05ED8116280F}Configs/Events/LF_Events.conf";

	protected static ref LF_EventsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The core file with every pack overlay over it, or null (logged once) when it does not resolve.
	static LF_EventsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("event config does not resolve, no event can run: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_EventsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("event config has an unexpected root class, no event can run: %1", CONFIG_PATH));
			return null;
		}

		if (!s_Config.m_aEvents)
			s_Config.m_aEvents = new array<ref LF_EventDef>();

		ApplyPackOverlays(s_Config);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers each pack overlay over `config`, in load order.
	static void ApplyPackOverlays(notnull LF_EventsConfig config)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_EventsConfig, "events", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			ApplyOverlay(config, resource.GetResource().ToBaseContainer(), names[i], claims);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One overlay container over `config`. \return Events changed.
	static int ApplyOverlay(notnull LF_EventsConfig config, notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!config.m_aEvents)
			config.m_aEvents = new array<ref LF_EventDef>();

		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_EventDefList(config.m_aEvents), root, "m_aEvents", source, claims);
		int modules = MergeModules(config, root, source);
		LF_Debug.Info(FEATURE, string.Format("events: pack overlay %1 changed %2 event(s) and %3 module(s)", source, changed, modules));
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds an overlay's module refs (#2137). A ref whose id a lower layer has replaces it (logged).
	//! \return How many refs the overlay brought.
	static int MergeModules(notnull LF_EventsConfig config, notnull BaseContainer root, string source)
	{
		BaseContainerList list = root.GetObjectArray("m_aModules");
		if (!list)
			return 0;

		if (!config.m_aModules)
			config.m_aModules = new array<ref LF_EventModuleRef>();

		int merged;
		for (int i = 0; i < list.Count(); i++)
		{
			BaseContainer entry = list.Get(i);
			if (!entry)
				continue;

			LF_EventModuleRef added = LF_EventModuleRef.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
			if (!added || added.m_sId.IsEmpty())
			{
				LF_Debug.Error(FEATURE, string.Format("events: pack overlay %1 has a module entry with no id; skipped", source));
				continue;
			}

			merged++;
			int existing = FindModule(config, added.m_sId);
			if (existing < 0)
			{
				config.m_aModules.Insert(added);
				continue;
			}

			LF_Debug.Warn(FEATURE, string.Format("events: pack overlay %1 replaces module %2", source, added.m_sId));
			config.m_aModules.Set(existing, added);
		}

		return merged;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the index of the module ref with this id, or -1.
	static int FindModule(notnull LF_EventsConfig config, string id)
	{
		if (!config.m_aModules)
			return -1;

		foreach (int i, LF_EventModuleRef entry : config.m_aModules)
		{
			if (entry && entry.m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The events that can run, in file order. A bad one is logged once and left out.
	//! \return How many events were skipped.
	static int Collect(notnull LF_EventsConfig config, notnull array<LF_EventDef> outEvents)
	{
		outEvents.Clear();
		if (!config.m_aEvents)
			return 0;

		int skipped;
		foreach (LF_EventDef def : config.m_aEvents)
		{
			string reason = LF_EventPhaseRules.Validate(def);
			if (!reason.IsEmpty())
			{
				skipped++;
				LF_Debug.Error(FEATURE, string.Format("event skipped: %1", reason));
				continue;
			}

			outEvents.Insert(def);
		}

		return skipped;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The event with this id after the overlays, or null (unknown or disabled by a pack).
	static LF_EventDef Find(string eventId)
	{
		LF_EventsConfig config = Get();
		if (!config || !config.m_aEvents)
			return null;

		foreach (LF_EventDef def : config.m_aEvents)
		{
			if (def && def.m_sEventId == eventId && LF_EventPhaseRules.Validate(def).IsEmpty())
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_EventsConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets the loaded list; the next Get reads the files again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventOverlay's statics (registered at script load; Core/LF_StaticReset.c).
class LF_EventOverlay_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventOverlay_StaticResetHook());

	override string Owner()
	{
		return "LF_EventOverlay";
	}

	override void Apply()
	{
		LF_EventOverlay.ResetStatics();
	}
}
