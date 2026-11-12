// LF_VisualBoard.c - SPIKE (docs/architecture/visual-boards-spike.md). Not production code.
//
// The wanted board shows its posters on the prop: one A4 sheet per wanted person (a portrait,
// the name once identified, the offence, the wanted level) and one per stolen car (a picture of
// the model and the plate). The server owns the list and sends it to every client that has the
// board streamed in: RplSave/RplLoad for a late joiner, one reliable broadcast when it changes.
// Each client draws its own sheets (RTTextureWidget -> $rendertarget) and its own portraits
// (ItemPreviewWidget inside the render target); a dedicated server draws nothing.

//------------------------------------------------------------------------------------------------
//! Server-side capture of what a poster needs beyond the text record.
class LF_VisualBoardData
{
	static const string FEATURE = "VisualBoard";
	static const int MAX_PEOPLE = 3; // lf-fixed: spike
	static const int MAX_VEHICLES = 1; // lf-fixed: spike
	static const string KIND_PERSON = "P";
	static const string KIND_VEHICLE = "V";
	//! Spike: fill free sheets with made-up entries so the look and the late-join path show
	//! something before any crime.
	static bool s_bDemo = true;

	//! Report sequence -> "characterPrefab|slot=clothPrefab|...".
	protected static ref map<int, string> s_mAppearance = new map<int, string>();
	//! Stolen vehicle plate -> vehicle prefab.
	protected static ref map<string, string> s_mVehiclePrefab = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	//! Server: remember how the suspect looked when report `sequence` was made.
	static void CaptureSuspect(int sequence, IEntity suspect)
	{
		if (!suspect || !suspect.GetPrefabData())
			return;

		string appearance = suspect.GetPrefabData().GetPrefabName();
		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(suspect.FindComponent(EquipedLoadoutStorageComponent));
		if (storage)
		{
			for (int i = 0; i < storage.GetSlotsCount(); i++)
			{
				IEntity item = storage.Get(i);
				if (!item || !item.GetPrefabData())
					continue;

				appearance += "|" + i.ToString() + "=" + item.GetPrefabData().GetPrefabName();
			}
		}

		s_mAppearance.Set(sequence, appearance);
		LF_Debug.Info(FEATURE, string.Format("captured suspect of report #%1: %2 chars", sequence, appearance.Length()));
	}

	//------------------------------------------------------------------------------------------------
	static void CaptureVehicle(string plate, IEntity vehicle)
	{
		if (plate.IsEmpty() || !vehicle || !vehicle.GetPrefabData())
			return;

		s_mVehiclePrefab.Set(plate, vehicle.GetPrefabData().GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	protected static string Clean(string text)
	{
		text.Replace(";", ",");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! "kind;title;offence;level;detail;appearance". The appearance is last: nothing in it is typed
	//! by a player.
	static string Encode(string kind, string title, string offence, int level, string detail, string appearance)
	{
		return string.Format("%1;%2;%3;%4;%5;%6", kind, Clean(title), Clean(offence), level, Clean(detail), appearance);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the public posters, people first (one per suspect, newest report), then stolen cars.
	static void BuildEntries(notnull array<string> outEntries)
	{
		outEntries.Clear();
		array<LF_WantedReportRecord> reports = {};
		LF_WantedReports.CollectNewestFirst(reports);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		array<string> seen = {};
		foreach (LF_WantedReportRecord report : reports)
		{
			if (seen.Count() >= MAX_PEOPLE)
				break;

			if (seen.Contains(report.m_sPlayerUid))
				continue;

			seen.Insert(report.m_sPlayerUid);
			string name;
			int level = 1;
			if (accounts)
			{
				LF_PlayerCharacter suspect = accounts.FindCharacterByKey(report.m_sPlayerUid);
				LF_CharacterStanding standing = accounts.FindStanding(report.m_sPlayerUid);
				if (report.m_bIdentified && suspect)
					name = suspect.GetFullName();
				if (standing)
					level = standing.m_iWantedLevel;
			}

			string appearance;
			s_mAppearance.Find(report.m_iSequence, appearance);
			outEntries.Insert(Encode(KIND_PERSON, name, report.m_sOffence, level, report.m_sClothing, appearance));
		}

		int people = outEntries.Count();
		if (s_bDemo)
		{
			if (people < 1)
				outEntries.Insert(Encode(KIND_PERSON, "Josef Novak", LF_WantedReports.OFFENCE_ROBBERY, 2, "", "{2CDFBF355F3410DD}Prefabs/Characters/Factions/CIV/Dockworker/Character_CIV_Dockworker_2.et"));
			if (people < 2)
				outEntries.Insert(Encode(KIND_PERSON, "", LF_WantedReports.OFFENCE_VEHICLE_THEFT, 1, "", "{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et"));
			if (people < 3)
				outEntries.Insert(Encode(KIND_PERSON, "Karel Dvorak", LF_WantedReports.OFFENCE_MURDER, 3, "", "{2CE7D4A6C32673BD}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_3.et"));
		}

		array<string> stolen = {};
		LF_StolenVehicles.BoardEntries(MAX_VEHICLES, stolen);
		foreach (string entry : stolen)
		{
			// "V;plate;nameKey;place"
			array<string> fields = {};
			entry.Split(";", fields, false);
			if (fields.Count() < 3)
				continue;

			string prefab;
			s_mVehiclePrefab.Find(fields[1], prefab);
			outEntries.Insert(Encode(KIND_VEHICLE, fields[1], fields[2], 0, "", prefab));
		}

		if (s_bDemo && stolen.IsEmpty())
			outEntries.Insert(Encode(KIND_VEHICLE, "EV 47-19", "S105", 0, "", "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et"));
	}
}

//------------------------------------------------------------------------------------------------
//! Client: one sheet on the board.
class LF_VisualPoster : Managed
{
	static const ResourceName SHEET_PREFAB = "{B7765F4913B42527}Prefabs/Props/Signs/LF_SignFace.et";
	static const ResourceName LAYOUT = "{9D3E71A45C0BFA01}UI/Layouts/Police/LF_WantedPoster.layout";
	static const ResourceName PREVIEW_MANAGER = "{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et";

	protected IEntity m_Sheet;
	protected IEntity m_Dummy;
	protected Widget m_wRoot;
	protected RTTextureWidget m_wTexture;
	protected float m_fScale = 1;

	//------------------------------------------------------------------------------------------------
	protected void Place(Widget w, float x, float y, float width, float height)
	{
		if (!w)
			return;

		FrameSlot.SetAnchorMin(w, 0, 0);
		FrameSlot.SetAnchorMax(w, 0, 0);
		FrameSlot.SetPos(w, x * m_fScale, y * m_fScale);
		FrameSlot.SetSize(w, width * m_fScale, height * m_fScale);
	}

	//------------------------------------------------------------------------------------------------
	protected void Line(string widgetName, string text, float y, float height, int font)
	{
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget(widgetName));
		if (!w)
			return;

		Place(w, 16, y, 480, height);
		w.SetExactFontSize(Math.Round(font * m_fScale));
		w.SetTextWrapping(true);
		w.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! \param offset Sheet centre in the board's space.
	bool Build(IEntity board, vector offset, float sheetScale, string entry)
	{
		array<string> fields = {};
		entry.Split(";", fields, false);
		if (fields.Count() < 6)
		{
			LF_Debug.Error(LF_VisualBoardData.FEATURE, "poster dropped, malformed entry: " + entry);
			return false;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Resource sheetResource = Resource.Load(SHEET_PREFAB);
		if (!workspace || !sheetResource || !sheetResource.IsValid())
			return false;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = board;
		Math3D.AnglesToMatrix("0 0 0", params.Transform);
		params.Transform[3] = offset;
		params.Scale = sheetScale;
		m_Sheet = GetGame().SpawnEntityPrefabLocal(sheetResource, board.GetWorld(), params);
		m_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!m_Sheet || !m_wRoot)
		{
			LF_Debug.Error(LF_VisualBoardData.FEATURE, "poster sheet or layout failed");
			Clear();
			return false;
		}

		m_wTexture = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("RTTexture0"));
		if (!m_wTexture)
		{
			Clear();
			return false;
		}

		// Workbench renders the target at layout size times the DPI scale while the widgets lay
		// out one unit per pixel (LF_SignComponent.FaceTitleRect).
		float tw, th;
		m_wTexture.GetScreenSize(tw, th);
		LF_Debug.Info(LF_VisualBoardData.FEATURE, string.Format("target: dpi=%1 workspace=%2x%3 rt=%4x%5", workspace.DPIScale(1), workspace.GetWidth(), workspace.GetHeight(), tw, th));

		bool person = fields[0] == LF_VisualBoardData.KIND_PERSON;
		string title = fields[1];
		string offence = LF_WantedBoard.ResolveKey(fields[2]);
		int level = fields[3].ToInt();
		string appearance = fields[5];
		for (int i = 6; i < fields.Count(); i++)
			appearance += ";" + fields[i];

		Place(m_wRoot.FindAnyWidget("Paper"), 0, 0, 512, 724);
		Place(m_wRoot.FindAnyWidget("PhotoBack"), 96, 112, 320, 340);
		ItemPreviewWidget photo = ItemPreviewWidget.Cast(m_wRoot.FindAnyWidget("Photo"));
		Widget clip = m_wRoot.FindAnyWidget("PhotoClip");
		Place(clip, 96, 112, 320, 340);
		if (clip)
			clip.SetFlags(WidgetFlags.CLIPCHILDREN);

		// A named suspect gets a mugshot: a head-and-shoulders window cut out of a 3x full-body
		// picture. An unknown one is shown head to toe, as the witness saw them.
		if (fields[0] == LF_VisualBoardData.KIND_PERSON && !fields[1].IsEmpty())
			Place(photo, -320, -24, 960, 1020);
		else
			Place(photo, 0, 0, 320, 340);
		Widget plateBack = m_wRoot.FindAnyWidget("PlateBack");

		if (person)
		{
			if (plateBack)
				plateBack.SetVisible(false);

			if (title.IsEmpty())
				title = WidgetManager.Translate("#LF-Wanted_Unknown");

			title.ToUpper();
			Line("Header", "WANTED", 10, 96, 88);
			Line("Name", title, 458, 70, 46);
			Line("Offence", offence, 528, 56, 36);
			Line("Detail", LF_WantedBoard.PosterDescription(fields[4], string.Empty, string.Empty), 584, 78, 22);
			Line("Reward", WidgetManager.Translate("#LF-Wanted_Level", level), 664, 50, 34);
			ShowPortrait(photo, appearance, true);
		}
		else
		{
			Place(plateBack, 76, 466, 360, 84);
			Line("Header", "STOLEN", 10, 96, 88);
			Line("Name", title, 466, 84, 64);
			Line("Offence", offence, 560, 56, 36);
			Line("Detail", "Seen it? Tell the police.", 620, 50, 26);
			Line("Reward", "", 664, 50, 34);
			ShowPortrait(photo, appearance, false);
		}

		// One frame a second is plenty for paper; the engine redraws a render target every frame
		// otherwise.
		m_wTexture.SetMaxFPS(1);
		m_wTexture.SetRenderTarget(m_Sheet);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPortrait(ItemPreviewWidget photo, string appearance, bool person)
	{
		if (!photo || appearance.IsEmpty())
		{
			LF_Debug.Info(LF_VisualBoardData.FEATURE, "poster has no picture: silhouette box only");
			return;
		}

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
		{
			Resource managerResource = Resource.Load(PREVIEW_MANAGER);
			if (managerResource && managerResource.IsValid())
				GetGame().SpawnEntityPrefabLocal(managerResource, world);

			manager = world.GetItemPreviewManager();
		}

		if (!manager)
		{
			LF_Debug.Error(LF_VisualBoardData.FEATURE, "no ItemPreviewManagerEntity: poster without picture");
			return;
		}

		array<string> parts = {};
		appearance.Split("|", parts, true);
		ResourceName prefab = parts[0];
		// Preview attributes made in script from a .conf drew a black frame (spike finding), so the
		// prefab's own camera is used and the mugshot is a clipped window.
		PreviewRenderAttributes attributes;
		if (parts.Count() == 1)
		{
			manager.SetPreviewItemFromPrefab(photo, prefab, attributes, true);
			LF_Debug.Info(LF_VisualBoardData.FEATURE, "picture from prefab: " + prefab);
			return;
		}

		// A dressed dummy: the suspect's character prefab in the preview world, wearing what the
		// server captured (the vanilla arsenal-loadout preview does the same on its shared entity).
		IEntity shared = manager.ResolvePreviewEntityForPrefab(prefab);
		Resource characterResource = Resource.Load(prefab);
		if (!shared || !characterResource || !characterResource.IsValid())
		{
			LF_Debug.Error(LF_VisualBoardData.FEATURE, "no preview entity for " + prefab);
			return;
		}

		m_Dummy = GetGame().SpawnEntityPrefabLocal(characterResource, shared.GetWorld());
		if (!m_Dummy)
		{
			LF_Debug.Warn(LF_VisualBoardData.FEATURE, "dummy did not spawn in the preview world: shared preview entity instead");
			manager.SetPreviewItemFromPrefab(photo, prefab, attributes, true);
			return;
		}

		int dressed;
		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(m_Dummy.FindComponent(EquipedLoadoutStorageComponent));
		for (int i = 1; storage && i < parts.Count(); i++)
		{
			int eq = parts[i].IndexOf("=");
			if (eq <= 0)
				continue;

			InventoryStorageSlot slot = storage.GetSlot(parts[i].Substring(0, eq).ToInt());
			Resource clothResource = Resource.Load(parts[i].Substring(eq + 1, parts[i].Length() - eq - 1));
			if (!slot || !clothResource || !clothResource.IsValid())
				continue;

			IEntity old = slot.GetAttachedEntity();
			if (old)
				SCR_EntityHelper.DeleteEntityAndChildren(old);

			IEntity cloth = GetGame().SpawnEntityPrefabLocal(clothResource, m_Dummy.GetWorld());
			if (!cloth)
				continue;

			slot.AttachEntity(cloth);
			dressed++;
		}

		manager.SetPreviewItem(photo, m_Dummy, attributes, true);
		LF_Debug.Info(LF_VisualBoardData.FEATURE, string.Format("picture from dressed dummy: %1 with %2 item(s)", prefab, dressed));
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		if (m_wTexture && m_Sheet && !m_Sheet.IsDeleted())
			m_wTexture.RemoveRenderTarget(m_Sheet);

		m_wTexture = null;
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		if (m_Dummy && !m_Dummy.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Dummy);

		m_Dummy = null;
		if (m_Sheet && !m_Sheet.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Sheet);

		m_Sheet = null;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Police", description: "SPIKE: draws the wanted posters on the board prop")]
class LF_VisualBoardComponentClass : ScriptComponentClass
{
}

class LF_VisualBoardComponent : ScriptComponent
{
	static const int POLL_MS = 2000; // lf-fixed: spike
	static const int MAX_ENTRIES = 4; // lf-fixed: spike

	[Attribute("0.55", UIWidgets.EditBox, "Height of the sheet centres above the board origin, metres")]
	protected float m_fRowHeight;

	[Attribute("0.34", UIWidgets.EditBox, "Distance between sheet centres, metres")]
	protected float m_fSpacing;

	[Attribute("1.45", UIWidgets.EditBox, "Sheet scale. 1 is A4")]
	protected float m_fSheetScale;

	[Attribute("-0.007", UIWidgets.EditBox, "Sheet depth in front of the board face (local Z)")]
	protected float m_fDepth;

	protected ref array<string> m_aEntries = {};
	protected ref array<ref LF_VisualPoster> m_aPosters = {};
	protected string m_sSignature;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		// Spike: the dispatch boards inherit the wanted board prefab; only the wanted board draws.
		LF_SignComponent sign = LF_SignComponent.Cast(owner.FindComponent(LF_SignComponent));
		if (sign && sign.GetSignId() != "police_wanted")
			return;

		if (Replication.IsServer() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(ServerPoll, 500, false);
			GetGame().GetCallqueue().CallLater(ServerPoll, POLL_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuild the list; send it when it differs from what the clients have.
	protected void ServerPoll()
	{
		array<string> entries = {};
		LF_VisualBoardData.BuildEntries(entries);
		while (entries.Count() > MAX_ENTRIES)
			entries.Remove(entries.Count() - 1);

		string signature;
		foreach (string entry : entries)
			signature += entry + "\n";

		if (signature == m_sSignature)
			return;

		m_sSignature = signature;
		m_aEntries = entries;
		LF_Debug.Info(LF_VisualBoardData.FEATURE, string.Format("server: board list changed, %1 poster(s), %2 chars, broadcast", entries.Count(), signature.Length()));
		Rpc(RpcDo_SetEntries, entries);
		if (RplSession.Mode() != RplMode.Dedicated)
			Redraw("host");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetEntries(array<string> entries)
	{
		m_aEntries = {};
		if (entries)
		{
			foreach (string entry : entries)
			{
				if (m_aEntries.Count() < MAX_ENTRIES)
					m_aEntries.Insert(entry);
			}
		}

		Redraw("rpc");
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(1);
		writer.WriteInt(m_aEntries.Count());
		foreach (string entry : m_aEntries)
			writer.WriteString(entry);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int version;
		int count;
		if (!reader.ReadInt(version) || !reader.ReadInt(count))
			return false;

		m_aEntries = {};
		for (int i = 0; i < count; i++)
		{
			string entry;
			if (!reader.ReadString(entry))
				return false;

			if (i < MAX_ENTRIES)
				m_aEntries.Insert(entry);
		}

		Redraw("jip");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Redraw(string source)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !GetGame().GetWorkspace() || !GetGame().GetCallqueue())
			return;

		LF_Debug.Info(LF_VisualBoardData.FEATURE, string.Format("client: %1 poster(s) from %2", m_aEntries.Count(), source));
		GetGame().GetCallqueue().Remove(Draw);
		GetGame().GetCallqueue().CallLater(Draw, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Draw()
	{
		ClearPosters();
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		int count = m_aEntries.Count();
		foreach (int index, string entry : m_aEntries)
		{
			LF_VisualPoster poster = new LF_VisualPoster();
			vector offset = Vector(((count - 1) * 0.5 - index) * m_fSpacing, m_fRowHeight, m_fDepth);
			if (poster.Build(owner, offset, m_fSheetScale, entry))
				m_aPosters.Insert(poster);
		}

		LF_Debug.Info(LF_VisualBoardData.FEATURE, string.Format("client: drew %1 of %2 poster(s) at %3", m_aPosters.Count(), count, owner.GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPosters()
	{
		foreach (LF_VisualPoster poster : m_aPosters)
		{
			if (poster)
				poster.Clear();
		}

		m_aPosters.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(ServerPoll);
			GetGame().GetCallqueue().Remove(Draw);
		}

		ClearPosters();
		super.OnDelete(owner);
	}
}
