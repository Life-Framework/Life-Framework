// LF_HoldTunerPlugin.c - World Editor: tune how a hand-carried item sits in the hand, live, and save it to the prefab.
//
// Workbench-only (Scripts/WorkbenchGame/ never compiles into the game). User guide: docs/tooling/hold-tuner.md.
//
// What drives a hold (docs/asset-pipeline/states.md "Hand"): the item is attached with its origin and axes on the
// character's LeftHandProp bone. The anim set the item borrows (ItemAnimationAttributes > AnimationAttachments,
// bound as "Gadget") poses the arm AND moves that bone; HandAdditiveTranslation (metres, in the bone frame) and
// HandAdditiveRotation (degrees a b c; the item's axes as rows in the bone frame are Rz(c) * Rx(a) * Ry(b)) move
// the item off the bone. There is no grip socket or hand IK target on the item. The tuner edits those two values.
//
// The World Editor runs no animation, so the preview puts the hand where the hold pose puts it, measured in a
// session with the harness verb `heldxf` (POSES below): the item lands where it will be in play relative to the
// body. The preview character's arm stays in the editor's rest pose. For an anim set not in POSES the tuner uses
// the preview character's live LeftHandProp bone (the rest pose) and says so.
//
// Two entry points share LF_HoldTuner:
//   Plugins > Life Framework > Hold Tuner (Ctrl+Shift+H): pick the item, preview, save to prefab, remove preview.
//   The "LF Hold Tuner" World Editor tool (toolbar): the live panel; nudge buttons and keys move the preview at once.
//
// Head-less (RunCommandline: a Workbench started with -plugin=LF_HoldTunerPlugin, as look.mjs runWorkbench does;
// no cli command wraps it yet) reads $profile:lf-hold-tuner/plan.txt:
//   world <resource path>         open this world first
//   item <{GUID}prefab>           preview this item (reads its current values)
//   expect-offset|expect-rotation <x> <y> <z>   the tuner's values now (1 mm / 0.1 degree)
//   offset|rotation <x> <y> <z>   set the values and move the preview
//   nudge <offset|rotation> <0|1|2> <amount>   one step on one axis, as the panel buttons do
//   expect-hand <x> <y> <z> <tolerance>          the preview item's origin in the preview character's frame
//   save                          write the values into the prefab (one undo step) and save the .et
//   expect-prefab <offset|rotation> <x> <y> <z> the values the prefab holds now
//   undo | redo                   Edit > Undo / Redo
//   clear                         remove the preview entities
// status.txt gets one line per step and a final `done <failed>`.

//! The tuner's state and every operation the plugin, the tool and the head-less plan share.
class LF_HoldTuner
{
	static const string TITLE = "LF Hold Tuner";
	static const string ITEM_NAME = "LF_HoldTuner_Item";
	static const string CHARACTER_NAME = "LF_HoldTuner_Character";
	//! A plain vanilla civilian: a player prefab would run game components in the editor world.
	static const ResourceName DEFAULT_CHARACTER = "{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et";

	static ResourceName s_Item;
	static ResourceName s_Character = DEFAULT_CHARACTER;
	static vector s_vOffset;
	static vector s_vRotation;
	//! The hand frame's source: "measured <pose>" or "rest pose bone"
	static string s_sFrame;

	//------------------------------------------------------------------------------------------------
	static WorldEditorAPI Api()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
			return null;

		return worldEditor.GetApi();
	}

	//------------------------------------------------------------------------------------------------
	//! The hold pose's LeftHandProp in the character's model frame (x right, y up, z forward), measured with
	//! `heldxf` standing still (2026-11-16, lf-cloud, build 26266485). False for an anim set not measured yet.
	static bool PoseFrame(string graph, out string pose, out vector mat[4])
	{
		if (graph.Contains("Jerrycan.agr"))
		{
			// One hand down by the left side, as a jerrycan is carried (the parcel and the cash sack).
			pose = "jerrycan (one hand, by the side)";
			mat[0] = Vector(-0.9933, -0.0019, 0.1153);
			mat[1] = Vector(0.0023, 0.9993, 0.0366);
			mat[2] = Vector(-0.1153, 0.0367, -0.9927);
			mat[3] = Vector(-0.2302, 0.3307, 0.1463);
			return true;
		}

		if (graph.Contains("player_carry.agr"))
		{
			// The mod's carry set: the arm comes up across the chest, but the prop bone stays about a metre to
			// the left at shoulder height, away from the hand (why the cash sack floated a metre out).
			pose = "player_carry (the bone is not in the hand)";
			mat[0] = Vector(-0.0796, -0.9934, -0.0829);
			mat[1] = Vector(0.3312, -0.1047, 0.9377);
			mat[2] = Vector(-0.9402, 0.0472, 0.3373);
			mat[3] = Vector(-0.7030, 1.4519, 0.2551);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Rotation helpers: 3x3 matrices as three row vectors, the same convention as tools/assets/pipeline/states.mjs.
	//------------------------------------------------------------------------------------------------
	protected static void Mul(vector a[3], vector b[3], out vector res[3])
	{
		for (int i = 0; i < 3; i++)
		{
			vector row;
			for (int j = 0; j < 3; j++)
			{
				row[j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
			}

			res[i] = row;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rows of the item's axes in the bone frame for HandAdditiveRotation a b c: Rz(c) * Rx(a) * Ry(b)
	static void AdditiveMatrix(vector angles, out vector res[3])
	{
		float ca = Math.Cos(angles[0] * Math.DEG2RAD);
		float sa = Math.Sin(angles[0] * Math.DEG2RAD);
		float cb = Math.Cos(angles[1] * Math.DEG2RAD);
		float sb = Math.Sin(angles[1] * Math.DEG2RAD);
		float cc = Math.Cos(angles[2] * Math.DEG2RAD);
		float sc = Math.Sin(angles[2] * Math.DEG2RAD);
		vector rx[3];
		rx[0] = Vector(1, 0, 0);
		rx[1] = Vector(0, ca, -sa);
		rx[2] = Vector(0, sa, ca);
		vector ry[3];
		ry[0] = Vector(cb, 0, -sb);
		ry[1] = Vector(0, 1, 0);
		ry[2] = Vector(sb, 0, cb);
		vector rz[3];
		rz[0] = Vector(cc, -sc, 0);
		rz[1] = Vector(sc, cc, 0);
		rz[2] = Vector(0, 0, 1);
		vector xy[3];
		Mul(rx, ry, xy);
		Mul(rz, xy, res);
	}

	//------------------------------------------------------------------------------------------------
	//! HandAdditiveRotation whose AdditiveMatrix is m
	static vector AdditiveAngles(vector m[3])
	{
		float sa = Math.Clamp(m[2][1], -1, 1);
		float a = Math.Asin(sa);
		if (Math.AbsFloat(Math.Cos(a)) < 0.000001)
			return Vector(a * Math.RAD2DEG, Math.Atan2(-m[0][2], m[0][0]) * Math.RAD2DEG, 0);

		return Vector(a * Math.RAD2DEG, Math.Atan2(m[2][0], m[2][2]) * Math.RAD2DEG, Math.Atan2(-m[0][1], m[1][1]) * Math.RAD2DEG);
	}

	//------------------------------------------------------------------------------------------------
	//! The InventoryItemComponent index in an entity source, or -1
	static int FindItemComponent(IEntitySource src)
	{
		if (!src)
			return -1;

		for (int i = 0, n = src.GetComponentCount(); i < n; i++)
		{
			IEntityComponentSource comp = src.GetComponent(i);
			if (!comp)
				continue;

			string cls = comp.GetClassName();
			typename t = cls.ToType();
			if (cls == "InventoryItemComponent" || (t && t.IsInherited(InventoryItemComponent)))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The ItemAnimationAttributes container of an entity source, or null
	static BaseContainer AnimAttributes(IEntitySource src)
	{
		int comp = FindItemComponent(src);
		if (comp < 0)
			return null;

		BaseContainer attributes = src.GetComponent(comp).GetObject("Attributes");
		if (!attributes)
			return null;

		return attributes.GetObject("ItemAnimationAttributes");
	}

	//------------------------------------------------------------------------------------------------
	//! The anim graph the item borrows for the hold, or empty. Not named Graph: a call to that name inside this
	//! class fails the WorkbenchGame compile ("Too many parameters for 'Graph' method").
	static string HoldGraph(BaseContainer anim)
	{
		if (!anim)
			return string.Empty;

		BaseContainerList attachments = anim.GetObjectArray("AnimationAttachments");
		if (!attachments || attachments.Count() == 0)
			return string.Empty;

		ResourceName graph;
		attachments.Get(0).Get("AnimGraph", graph);
		return graph;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab's entity source (not an instance), or null
	static IEntitySource LoadPrefab(ResourceName prefab)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid() || !res.GetResource())
			return null;

		return res.GetResource().ToEntitySource();
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the item prefab's current hold values into the tuner. False and `problem` when it cannot be tuned.
	static bool Pick(ResourceName item, out string problem)
	{
		IEntitySource src = LoadPrefab(item);
		if (!src)
		{
			problem = "The prefab does not load: " + item;
			return false;
		}

		BaseContainer anim = AnimAttributes(src);
		if (!anim)
		{
			problem = "No InventoryItemComponent > Attributes > ItemAnimationAttributes on " + item + ": nothing drives a hold.";
			return false;
		}

		s_Item = item;
		anim.Get("HandAdditiveTranslation", s_vOffset);
		anim.Get("HandAdditiveRotation", s_vRotation);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The bone frame the item hangs from, in world space; `s_sFrame` says where it came from
	static bool HandFrame(WorldEditorAPI api, IEntitySource itemSrc, IEntitySource charSrc, out vector bone[4])
	{
		IEntity character = api.SourceToEntity(charSrc);
		if (!character)
			return false;

		vector body[4];
		character.GetWorldTransform(body);
		vector local[4];
		string pose;
		if (PoseFrame(HoldGraph(AnimAttributes(itemSrc)), pose, local))
		{
			s_sFrame = "measured " + pose;
		}
		else
		{
			Animation animation = character.GetAnimation();
			TNodeId id = -1;
			if (animation)
				id = animation.GetBoneIndex("LeftHandProp");

			if (id == -1 || !animation.GetBoneMatrix(id, local))
				return false;

			s_sFrame = "rest pose bone (anim set not measured: check the hold in play)";
		}

		Math3D.MatrixMultiply4(body, local, bone);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the preview item where the tuner's values put it; false when there is no preview
	static bool UpdatePreview()
	{
		WorldEditorAPI api = Api();
		if (!api)
			return false;

		IEntitySource itemSrc = api.FindEntityByName(ITEM_NAME);
		IEntitySource charSrc = api.FindEntityByName(CHARACTER_NAME);
		IEntity item = api.SourceToEntity(itemSrc);
		vector bone[4];
		if (!item || !HandFrame(api, itemSrc, charSrc, bone))
			return false;

		vector m[3];
		AdditiveMatrix(s_vRotation, m);
		vector additive[4];
		additive[0] = m[0];
		additive[1] = m[1];
		additive[2] = m[2];
		additive[3] = s_vOffset;
		vector world[4];
		Math3D.MatrixMultiply4(bone, additive, world);
		item.SetWorldTransform(world);
		item.Update();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the preview item's transform (after you moved it with the gizmo) back into the tuner's values
	static bool ReadFromGizmo()
	{
		WorldEditorAPI api = Api();
		if (!api)
			return false;

		IEntitySource itemSrc = api.FindEntityByName(ITEM_NAME);
		IEntitySource charSrc = api.FindEntityByName(CHARACTER_NAME);
		IEntity item = api.SourceToEntity(itemSrc);
		vector bone[4];
		if (!item || !HandFrame(api, itemSrc, charSrc, bone))
			return false;

		vector world[4];
		item.GetWorldTransform(world);
		vector local[4];
		Math3D.MatrixInvMultiply4(bone, world, local);
		vector m[3];
		m[0] = local[0];
		m[1] = local[1];
		m[2] = local[2];
		s_vOffset = Round(local[3], 1000);
		s_vRotation = Round(AdditiveAngles(m), 100);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static vector Round(vector v, float per)
	{
		return Vector(Math.Round(v[0] * per) / per, Math.Round(v[1] * per) / per, Math.Round(v[2] * per) / per);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns (or replaces) the preview character and item at `at`, facing +z, and frames the camera on the hand
	static bool StartPreview(vector at, out string problem)
	{
		WorldEditorAPI api = Api();
		if (!api || s_Item.IsEmpty())
		{
			problem = "Pick an item prefab first.";
			return false;
		}

		bool own = api.BeginEntityAction(TITLE + ": preview");
		ClearPreview(api);
		IEntitySource charSrc = api.CreateEntity(s_Character, CHARACTER_NAME, api.GetCurrentEntityLayerId(), null, at, vector.Zero);
		IEntitySource itemSrc = api.CreateEntity(s_Item, ITEM_NAME, api.GetCurrentEntityLayerId(), null, at, vector.Zero);
		if (own)
			api.EndEntityAction(TITLE + ": preview");

		if (!charSrc || !itemSrc)
		{
			problem = "Could not create the preview entities (is a layer active and unlocked?).";
			return false;
		}

		if (!UpdatePreview())
		{
			problem = "The preview character has no LeftHandProp bone: pick a human character prefab.";
			return false;
		}

		IEntity item = api.SourceToEntity(itemSrc);
		vector hand = item.GetOrigin();
		vector eye = at + Vector(-1.3, 1.4, 1.5);
		vector look = hand - eye;
		look.Normalize();
		api.SetCamera(eye, look);
		api.SetEntitySelection(itemSrc);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ClearPreview(WorldEditorAPI api)
	{
		array<string> names = { ITEM_NAME, CHARACTER_NAME };
		foreach (string name : names)
		{
			IEntitySource src = api.FindEntityByName(name);
			if (src)
				api.DeleteEntity(src);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string VecText(vector v)
	{
		return string.Format("%1 %2 %3", Num(v[0]), Num(v[1]), Num(v[2]));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Num(float f)
	{
		float r = Math.Round(f * 10000) / 10000;
		if (Math.AbsFloat(r) < 0.00005)
			r = 0;

		return r.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the tuner's values into the item prefab as one undo step and saves its .et
	static bool SaveToPrefab(out string problem)
	{
		WorldEditorAPI api = Api();
		IEntitySource prefab = LoadPrefab(s_Item);
		if (!api || !prefab)
		{
			problem = "No World Editor or the prefab does not load: " + s_Item;
			return false;
		}

		// Prefer the spawned preview's ancestor: it is the prefab source the World Editor tracks for undo.
		IEntitySource itemSrc = api.FindEntityByName(ITEM_NAME);
		if (itemSrc && itemSrc.GetAncestor())
			prefab = IEntitySource.Cast(itemSrc.GetAncestor());

		int comp = FindItemComponent(prefab);
		if (comp < 0)
		{
			problem = "No InventoryItemComponent on " + s_Item;
			return false;
		}

		array<ref ContainerIdPathEntry> path = { new ContainerIdPathEntry("components", comp), new ContainerIdPathEntry("Attributes"), new ContainerIdPathEntry("ItemAnimationAttributes") };
		bool own = api.BeginEntityAction(TITLE + ": save hold");
		bool ok = api.SetVariableValue(prefab, path, "HandAdditiveTranslation", VecText(s_vOffset));
		ok = api.SetVariableValue(prefab, path, "HandAdditiveRotation", VecText(s_vRotation)) && ok;
		if (ok)
			ok = api.SaveEntityTemplate(prefab);

		if (own)
			api.EndEntityAction(TITLE + ": save hold");

		if (!ok)
			problem = "The World Editor refused the write or the save of " + s_Item;

		UpdatePreview();
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! The values the prefab holds now (offset or rotation)
	static vector PrefabValue(string which)
	{
		vector v;
		BaseContainer anim = AnimAttributes(LoadPrefab(s_Item));
		WorldEditorAPI api = Api();
		IEntitySource itemSrc;
		if (api)
			itemSrc = api.FindEntityByName(ITEM_NAME);

		if (itemSrc && itemSrc.GetAncestor())
			anim = AnimAttributes(IEntitySource.Cast(itemSrc.GetAncestor()));

		if (!anim)
			return v;

		if (which == "rotation")
			anim.Get("HandAdditiveRotation", v);
		else
			anim.Get("HandAdditiveTranslation", v);

		return v;
	}

	//------------------------------------------------------------------------------------------------
	//! One step on one axis: what = "offset" (metres) or "rotation" (degrees)
	static void Nudge(string what, int axis, float amount)
	{
		if (what == "rotation")
			s_vRotation[axis] = s_vRotation[axis] + amount;
		else
			s_vOffset[axis] = s_vOffset[axis] + amount;

		UpdatePreview();
	}

	//------------------------------------------------------------------------------------------------
	static string Report()
	{
		return string.Format("%1\nHandAdditiveTranslation %2\nHandAdditiveRotation %3\nhand frame: %4", s_Item, VecText(s_vOffset), VecText(s_vRotation), s_sFrame);
	}
}

//! Plugins > Life Framework > Hold Tuner: pick the item, preview it held, save, clean up.
[WorkbenchPluginAttribute(name: "Hold Tuner", description: "Preview a hand-carried item in a character's hand, nudge its hold offset and rotation, and save them to the item prefab", shortcut: "Ctrl+Shift+H", wbModules: { "WorldEditor" }, category: "Life Framework", awesomeFontCode: 0xF256)]
class LF_HoldTunerPlugin : WorldEditorPlugin
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The carried item prefab (an item with InventoryItemComponent > ItemAnimationAttributes, e.g. Prefabs/Items/Roleplay/LF_MoneyBag.et)", "et", category: "Item")]
	ResourceName m_Item;

	[Attribute("{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et", UIWidgets.ResourcePickerThumbnail, "Who holds it in the preview (a human character prefab)", "et", category: "Item")]
	ResourceName m_Character;

	[Attribute("0 0 0", UIWidgets.Auto, "HandAdditiveTranslation: metres in the hand bone's frame. Filled from the prefab when you press Preview with a new item.", category: "Hold")]
	vector m_vOffset;

	[Attribute("0 0 0", UIWidgets.Auto, "HandAdditiveRotation: degrees a b c (item axes = Rz(c) * Rx(a) * Ry(b) in the hand bone's frame)", category: "Hold")]
	vector m_vRotation;

	protected ref array<string> m_aStatus = {};
	protected int m_iFailed;
	protected static const string DIR = "$profile:lf-hold-tuner";

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		WorldEditorAPI api = LF_HoldTuner.Api();
		if (!api)
			return;

		if (m_Item.IsEmpty())
			m_Item = LF_HoldTuner.s_Item;

		bool fresh = m_Item != LF_HoldTuner.s_Item;
		if (!fresh)
		{
			m_vOffset = LF_HoldTuner.s_vOffset;
			m_vRotation = LF_HoldTuner.s_vRotation;
		}

		string text = "Pick the carried item and press Preview: a character and the item appear in front of the camera, the item where the hold pose puts the hand.\n\nNudge it live with the \"LF Hold Tuner\" tool in the World Editor toolbar (buttons or the number pad), or move the item with the gizmo and press \"Read from gizmo\" there. Save to prefab writes HandAdditiveTranslation and HandAdditiveRotation into the item prefab as one Ctrl+Z.";
		if (!LF_HoldTuner.s_Item.IsEmpty())
			text += "\n\nNow: " + LF_HoldTuner.Report();

		int choice = Workbench.ScriptDialog(LF_HoldTuner.TITLE, text, this);
		if (choice == 0)
			return;

		if (choice == 3)
		{
			bool own = api.BeginEntityAction(LF_HoldTuner.TITLE + ": remove preview");
			LF_HoldTuner.ClearPreview(api);
			if (own)
				api.EndEntityAction(LF_HoldTuner.TITLE + ": remove preview");

			return;
		}

		string problem;
		if (m_Item != LF_HoldTuner.s_Item || api.FindEntityByName(LF_HoldTuner.ITEM_NAME) == null)
		{
			if (!LF_HoldTuner.Pick(m_Item, problem))
			{
				Workbench.Dialog(LF_HoldTuner.TITLE, problem);
				return;
			}

			if (!m_Character.IsEmpty())
				LF_HoldTuner.s_Character = m_Character;

			if (!fresh)
			{
				LF_HoldTuner.s_vOffset = m_vOffset;
				LF_HoldTuner.s_vRotation = m_vRotation;
			}

			if (!LF_HoldTuner.StartPreview(ViewCentre(api), problem))
			{
				Workbench.Dialog(LF_HoldTuner.TITLE, problem);
				return;
			}
		}
		else
		{
			LF_HoldTuner.s_vOffset = m_vOffset;
			LF_HoldTuner.s_vRotation = m_vRotation;
			LF_HoldTuner.UpdatePreview();
		}

		if (choice == 2)
		{
			if (!LF_HoldTuner.SaveToPrefab(problem))
				Workbench.Dialog(LF_HoldTuner.TITLE, problem);
		}

		Print("[LFHoldTuner] " + LF_HoldTuner.Report(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The ground point in the middle of the World Editor view, or the origin when nothing is hit
	protected vector ViewCentre(WorldEditorAPI api)
	{
		vector start, end, dir;
		if (api.TraceWorldPos(api.GetScreenWidth() / 2, api.GetScreenHeight() / 2, TraceFlags.WORLD | TraceFlags.ENTS, start, end, dir))
			return end;

		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Preview", true)]
	int ButtonPreview()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Save to prefab")]
	int ButtonSave()
	{
		return 2;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Remove preview")]
	int ButtonRemove()
	{
		return 3;
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Close")]
	int ButtonClose()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	// Head-less (-plugin=LF_HoldTunerPlugin): the plan file drives the same LF_HoldTuner calls the dialog uses.
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
		Print("[LFHoldTuner] " + line, LogLevel.NORMAL);
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
	protected void Expect(string what, vector got, vector want, float tolerance)
	{
		if (vector.Distance(got, want) <= tolerance)
			Note(string.Format("ok %1 = %2", what, LF_HoldTuner.VecText(got)));
		else
			Fail(string.Format("%1 wanted %2 got %3", what, LF_HoldTuner.VecText(want), LF_HoldTuner.VecText(got)));
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string reason)
	{
		m_iFailed++;
		Note("error " + reason);
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
		foreach (string l : lines)
		{
			array<string> p = {};
			l.Split(" ", p, true);
			string verb = p[0];
			vector v;
			if (p.Count() > 3)
				v = Vector(p[p.Count() - 3].ToFloat(), p[p.Count() - 2].ToFloat(), p[p.Count() - 1].ToFloat());

			string problem;
			if (verb == "world" && p.Count() > 1)
			{
				if (worldEditor && worldEditor.SetOpenedResource(p[1]))
					Note("ok world " + p[1]);
				else
					Fail("world_not_opened " + p[1]);
			}
			else if (verb == "item" && p.Count() > 1)
			{
				WorldEditorAPI api = LF_HoldTuner.Api();
				vector at;
				if (api)
					at = Vector(300, api.GetTerrainSurfaceY(300, 205), 205);

				if (LF_HoldTuner.Pick(p[1], problem) && LF_HoldTuner.StartPreview(at, problem))
					Note("ok item " + LF_HoldTuner.Report());
				else
					Fail("item " + problem);
			}
			else if (verb == "expect-offset" && p.Count() > 3)
			{
				Expect("offset", LF_HoldTuner.s_vOffset, v, 0.001);
			}
			else if (verb == "expect-rotation" && p.Count() > 3)
			{
				Expect("rotation", LF_HoldTuner.s_vRotation, v, 0.1);
			}
			else if ((verb == "offset" || verb == "rotation") && p.Count() > 3)
			{
				if (verb == "offset")
					LF_HoldTuner.s_vOffset = v;
				else
					LF_HoldTuner.s_vRotation = v;

				if (LF_HoldTuner.UpdatePreview())
					Note("ok " + l);
				else
					Fail("no preview for " + l);
			}
			else if (verb == "nudge" && p.Count() > 3)
			{
				LF_HoldTuner.Nudge(p[1], p[2].ToInt(), p[3].ToFloat());
				Note("ok " + l + " -> " + LF_HoldTuner.VecText(LF_HoldTuner.s_vOffset) + " / " + LF_HoldTuner.VecText(LF_HoldTuner.s_vRotation));
			}
			else if (verb == "expect-hand" && p.Count() > 4)
			{
				ExpectHand(Vector(p[1].ToFloat(), p[2].ToFloat(), p[3].ToFloat()), p[4].ToFloat());
			}
			else if (verb == "save")
			{
				if (LF_HoldTuner.SaveToPrefab(problem))
					Note("ok save " + LF_HoldTuner.Report());
				else
					Fail("save " + problem);
			}
			else if (verb == "expect-prefab" && p.Count() > 4)
			{
				float tolerance = 0.001;
				if (p[1] == "rotation")
					tolerance = 0.1;

				Expect("prefab " + p[1], LF_HoldTuner.PrefabValue(p[1]), v, tolerance);
			}
			else if (verb == "undo" || verb == "redo")
			{
				string menu = "Undo";
				if (verb == "redo")
					menu = "Redo";

				if (worldEditor && worldEditor.ExecuteAction({ "Edit", menu }))
					Note("ok " + verb);
				else
					Fail(verb + " returned false");
			}
			else if (verb == "clear")
			{
				WorldEditorAPI clearApi = LF_HoldTuner.Api();
				if (clearApi)
					LF_HoldTuner.ClearPreview(clearApi);

				Note("ok clear");
			}
			else
			{
				Fail("bad line: " + l);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The preview item's origin in the preview character's frame, against a wanted point
	protected void ExpectHand(vector want, float tolerance)
	{
		WorldEditorAPI api = LF_HoldTuner.Api();
		IEntity item = api.SourceToEntity(api.FindEntityByName(LF_HoldTuner.ITEM_NAME));
		IEntity character = api.SourceToEntity(api.FindEntityByName(LF_HoldTuner.CHARACTER_NAME));
		if (!item || !character)
		{
			Fail("expect-hand: no preview");
			return;
		}

		Expect("hand origin (body frame)", character.CoordToLocal(item.GetOrigin()), want, tolerance);
	}
}

//! The live panel: World Editor toolbar > LF Hold Tuner. Each button (or number-pad key) moves the preview at once.
[WorkbenchToolAttribute(name: "LF Hold Tuner", description: "Nudge the Hold Tuner preview item live: offset in centimetres, rotation in degrees. Start it from Plugins > Life Framework > Hold Tuner.", wbModules: { "WorldEditor" }, awesomeFontCode: 0xF256)]
class LF_HoldTunerTool : WorldEditorTool
{
	[Attribute("1", UIWidgets.Auto, "Offset step in centimetres", category: "Steps")]
	float m_fStepCm;

	[Attribute("5", UIWidgets.Auto, "Rotation step in degrees", category: "Steps")]
	float m_fStepDeg;

	//------------------------------------------------------------------------------------------------
	protected void Step(string what, int axis, int sign)
	{
		float amount = sign * m_fStepCm * 0.01;
		if (what == "rotation")
			amount = sign * m_fStepDeg;

		LF_HoldTuner.Nudge(what, axis, amount);
		Print("[LFHoldTuner] " + LF_HoldTuner.Report(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Number pad: 4/6 offset x, 8/2 offset y, 7/1 offset z; with Ctrl the same keys turn a, b, c.
	override void OnKeyPressEvent(KeyCode key, bool isAutoRepeat)
	{
		string what = "offset";
		if (GetModifierKeyState(ModifierKey.CONTROL))
			what = "rotation";

		switch (key)
		{
			case KeyCode.KC_NUMPAD6: Step(what, 0, 1); break;
			case KeyCode.KC_NUMPAD4: Step(what, 0, -1); break;
			case KeyCode.KC_NUMPAD8: Step(what, 1, 1); break;
			case KeyCode.KC_NUMPAD2: Step(what, 1, -1); break;
			case KeyCode.KC_NUMPAD7: Step(what, 2, 1); break;
			case KeyCode.KC_NUMPAD1: Step(what, 2, -1); break;
		}
	}

	[ButtonAttribute("Offset X +")] void OffXPlus() { Step("offset", 0, 1); }
	[ButtonAttribute("Offset X -")] void OffXMinus() { Step("offset", 0, -1); }
	[ButtonAttribute("Offset Y +")] void OffYPlus() { Step("offset", 1, 1); }
	[ButtonAttribute("Offset Y -")] void OffYMinus() { Step("offset", 1, -1); }
	[ButtonAttribute("Offset Z +")] void OffZPlus() { Step("offset", 2, 1); }
	[ButtonAttribute("Offset Z -")] void OffZMinus() { Step("offset", 2, -1); }
	[ButtonAttribute("Rotate A +")] void RotAPlus() { Step("rotation", 0, 1); }
	[ButtonAttribute("Rotate A -")] void RotAMinus() { Step("rotation", 0, -1); }
	[ButtonAttribute("Rotate B +")] void RotBPlus() { Step("rotation", 1, 1); }
	[ButtonAttribute("Rotate B -")] void RotBMinus() { Step("rotation", 1, -1); }
	[ButtonAttribute("Rotate C +")] void RotCPlus() { Step("rotation", 2, 1); }
	[ButtonAttribute("Rotate C -")] void RotCMinus() { Step("rotation", 2, -1); }

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Read from gizmo")]
	void FromGizmo()
	{
		if (LF_HoldTuner.ReadFromGizmo())
			Print("[LFHoldTuner] from gizmo: " + LF_HoldTuner.Report(), LogLevel.NORMAL);
		else
			Print("[LFHoldTuner] no preview: start one from Plugins > Life Framework > Hold Tuner", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	[ButtonAttribute("Save to prefab")]
	void Save()
	{
		string problem;
		if (LF_HoldTuner.SaveToPrefab(problem))
			Print("[LFHoldTuner] saved: " + LF_HoldTuner.Report(), LogLevel.NORMAL);
		else
			Print("[LFHoldTuner] " + problem, LogLevel.ERROR);
	}
}
