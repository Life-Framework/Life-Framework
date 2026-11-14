//------------------------------------------------------------------------------------------------
//! Every system can be turned on or off from config (#960). A system is a Feature/<Area> folder or a
//! named part of one (Robbery in Crime, Pager in Comms), listed once in
//! Configs/Systems/LF_SystemCatalog.conf with how it may be switched, the system it is part of, the
//! systems it needs and the items only it uses.
//!
//! On or off is a config section like any other: the catalog's default (on unless opt-in), then the
//! pack's LF_SystemsConfig overlays from the mission header, then the owner's `systems` section in
//! config.json. A client takes the server's answer from the config stream.
//!
//! One read path: LF_Systems.IsEnabled("<id>"), checked at a system's entry points. A system that
//! is off hides its actions, refuses its server requests with a logged reason (Refuses), spawns
//! nothing and leaves out its loadout items (IsItemAllowed). It logs once when it goes off. Its
//! saved data is never touched: load and save run as before, so turning it on again restores it.
//!
//! A system can also turn itself off at run time (SwitchOff), for example when the map has none of
//! the places it needs (#1047). That logs once and holds until the world ends.
//------------------------------------------------------------------------------------------------
enum LF_ESystemSwitch
{
	NOT_YET,	//!< Its entry points do not check LF_Systems yet; a config that turns it off is refused.
	SWITCHABLE,	//!< Its entry points check LF_Systems; config may turn it off.
	LOCKED		//!< Framework plumbing the rest stands on; always on.
}

//------------------------------------------------------------------------------------------------
//! One system in the catalog.
[BaseContainerProps()]
class LF_SystemDef
{
	[Attribute("", UIWidgets.EditBox, "System id: a Feature/<Area> folder name, or a named part of one (Robbery in Crime)")]
	string m_sId;

	[Attribute("0", UIWidgets.ComboBox, "Whether config may turn it off. NOT_YET: its entry points do not check the switch yet. LOCKED: framework plumbing", "", ParamEnumArray.FromEnum(LF_ESystemSwitch))]
	LF_ESystemSwitch m_eSwitch;

	[Attribute("0", UIWidgets.CheckBox, "Opt-in: off unless a pack or the owner turns it on")]
	bool m_bOptIn;

	[Attribute("", UIWidgets.EditBox, "The system this one is part of. It is off whenever that one is off")]
	string m_sParent;

	[Attribute("", UIWidgets.Auto, "Systems this one needs. A config that turns one of these off while this one is on is refused at boot")]
	ref array<string> m_aRequires;

	[Attribute("", UIWidgets.ResourceNamePicker, "Item prefabs only this system uses. A loadout leaves them out while the system is off", "et")]
	ref array<ResourceName> m_aItems;

	[Attribute("", UIWidgets.Auto, "LF_Places kinds this system works at. At game start, with none of them placed on the map, it switches itself off for that world (#1047)")]
	ref array<string> m_aPlaces;
}

//------------------------------------------------------------------------------------------------
//! The list of systems: what the game can switch. Core only; a pack changes on or off through
//! LF_SystemsConfig, never this list.
[BaseContainerProps(configRoot: true)]
class LF_SystemCatalog
{
	static const ResourceName CONFIG_PATH = "{B274391A95CF3D72}Configs/Systems/LF_SystemCatalog.conf";

	[Attribute("", UIWidgets.Object, "Every system, one entry each. validate-systems-list fails a Feature/<Area> folder with no entry")]
	ref array<ref LF_SystemDef> m_aSystems;

	//------------------------------------------------------------------------------------------------
	//! \return The entry for an id, or null.
	LF_SystemDef Find(string id)
	{
		if (!m_aSystems || id.IsEmpty())
			return null;

		foreach (LF_SystemDef def : m_aSystems)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests build a catalog in script.
	LF_SystemDef Add(string id, LF_ESystemSwitch switchKind, string parent = string.Empty, bool optIn = false)
	{
		if (!m_aSystems)
			m_aSystems = new array<ref LF_SystemDef>();

		LF_SystemDef def = new LF_SystemDef();
		def.m_sId = id;
		def.m_eSwitch = switchKind;
		def.m_sParent = parent;
		def.m_bOptIn = optIn;
		def.m_aRequires = {};
		def.m_aItems = {};
		m_aSystems.Insert(def);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The shipped catalog, or null when the file does not load (every system then stays on).
	static LF_SystemCatalog LoadShipped()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error(LF_Systems.FEATURE, string.Format("the system catalog does not load, every system stays on: %1", CONFIG_PATH));
			return null;
		}

		LF_SystemCatalog catalog = LF_SystemCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!catalog)
			LF_Debug.Error(LF_Systems.FEATURE, string.Format("the system catalog has an unexpected root class, every system stays on: %1", CONFIG_PATH));

		return catalog;
	}
}

//------------------------------------------------------------------------------------------------
//! One system's on or off, set by a pack overlay or the owner. The record key is the system id.
[BaseContainerProps()]
class LF_SystemSwitch
{
	[Attribute("", UIWidgets.EditBox, "System id, as Configs/Systems/LF_SystemCatalog.conf lists it")]
	string m_sId;

	[Attribute("1", UIWidgets.CheckBox, "On. Off hides the system's actions, refuses its requests and spawns nothing; its saved data is kept")]
	bool m_bEnabled;

	//! Which layer set it, for the log ("pack <file>", "owner config"). Not an attribute.
	string m_sSource;
}

//------------------------------------------------------------------------------------------------
//! The `systems` section: which systems this server runs. A pack ships a `.conf` of this class in
//! its mission header's overlay list; the owner writes `systems.switches.<id>.enabled` in
//! config.json. A system with no switch keeps the catalog's default.
[BaseContainerProps(configRoot: true)]
class LF_SystemsConfig
{
	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "systems";

	[Attribute("", UIWidgets.Object, "On or off per system, keyed by system id. Turning a system off keeps its saved data, so turning it on again restores it")]
	ref array<ref LF_SystemSwitch> m_aSwitches;

	//------------------------------------------------------------------------------------------------
	LF_SystemSwitch FindSwitch(string id)
	{
		if (!m_aSwitches)
			return null;

		foreach (LF_SystemSwitch entry : m_aSwitches)
		{
			if (entry && entry.m_sId == id)
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one system; a later layer replaces an earlier one's value for the same id.
	void Set(string id, bool enabled, string source)
	{
		LF_SystemSwitch entry = FindSwitch(id);
		if (!entry)
		{
			if (!m_aSwitches)
				m_aSwitches = new array<ref LF_SystemSwitch>();

			entry = new LF_SystemSwitch();
			entry.m_sId = id;
			m_aSwitches.Insert(entry);
		}

		entry.m_bEnabled = enabled;
		entry.m_sSource = source;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack's overlays of this class, in the mission header's order. Overlays of other
	//! classes in the same list belong to other sections and are skipped.
	void ApplyPackOverlays()
	{
		array<ResourceName> overlays = {};
		LF_ConfigService.GetPackOverlays(LF_SystemCatalog.CONFIG_PATH, overlays);
		foreach (ResourceName overlay : overlays)
		{
			Resource resource = BaseContainerTools.LoadContainer(overlay);
			if (!resource || !resource.IsValid() || !resource.GetResource())
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("systems: pack overlay does not resolve, skipped: %1", overlay));
				continue;
			}

			BaseContainer container = resource.GetResource().ToBaseContainer();
			typename overlayType;
			if (container)
				overlayType = container.GetClassName().ToType();
			if (!overlayType || !overlayType.IsInherited(LF_SystemsConfig))
				continue;

			LF_SystemsConfig pack = LF_SystemsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
			if (!pack || !pack.m_aSwitches)
				continue;

			foreach (LF_SystemSwitch entry : pack.m_aSwitches)
			{
				if (entry && !entry.m_sId.IsEmpty())
					Set(entry.m_sId, entry.m_bEnabled, "pack " + overlay);
			}

			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("systems: pack overlay %1 set %2 switches", overlay, pack.m_aSwitches.Count()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "systems" section: `switches` is an object keyed by system id, each with
	//! `enabled`. An id the catalog does not list is refused; whether the system may be turned off
	//! is decided with the other layers (LF_Systems.Decide).
	//! \param[in] reader A reader on the section.
	//! \param[in] catalog The catalog the ids are checked against; null accepts any id.
	void ApplyOwner(notnull LF_ConfigReader reader, LF_SystemCatalog catalog)
	{
		array<string> ownerKeys = {"switches"};
		reader.WarnUnknownKeys(ownerKeys);

		int count = reader.BeginRecords("switches");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			if (catalog && !catalog.Find(key))
			{
				reader.RejectRecord("the key is not a system id (Configs/Systems/LF_SystemCatalog.conf lists them)");
				reader.EndRecord();
				continue;
			}

			array<string> recordKeys = {"enabled"};
			reader.WarnUnknownKeys(recordKeys);
			bool enabled = true;
			if (reader.ReadBool("enabled", enabled))
				Set(key, enabled, "owner config");

			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}
}

//------------------------------------------------------------------------------------------------
//! The switch. Server and client hold the same answer: the server decides from the layers, the
//! client takes the server's effective values from the config stream.
class LF_Systems
{
	static const string FEATURE = "Systems";
	//! The stream's record list (one `enabled` per system), the same shape as the owner section.
	static const string KEY_SWITCHES = "switches";
	//! A parent or requirement chain longer than this is a loop in the catalog; it reads as on.
	protected static const int MAX_DEPTH = 8;	// lf-fixed: a loop guard, not tuning

	protected static ref LF_SystemCatalog s_Catalog;
	//! Each system's own on or off after the layers and the dependency check.
	protected static ref map<string, bool> s_mOn;
	//! Why a system's own switch is off ("owner config", "pack <file>", "opt-in", ...).
	protected static ref map<string, string> s_mReason;
	//! Systems that turned themselves off at run time, with the reason (SwitchOff). Kept on a reload.
	protected static ref map<string, string> s_mRuntimeOff;
	//! Item prefab GUID -> the system that owns it.
	protected static ref map<string, string> s_mItemOwner;
	protected static ref set<string> s_aUnknownLogged;
	protected static bool s_bResolved;

	//------------------------------------------------------------------------------------------------
	//! The boot hook (LF_GameModeRoleplay.OnGameStart, after LF_ConfigService.Boot). Resolves once;
	//! a feature that asked earlier already resolved with the same owner file.
	static void Boot()
	{
		if (!s_bResolved)
			Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves every system again from the shipped catalog and the current layers: the server after
	//! /configreload, the client after a config stream, tests after they change the owner layer.
	static void Reload()
	{
		LF_SystemCatalog catalog = LF_SystemCatalog.LoadShipped();
		LF_SystemsConfig layers = new LF_SystemsConfig();
		layers.ApplyPackOverlays();

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_SystemsConfig.SECTION);
		if (reader)
		{
			layers.ApplyOwner(reader, catalog);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("systems: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		// Client: the server's effective values replace the result (#285). Null on the server.
		LF_ConfigReader streamed = LF_ConfigStream.OpenReceivedSection(LF_SystemsConfig.SECTION);
		if (streamed)
		{
			ApplyStream(streamed, catalog, layers);
			Install(catalog, layers, "the server");
			return;
		}

		Install(catalog, layers, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Decides and installs one catalog and its layers. Tests call it with a catalog built in script;
	//! Reload() puts the shipped one back. Logs one line per system that is off.
	//! \param[in] catalog The catalog; null means every system is on.
	//! \param[in] layers The merged pack and owner switches; null means the catalog's defaults.
	//! \param[in] trusted Non-empty when the layers are the server's effective values (a client):
	//! they are taken as they are, with no switchability or dependency check.
	static void Install(LF_SystemCatalog catalog, LF_SystemsConfig layers, string trusted = string.Empty)
	{
		s_Catalog = catalog;
		s_mOn = new map<string, bool>();
		s_mReason = new map<string, string>();
		s_mItemOwner = new map<string, string>();
		s_aUnknownLogged = new set<string>();
		if (!s_mRuntimeOff)
			s_mRuntimeOff = new map<string, string>();
		s_bResolved = true;

		if (!catalog || !catalog.m_aSystems)
			return;

		if (trusted.IsEmpty())
		{
			Decide(catalog, layers, s_mOn, s_mReason);
		}
		else
		{
			foreach (LF_SystemDef def : catalog.m_aSystems)
			{
				if (!def)
					continue;

				bool on = !def.m_bOptIn;
				LF_SystemSwitch entry;
				if (layers)
					entry = layers.FindSwitch(def.m_sId);
				if (entry)
					on = entry.m_bEnabled;
				s_mOn.Set(def.m_sId, on);
				if (!on)
					s_mReason.Set(def.m_sId, trusted);
			}
		}

		int off;
		string offNames;
		foreach (LF_SystemDef def : catalog.m_aSystems)
		{
			if (!def)
				continue;

			if (def.m_aItems)
			{
				foreach (ResourceName item : def.m_aItems)
				{
					string itemKey = ItemKey(item);
					if (!itemKey.IsEmpty())
						s_mItemOwner.Set(itemKey, def.m_sId);
				}
			}

			if (IsOn(def.m_sId, 0))
				continue;

			off++;
			if (!offNames.IsEmpty())
				offNames += ", ";
			offNames += def.m_sId;
			LF_Debug.Info(def.m_sId, string.Format("%1 is off (%2): its actions are hidden, its requests are refused, it spawns nothing; its saved data is kept", def.m_sId, WhyOff(def.m_sId)));
		}

		if (off == 0)
			LF_Debug.Info(FEATURE, string.Format("systems: all %1 on", catalog.m_aSystems.Count()));
		else
			LF_Debug.Info(FEATURE, string.Format("systems: %1 of %2 on; off: %3", catalog.m_aSystems.Count() - off, catalog.m_aSystems.Count(), offNames));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: each system's own on or off from the catalog's defaults and the layers, then the
	//! dependency check. A switch the catalog forbids (LOCKED, NOT_YET) or an unknown id is refused
	//! with a logged reason. A config that turns off a system an enabled one needs is refused and the
	//! needed system goes back to its shipped value (#279); when that value is off (opt-in), the
	//! system that needs it is turned off instead.
	//! \param[out] outOn Each system's own on or off.
	//! \param[out] outReason Why each system that is off is off.
	static void Decide(notnull LF_SystemCatalog catalog, LF_SystemsConfig layers, notnull map<string, bool> outOn, notnull map<string, string> outReason)
	{
		outOn.Clear();
		outReason.Clear();
		if (!catalog.m_aSystems)
			return;

		foreach (LF_SystemDef def : catalog.m_aSystems)
		{
			if (!def || def.m_sId.IsEmpty())
				continue;

			outOn.Set(def.m_sId, !def.m_bOptIn);
			if (def.m_bOptIn)
				outReason.Set(def.m_sId, "opt-in; nothing turned it on");
		}

		if (layers && layers.m_aSwitches)
		{
			foreach (LF_SystemSwitch entry : layers.m_aSwitches)
			{
				if (!entry)
					continue;

				LF_SystemDef def = catalog.Find(entry.m_sId);
				if (!def)
				{
					LF_Debug.Error(FEATURE, string.Format("%1 sets systems.switches.%2, which is not a system; ignored", entry.m_sSource, entry.m_sId));
					continue;
				}

				if (!entry.m_bEnabled && def.m_eSwitch == LF_ESystemSwitch.LOCKED)
				{
					LF_Debug.Error(FEATURE, string.Format("%1 turns %2 off, but %2 is part of the framework and cannot be turned off; it stays on", entry.m_sSource, entry.m_sId));
					continue;
				}

				if (!entry.m_bEnabled && def.m_eSwitch == LF_ESystemSwitch.NOT_YET)
				{
					LF_Debug.Error(FEATURE, string.Format("%1 turns %2 off, but %2 has no off switch yet (its entry points do not check it, #960); it stays on", entry.m_sSource, entry.m_sId));
					continue;
				}

				outOn.Set(entry.m_sId, entry.m_bEnabled);
				if (entry.m_bEnabled)
					outReason.Remove(entry.m_sId);
				else
					outReason.Set(entry.m_sId, entry.m_sSource);
			}
		}

		// Dependencies. Each pass fixes at least one system, so the passes are bounded by the count.
		for (int pass = 0; pass <= catalog.m_aSystems.Count(); pass++)
		{
			bool changed;
			foreach (LF_SystemDef def : catalog.m_aSystems)
			{
				if (!def || !def.m_aRequires || !ChainOn(catalog, outOn, def.m_sId, 0))
					continue;

				foreach (string needed : def.m_aRequires)
				{
					if (ChainOn(catalog, outOn, needed, 0))
						continue;

					string culprit = OffInChain(catalog, outOn, needed);
					LF_SystemDef culpritDef = catalog.Find(culprit);
					if (!culpritDef)
					{
						LF_Debug.Error(FEATURE, string.Format("%1 needs %2, which is not a system; the requirement is ignored", def.m_sId, needed));
						continue;
					}

					if (!culpritDef.m_bOptIn)
					{
						LF_Debug.Error(FEATURE, string.Format("%1 turns %2 off, but %3 is on and needs %4; %2 stays on (the shipped value)", outReason.Get(culprit), culprit, def.m_sId, needed));
						outOn.Set(culprit, true);
						outReason.Remove(culprit);
					}
					else
					{
						LF_Debug.Error(FEATURE, string.Format("%1 needs %2, which is off (opt-in); %1 is off too", def.m_sId, needed));
						outOn.Set(def.m_sId, false);
						outReason.Set(def.m_sId, string.Format("needs %1, which is off", needed));
					}

					changed = true;
					break;
				}
			}

			if (!changed)
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a system's own switch and its parents' are on (requirements not counted).
	protected static bool ChainOn(notnull LF_SystemCatalog catalog, notnull map<string, bool> on, string id, int depth)
	{
		if (depth > MAX_DEPTH)
			return true;

		LF_SystemDef def = catalog.Find(id);
		if (!def)
			return false;

		if (!on.Get(id))
			return false;

		if (def.m_sParent.IsEmpty())
			return true;

		return ChainOn(catalog, on, def.m_sParent, depth + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the system in the parent chain of id whose own switch is off (id itself first).
	protected static string OffInChain(notnull LF_SystemCatalog catalog, notnull map<string, bool> on, string id)
	{
		string current = id;
		for (int depth = 0; depth <= MAX_DEPTH; depth++)
		{
			LF_SystemDef def = catalog.Find(current);
			if (!def)
				return current;

			if (!on.Get(current))
				return current;

			if (def.m_sParent.IsEmpty())
				return current;

			current = def.m_sParent;
		}

		return current;
	}

	//------------------------------------------------------------------------------------------------
	//! The one read path. True while the system, its parents and what it needs are on. An id the
	//! catalog does not list logs one error and reads as on (validate-systems-list keeps them known).
	static bool IsEnabled(string id)
	{
		EnsureResolved();
		return IsOn(id, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsOn(string id, int depth)
	{
		if (depth > MAX_DEPTH || !s_Catalog)
			return true;

		LF_SystemDef def = s_Catalog.Find(id);
		if (!def)
		{
			if (s_aUnknownLogged && !s_aUnknownLogged.Contains(id))
			{
				s_aUnknownLogged.Insert(id);
				LF_Debug.Error(FEATURE, string.Format("%1 is not in the system catalog; it reads as on", id));
			}
			return true;
		}

		if (!s_mOn.Get(id) || s_mRuntimeOff.Contains(id))
			return false;

		if (!def.m_sParent.IsEmpty() && !IsOn(def.m_sParent, depth + 1))
			return false;

		if (def.m_aRequires)
		{
			foreach (string needed : def.m_aRequires)
			{
				if (!IsOn(needed, depth + 1))
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a system reads as off, in words for the log; empty while it is on.
	static string WhyOff(string id)
	{
		EnsureResolved();
		if (IsOn(id, 0) || !s_Catalog)
			return string.Empty;

		if (s_mRuntimeOff.Contains(id))
			return s_mRuntimeOff.Get(id);

		if (!s_mOn.Get(id))
		{
			string reason = s_mReason.Get(id);
			if (reason.IsEmpty())
				reason = "config";
			return reason;
		}

		LF_SystemDef def = s_Catalog.Find(id);
		if (def && !def.m_sParent.IsEmpty() && !IsOn(def.m_sParent, 0))
			return string.Format("part of %1, which is off", def.m_sParent);

		if (def && def.m_aRequires)
		{
			foreach (string needed : def.m_aRequires)
			{
				if (!IsOn(needed, 0))
					return string.Format("needs %1, which is off", needed);
			}
		}

		return "off";
	}

	//------------------------------------------------------------------------------------------------
	//! Server entry points (an RPC handler, an action's server path): true when the system is off,
	//! with one log line naming what was refused and why. The caller then does nothing.
	//! \param[in] what What was asked, for the log ("weapon robbery", "plant at farm_plot_1").
	static bool Refuses(string id, string what)
	{
		if (IsEnabled(id))
			return false;

		LF_Debug.Warn(id, string.Format("%1 refused: %2 is off on this server (%3)", what, id, WhyOff(id)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A loadout asks before it gives an item: false when the item belongs to a system that is off.
	static bool IsItemAllowed(ResourceName prefab)
	{
		EnsureResolved();
		if (!s_mItemOwner)
			return true;

		string owner = s_mItemOwner.Get(ItemKey(prefab));
		if (owner.IsEmpty())
			return true;

		return IsOn(owner, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The GUID of a `{GUID}path` resource name, upper case, so a moved or re-spelled path still matches.
	protected static string ItemKey(ResourceName prefab)
	{
		return LF_Utils.PrefabGuid(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at run time: a system turns itself off, for example when the map has no place for it
	//! (#1047). Logs once ("<reason>: off"), holds until the world ends or ClearRuntimeOff, and
	//! reaches clients through the config stream. A LOCKED system cannot be switched off.
	//! \return True when the system is now off.
	static bool SwitchOff(string id, string reason)
	{
		// A client takes the server's answer from the stream; a client-side off would never clear.
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, string.Format("%1 asked to switch off on a client (%2); only the server decides", id, reason));
			return false;
		}

		EnsureResolved();
		LF_SystemDef def;
		if (s_Catalog)
			def = s_Catalog.Find(id);
		if (!def)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 asked to switch off, but it is not in the system catalog", id));
			return false;
		}

		if (def.m_eSwitch == LF_ESystemSwitch.LOCKED)
		{
			LF_Debug.Error(id, string.Format("%1 is part of the framework and cannot switch off (%2); it stays on", id, reason));
			return false;
		}

		if (s_mRuntimeOff.Contains(id))
			return true;

		bool wasOn = IsOn(id, 0);
		s_mRuntimeOff.Set(id, reason);
		if (wasOn)
			LF_Debug.Info(id, string.Format("%1: off", reason));

		PublishIfLive();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at game start (LF_GameModeRoleplay.OnGameStart, #1047): a system whose catalog entry
	//! lists places (m_aPlaces, LF_Places kinds) and finds none of them on this map switches itself
	//! off for this world, with one log line ("no JailCell placed on this map: off"). Placed
	//! components register in OnPostInit, before the game mode starts. A pack for another map places
	//! the components and the system stays on with no script change.
	//! \return How many systems this call switched off.
	static int CheckPlaces()
	{
		if (!Replication.IsServer())
			return 0;

		EnsureResolved();
		if (!s_Catalog || !s_Catalog.m_aSystems)
			return 0;

		int switched;
		array<IEntity> found = {};
		foreach (LF_SystemDef def : s_Catalog.m_aSystems)
		{
			if (!def || !def.m_aPlaces || def.m_aPlaces.IsEmpty() || s_mRuntimeOff.Contains(def.m_sId))
				continue;

			bool placed;
			string kinds;
			foreach (string kind : def.m_aPlaces)
			{
				found.Clear();
				if (LF_Places.FindAll(kind, found) > 0)
				{
					placed = true;
					break;
				}

				if (!kinds.IsEmpty())
					kinds += " or ";
				kinds += kind;
			}

			if (placed)
				continue;

			if (SwitchOff(def.m_sId, string.Format("no %1 placed on this map", kinds)))
				switched++;
		}

		return switched;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the run-time offs (tests; world end goes through ResetStatics).
	//! \param[in] id One system's run-time off; empty drops them all.
	static void ClearRuntimeOff(string id = string.Empty)
	{
		if (id.IsEmpty() || !s_mRuntimeOff)
			s_mRuntimeOff = new map<string, string>();
		else
			s_mRuntimeOff.Remove(id);

		PublishIfLive();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: when the stream already went out, send the new answer to every client.
	protected static void PublishIfLive()
	{
		if (!Replication.IsServer() || !LF_ConfigStream.IsPublished() || !GetGame())
			return;

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_PublishConfigStream();
	}

	//------------------------------------------------------------------------------------------------
	//! Writes every system's effective on or off into the config stream: run-time offs, parents and
	//! requirements included, so the client needs no layers of its own.
	//! \param[in] ctx A save context with this section's object open.
	static void WriteStream(notnull SCR_JsonSaveContext ctx)
	{
		EnsureResolved();
		ctx.StartObject(KEY_SWITCHES);
		if (s_Catalog && s_Catalog.m_aSystems)
		{
			foreach (LF_SystemDef def : s_Catalog.m_aSystems)
			{
				if (!def || def.m_sId.IsEmpty())
					continue;

				ctx.StartObject(def.m_sId);
				ctx.WriteValue("enabled", IsOn(def.m_sId, 0));
				ctx.EndObject();
			}
		}
		ctx.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's effective values replace the layers. An id this build does not know is
	//! skipped (an older client); a bad value keeps the client's own.
	protected static void ApplyStream(notnull LF_ConfigReader reader, LF_SystemCatalog catalog, notnull LF_SystemsConfig layers)
	{
		int count = reader.BeginRecords(KEY_SWITCHES);
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			bool enabled = true;
			if ((!catalog || catalog.Find(key)) && reader.ReadBool("enabled", enabled))
				layers.Set(key, enabled, "the server");

			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureResolved()
	{
		if (!s_bResolved)
			Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_Catalog = null;
		s_mOn = null;
		s_mReason = null;
		s_mRuntimeOff = null;
		s_mItemOwner = null;
		s_aUnknownLogged = null;
		s_bResolved = false;
	}
}
