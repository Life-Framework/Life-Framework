// LF_NpcDresserPlugin.c - World Editor: dress the selected NPCs from an outfit preset or a garment per slot.
//
// Workbench-only (Scripts/WorkbenchGame/ never compiles into the game). The maintainer selects NPCs, runs
// Plugins > Life Framework > LF NPC Dresser (Ctrl+Shift+D), picks a preset (an LF_NpcOutfitPreset .conf under
// Configs/NpcOutfits/) and/or a clothing prefab per slot, and presses Dress. Each NPC's
// BaseLoadoutManagerComponent slot gets its Prefab overridden in the layer through WorldEditorAPI.SetVariableValue
// inside one BeginEntityAction/EndEntityAction, so it is one Ctrl+Z. "Back to prefab" clears those overrides.
// User guide: docs/tooling/npc-dresser.md.
//
// tools\cli npc dress runs the same code head-less (RunCommandline) from $profile:lf-npc-dresser/plan.txt:
//   world <resource path>          open this world first
//   select <entity name>           add a named entity to the working set
//   preset <conf>                  the outfit preset (same as the dialog's preset picker)
//   slot <head|torso|legs|feet|vest|backpack> <prefab>   a per-slot pick (wins over the preset)
//   dress | undress                apply to the working set (one undo step)
//   expect <entity name> <slot> <prefab>      the slot's resolved Prefab now (a path suffix is enough)
//   snapshot <entity name>         remember every slot's Prefab now
//   expect-snapshot <entity name>  every slot is back to the snapshot (after an undo)
//   undo | redo                    Edit > Undo / Redo
//   save                           save the world (its layers)
// status.txt gets one line per step and a final `done <failed>`.

[WorkbenchPluginAttribute(name: "LF NPC Dresser", description: "Dress the selected NPCs: an outfit preset or a garment per slot, written into the layer as one undoable edit", shortcut: "Ctrl+Shift+D", wbModules: { "WorldEditor" }, category: "Life Framework", awesomeFontCode: 0xF553)]
class LF_NpcDresserPlugin : WorldEditorPlugin
{
	protected static const string TITLE = "LF NPC Dresser";
	protected static const string DIR = "$profile:lf-npc-dresser";

	[Attribute("", UIWidgets.ResourceNamePicker, "Outfit preset: Configs/NpcOutfits/ in the core (framework uniforms) or the pack (1989 Everon locals). A slot picked below wins over it.", "conf class=LF_NpcOutfitPreset", category: "Outfit")]
	ResourceName m_Preset;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Head: hat, cap or helmet (Prefabs/Characters/HeadGear)", "et", category: "Per slot")]
	ResourceName m_Head;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Torso: jacket, shirt or coat (Prefabs/Characters/Uniforms)", "et", category: "Per slot")]
	ResourceName m_Torso;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Legs: trousers (Prefabs/Characters/Uniforms)", "et", category: "Per slot")]
	ResourceName m_Legs;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Feet: shoes or boots (Prefabs/Characters/Footwear)", "et", category: "Per slot")]
	ResourceName m_Feet;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vest (Prefabs/Characters/Vests)", "et", category: "Per slot")]
	ResourceName m_Vest;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Backpack (Prefabs/Characters/Backpacks)", "et", category: "Per slot")]
	ResourceName m_Backpack;

	[Attribute("0", UIWidgets.CheckBox, "Clear the per-slot picks after dressing, so the next NPC starts from the preset alone", category: "Per slot")]
	bool m_bClearPicks;

	protected ref array<string> m_aStatus = {};
	protected int m_iFailed;

	//------------------------------------------------------------------------------------------------
	//! The slot keys in dialog order
	protected static array<string> SlotKeys()
	{
		return { "head", "torso", "legs", "feet", "vest", "backpack" };
	}

	//------------------------------------------------------------------------------------------------
	//! The preset field for a slot key
	protected static string PresetField(string key)
	{
		switch (key)
		{
			case "head": return "m_Head";
			case "torso": return "m_Torso";
			case "legs": return "m_Legs";
			case "feet": return "m_Feet";
			case "vest": return "m_Vest";
			case "backpack": return "m_Backpack";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! This dialog's own pick for a slot key
	protected ResourceName Pick(string key)
	{
		switch (key)
		{
			case "head": return m_Head;
			case "torso": return m_Torso;
			case "legs": return m_Legs;
			case "feet": return m_Feet;
			case "vest": return m_Vest;
			case "backpack": return m_Backpack;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Slot names our NPCs and vanilla characters use for a key, and the slot's AreaType class
	protected static void SlotNames(string key, out array<string> names, out string area)
	{
		names = {};
		switch (key)
		{
			case "head": names = { "Hat", "Head", "HeadCover", "Headgear", "Helmet" }; area = "LoadoutHeadCoverArea"; break;
			case "torso": names = { "Jacket", "Torso", "Shirt" }; area = "LoadoutJacketArea"; break;
			case "legs": names = { "Pants", "Legs", "Trousers" }; area = "LoadoutPantsArea"; break;
			case "feet": names = { "Boots", "Feet", "Footwear", "Shoes" }; area = "LoadoutBootsArea"; break;
			case "vest": names = { "Vest" }; area = "LoadoutVestArea"; break;
			case "backpack": names = { "Back", "Backpack" }; area = "LoadoutBackpackArea"; break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Index of the BaseLoadoutManagerComponent in an entity source, or -1
	protected static int FindLoadout(IEntitySource src)
	{
		if (!src)
			return -1;

		for (int i = 0, n = src.GetComponentCount(); i < n; i++)
		{
			IEntityComponentSource comp = src.GetComponent(i);
			if (!comp)
				continue;

			string cls = comp.GetClassName();
			if (cls == "BaseLoadoutManagerComponent")
				return i;

			typename t = cls.ToType();
			if (t && t.IsInherited(BaseLoadoutManagerComponent))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Index of the slot for `key` in a loadout's Slots list (by name, then AreaType, then the feet slot class), or -1
	protected static int FindSlot(BaseContainerList slots, string key)
	{
		if (!slots)
			return -1;

		array<string> names;
		string area;
		SlotNames(key, names, area);
		int n = slots.Count();
		for (int j = 0; j < n; j++)
		{
			BaseContainer slot = slots.Get(j);
			if (slot && names.Contains(slot.GetName()))
				return j;
		}

		for (int j = 0; j < n; j++)
		{
			BaseContainer slot = slots.Get(j);
			if (!slot)
				continue;

			BaseContainer areaType = slot.GetObject("AreaType");
			if (areaType && areaType.GetClassName() == area)
				return j;

			if (key == "feet" && slot.GetClassName() == "SCR_FeetLoadoutSlotInfo")
				return j;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab is a garment: it (or an ancestor) has a BaseLoadoutClothComponent
	protected static bool IsWearable(ResourceName prefab)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return false;

		BaseResourceObject obj = res.GetResource();
		if (!obj)
			return false;

		IEntitySource src = obj.ToEntitySource();
		while (src)
		{
			for (int i = 0, n = src.GetComponentCount(); i < n; i++)
			{
				IEntityComponentSource comp = src.GetComponent(i);
				if (!comp)
					continue;

				string cls = comp.GetClassName();
				if (cls.Contains("LoadoutClothComponent"))
					return true;

				typename t = cls.ToType();
				if (t && t.IsInherited(BaseLoadoutClothComponent))
					return true;
			}

			src = IEntitySource.Cast(src.GetAncestor());
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The outfit to put on: the preset's slots, then this dialog's picks over them. False and `problem` on a bad input.
	protected bool BuildOutfit(ResourceName preset, map<string, ResourceName> picks, notnull map<string, ResourceName> outfit, out string problem)
	{
		if (!preset.IsEmpty())
		{
			Resource res = BaseContainerTools.LoadContainer(preset);
			BaseContainer conf;
			if (res && res.IsValid() && res.GetResource())
				conf = res.GetResource().ToBaseContainer();

			if (!conf)
			{
				problem = "The preset does not load: " + preset;
				return false;
			}

			foreach (string key : SlotKeys())
			{
				ResourceName value;
				conf.Get(PresetField(key), value);
				if (!value.IsEmpty())
					outfit.Set(key, value);
			}
		}

		foreach (string key, ResourceName value : picks)
		{
			if (!value.IsEmpty())
				outfit.Set(key, value);
		}

		foreach (string key, ResourceName value : outfit)
		{
			if (!IsWearable(value))
				problem += string.Format("%1: %2 is not clothing (no BaseLoadoutClothComponent)\n", key, value);
		}

		return problem.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the outfit into one NPC's loadout slots (or clears the overrides when `undress`). Returns the slots touched.
	protected int DressOne(WorldEditorAPI api, IEntitySource src, map<string, ResourceName> outfit, bool undress, out string note)
	{
		int comp = FindLoadout(src);
		if (comp < 0)
		{
			note = "no BaseLoadoutManagerComponent";
			return 0;
		}

		BaseContainerList slots = src.GetComponent(comp).GetObjectArray("Slots");
		int touched;
		foreach (string key : SlotKeys())
		{
			ResourceName prefab;
			bool wanted = outfit.Find(key, prefab);
			if (!undress && !wanted)
				continue;

			int idx = FindSlot(slots, key);
			if (idx < 0)
			{
				if (wanted)
					note += string.Format(" no %1 slot;", key);

				continue;
			}

			array<ref ContainerIdPathEntry> path = { new ContainerIdPathEntry("components", comp), new ContainerIdPathEntry("Slots", idx) };
			if (undress)
			{
				BaseContainer slot = slots.Get(idx);
				if (slot.IsVariableSetDirectly("Prefab") && api.ClearVariableValue(src, path, "Prefab"))
					touched++;

				if (slot.IsVariableSetDirectly("Enabled"))
					api.ClearVariableValue(src, path, "Enabled");

				continue;
			}

			if (!api.SetVariableValue(src, path, "Prefab", prefab))
			{
				note += string.Format(" %1 write failed;", key);
				continue;
			}

			// A slot switched off in the prefab (Npc_Default_01's Hat) would hold the garment without showing it.
			if (slots.Get(idx).GetVarIndex("Enabled") >= 0)
				api.SetVariableValue(src, path, "Enabled", "1");

			touched++;
		}

		return touched;
	}

	//------------------------------------------------------------------------------------------------
	//! One undo step over every NPC; returns the report line
	protected string Apply(WorldEditorAPI api, array<IEntitySource> npcs, map<string, ResourceName> outfit, bool undress)
	{
		string verb = "dress";
		if (undress)
			verb = "back to prefab";

		bool own = api.BeginEntityAction(TITLE + ": " + verb);
		int done;
		string report;
		foreach (IEntitySource src : npcs)
		{
			string note;
			int n = DressOne(api, src, outfit, undress, note);
			if (n > 0)
				done++;

			report += string.Format("%1: %2 slot(s)%3\n", api.GetEntityNiceName(src), n, note);
		}

		if (own)
			api.EndEntityAction(TITLE + ": " + verb);

		return string.Format("%1 %2 of %3 NPC(s)\n%4", verb, done, npcs.Count(), report);
	}

	//------------------------------------------------------------------------------------------------
	protected map<string, ResourceName> DialogPicks()
	{
		map<string, ResourceName> picks = new map<string, ResourceName>();
		foreach (string key : SlotKeys())
		{
			picks.Set(key, Pick(key));
		}

		return picks;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
			return;

		WorldEditorAPI api = worldEditor.GetApi();
		if (!api)
			return;

		array<IEntitySource> npcs = {};
		string names;
		for (int i = 0, n = api.GetSelectedEntitiesCount(); i < n; i++)
		{
			IEntitySource src = api.GetSelectedEntity(i);
			if (FindLoadout(src) < 0)
				continue;

			npcs.Insert(src);
			if (npcs.Count() <= 4)
				names += " " + api.GetEntityNiceName(src);
		}

		if (npcs.IsEmpty())
		{
			Workbench.Dialog(TITLE, "Select one or more NPCs in the World Editor first (characters with a BaseLoadoutManagerComponent), then run the dresser again.");
			return;
		}

		if (npcs.Count() > 4)
			names += " ...";

		string text = string.Format("%1 NPC(s) selected:%2\n\nPick an outfit preset, a garment per slot, or both: a slot picked here wins over the preset, and a slot left empty keeps what the NPC wears. Dress is one Ctrl+Z.", npcs.Count(), names);
		int choice = Workbench.ScriptDialog(TITLE, text, this);
		if (choice == 0)
			return;

		map<string, ResourceName> outfit = new map<string, ResourceName>();
		bool undress = choice == 2;
		if (!undress)
		{
			string problem;
			if (!BuildOutfit(m_Preset, DialogPicks(), outfit, problem))
			{
				Workbench.Dialog(TITLE, "Nothing was changed.\n\n" + problem);
				return;
			}

			if (outfit.IsEmpty())
			{
				Workbench.Dialog(TITLE, "Nothing to put on: pick a preset or at least one slot.");
				return;
			}
		}

		string report = Apply(api, npcs, outfit, undress);
		Print("[LFNpcDresser] " + report, LogLevel.NORMAL);
		if (!undress && m_bClearPicks)
		{
			m_Head = "";
			m_Torso = "";
			m_Legs = "";
			m_Feet = "";
			m_Vest = "";
			m_Backpack = "";
		}
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Dress", true)]
	int ButtonDress()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Back to prefab")]
	int ButtonUndress()
	{
		return 2;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Cancel")]
	int ButtonCancel()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	// Head-less (tools\cli npc dress): the plan file drives the same Apply the dialog uses.
	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		RunPlan();
		Note(string.Format("done %1", m_iFailed));
		int code = 0;
		if (m_iFailed != 0)
			code = 1;

		Workbench.Exit(code);
	}

	//------------------------------------------------------------------------------------------------
	protected void Note(string line)
	{
		m_aStatus.Insert(line);
		Print("[LFNpcDresser] " + line, LogLevel.NORMAL);
		FileHandle f = FileIO.OpenFile(DIR + "/status.txt", FileMode.WRITE);
		if (!f)
			return;

		foreach (string saved : m_aStatus)
		{
			f.WriteLine(saved);
		}

		f.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string reason)
	{
		m_iFailed++;
		Note("error " + reason);
	}

	//------------------------------------------------------------------------------------------------
	//! The slot's resolved Prefab on a named entity; "?" when it cannot be read
	protected string SlotValue(WorldEditorAPI api, string entity, string key)
	{
		IEntitySource src = api.FindEntityByName(entity);
		int comp = FindLoadout(src);
		if (comp < 0)
			return "?";

		BaseContainerList slots = src.GetComponent(comp).GetObjectArray("Slots");
		int idx = FindSlot(slots, key);
		if (idx < 0)
			return "?";

		ResourceName value;
		slots.Get(idx).Get("Prefab", value);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPlan()
	{
		FileHandle plan = FileIO.OpenFile(DIR + "/plan.txt", FileMode.READ);
		if (!plan)
		{
			Fail("no plan " + DIR + "/plan.txt");
			return;
		}

		array<string> lines = {};
		string line;
		while (plan.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (!line.IsEmpty() && !line.StartsWith("#"))
				lines.Insert(line);
		}

		plan.Close();

		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		WorldEditorAPI api;
		if (worldEditor)
			api = worldEditor.GetApi();

		array<IEntitySource> npcs = {};
		map<string, ResourceName> picks = new map<string, ResourceName>();
		map<string, string> snapshot = new map<string, string>();
		ResourceName preset;
		foreach (string l : lines)
		{
			array<string> p = {};
			l.Split(" ", p, true);
			string verb = p[0];
			if (verb == "world" && p.Count() > 1)
			{
				if (!worldEditor || !worldEditor.SetOpenedResource(p[1]))
				{
					Fail("world_not_opened " + p[1]);
					return;
				}

				api = worldEditor.GetApi();
				string opened;
				if (api)
					api.GetWorldPath(opened);

				Note("ok world " + opened);
				continue;
			}

			if (!api)
			{
				Fail("no_world_api");
				return;
			}

			if (verb == "select" && p.Count() > 1)
			{
				IEntitySource src = api.FindEntityByName(p[1]);
				if (!src || FindLoadout(src) < 0)
				{
					Fail("not_an_npc " + p[1]);
					continue;
				}

				npcs.Insert(src);
				Note("ok select " + p[1]);
			}
			else if (verb == "preset" && p.Count() > 1)
			{
				preset = p[1];
				Note("ok preset " + preset);
			}
			else if (verb == "slot" && p.Count() > 2)
			{
				picks.Set(p[1], p[2]);
				Note("ok slot " + p[1] + " " + p[2]);
			}
			else if (verb == "dress" || verb == "undress")
			{
				map<string, ResourceName> outfit = new map<string, ResourceName>();
				string problem;
				if (verb == "dress" && !BuildOutfit(preset, picks, outfit, problem))
				{
					Fail("outfit " + problem);
					continue;
				}

				string report = Apply(api, npcs, outfit, verb == "undress");
				report.Replace("\n", " | ");
				Note("ok " + verb + " " + report);
			}
			else if (verb == "expect" && p.Count() > 3)
			{
				string got = SlotValue(api, p[1], p[2]);
				string want = p[3];
				if (got == want || got.Contains(want))
					Note(string.Format("ok expect %1 %2 = %3", p[1], p[2], got));
				else
					Fail(string.Format("expect %1 %2 wanted %3 got %4", p[1], p[2], want, got));
			}
			else if (verb == "snapshot" && p.Count() > 1)
			{
				foreach (string key : SlotKeys())
				{
					snapshot.Set(p[1] + " " + key, SlotValue(api, p[1], key));
				}

				Note("ok snapshot " + p[1]);
			}
			else if (verb == "expect-snapshot" && p.Count() > 1)
			{
				foreach (string key : SlotKeys())
				{
					string was = snapshot.Get(p[1] + " " + key);
					string now = SlotValue(api, p[1], key);
					if (now == was)
						Note(string.Format("ok expect-snapshot %1 %2 = %3", p[1], key, now));
					else
						Fail(string.Format("expect-snapshot %1 %2 was %3 now %4", p[1], key, was, now));
				}
			}
			else if (verb == "undo" || verb == "redo")
			{
				string item = "Undo";
				if (verb == "redo")
					item = "Redo";

				if (worldEditor.ExecuteAction({ "Edit", item }))
					Note("ok " + verb);
				else
					Fail(verb + " returned false");
			}
			else if (verb == "save")
			{
				if (worldEditor.Save())
					Note("ok save");
				else
					Fail("save returned false");
			}
			else
			{
				Fail("bad line: " + l);
			}
		}
	}
}
