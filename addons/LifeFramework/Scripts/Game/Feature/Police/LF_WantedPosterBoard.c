//------------------------------------------------------------------------------------------------
//! The wanted board's face on one client: one render target over the cork (a local
//! LF_BoardFace quad whose material samples $rendertarget), the cork drawn in it, and a paper
//! poster per entry. Built when the viewer comes within the draw radius, released beyond it.
//! Every (re)draw renders live for LIVE_MS, then the widget root is hidden: the mesh keeps the last
//! frame and the target stops costing anything (docs/architecture/visual-boards-budget.md).
class LF_WantedPosterFace : Managed
{
	static const ResourceName FACE_PREFAB = "{20A9CE2A32DFF4FB}Prefabs/Props/Signs/LF_BoardFace.et";
	static const ResourceName FACE_LAYOUT = "{6A0E5C7D93B14F28}UI/Layouts/Police/LF_WantedBoardFace.layout";
	static const ResourceName POSTER_LAYOUT = "{9F2B61D47C0E3A85}UI/Layouts/Police/LF_WantedPoster.layout";
	static const ResourceName CORK_TEXTURE = "{95F693947BE9C0BA}Assets/_SharedData/Wood/ST_Cork_01_BCR.edds";

	//! The target, in layout units: LF_WantedBoardFace.layout, 16:9 like LF_BoardFace.xob.
	static const float FACE_WIDTH = 1920;	// lf-fixed: the layout size
	static const float FACE_HEIGHT = 1080;	// lf-fixed: the layout size
	//! A poster: 0.30 x 0.43 m on a 1.70 m wide face, the spike's sheet.
	static const float POSTER_WIDTH = 340;	// lf-fixed: the layout size
	static const float POSTER_HEIGHT = 480;	// lf-fixed: the layout size
	static const float POSTER_GAP = 44;	// lf-fixed: the layout size
	//! The poster's own design units (the spike's 512 x 724 sheet), scaled to POSTER_WIDTH.
	static const float DESIGN_WIDTH = 512;	// lf-fixed: the layout size
	static const int PER_ROW = 4;	// lf-fixed: the layout has two rows of four
	//! How long a (re)drawn face renders before it freezes.
	static const int LIVE_MS = 3000;	// lf-fixed: measured, docs/architecture/visual-boards-budget.md
	//! A new face renders again this long after it was built, and again at twice that and more:
	//! textures that were still streaming in (the cork, the clothes in a portrait) are on the
	//! frozen frame then. A late joiner's cork was still at its lowest mip after 10 s.
	static const int SETTLE_REFRESH_MS = 10000;	// lf-fixed: texture streaming, not tuning
	static const int LATE_REFRESH_MS = 30000;	// lf-fixed: texture streaming, not tuning
	//! The settle refresh's live window.
	static const int REFRESH_LIVE_MS = 2000;	// lf-fixed: one frame at SetMaxFPS(1), with margin

	protected IEntity m_Face;
	protected Widget m_wRoot;
	protected RTTextureWidget m_wTexture;
	protected Widget m_wPosters;
	protected ref array<Widget> m_aPosters = {};
	protected bool m_bFrozen;

	//------------------------------------------------------------------------------------------------
	//! Spawn the face on the board and its render target. False (logged) when a resource is missing.
	bool Build(notnull IEntity board, vector offset, vector angles, float scale)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Resource faceResource = Resource.Load(FACE_PREFAB);
		if (!workspace || !faceResource || !faceResource.IsValid())
		{
			LF_Debug.Error(LF_WantedPosters.FEATURE, "board face not built: no workspace or the face prefab does not load: " + FACE_PREFAB);
			return false;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = board;
		Math3D.AnglesToMatrix(angles, params.Transform);
		params.Transform[3] = offset;
		params.Scale = scale;
		m_Face = GetGame().SpawnEntityPrefabLocal(faceResource, board.GetWorld(), params);
		m_wRoot = workspace.CreateWidgets(FACE_LAYOUT);
		if (m_wRoot)
		{
			m_wTexture = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("RTTexture0"));
			m_wPosters = m_wRoot.FindAnyWidget("Posters");
		}

		if (!m_Face || !m_wTexture || !m_wPosters)
		{
			LF_Debug.Error(LF_WantedPosters.FEATURE, "board face not built: the face entity or LF_WantedBoardFace.layout (RTTexture0, Posters) is missing");
			Release();
			return false;
		}

		// The face covers the board's cork, so it draws the cork the vanilla board is made of, tinted
		// in the layout to the vanilla board's colour under the scene light (the texture alone reads
		// pale through the render target). Without the texture the tint alone stays.
		ImageWidget cork = ImageWidget.Cast(m_wRoot.FindAnyWidget("Cork"));
		if (!cork || !cork.LoadImageTexture(0, CORK_TEXTURE))
			LF_Debug.Warn(LF_WantedPosters.FEATURE, "cork texture does not load, the face is plain: " + CORK_TEXTURE);

		// Paper does not move: one frame a second while live, none once frozen.
		m_wTexture.SetMaxFPS(1);
		m_wTexture.SetRenderTarget(m_Face);
		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Refresh, SETTLE_REFRESH_MS, false);
			GetGame().GetCallqueue().CallLater(Refresh, LATE_REFRESH_MS, false);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsBuilt()
	{
		return m_Face && m_wRoot;
	}

	//------------------------------------------------------------------------------------------------
	bool IsFrozen()
	{
		return m_bFrozen;
	}

	//------------------------------------------------------------------------------------------------
	//! Where poster `index` of `count` sits, top-left in layout units: one centred row up to four,
	//! two rows (the first full) above that.
	static vector SlotPosition(int index, int count)
	{
		int rows = 1;
		if (count > PER_ROW)
			rows = 2;

		int row = index / PER_ROW;
		int inRow = Math.Min(PER_ROW, count - row * PER_ROW);
		int col = index - row * PER_ROW;
		float rowWidth = inRow * POSTER_WIDTH + (inRow - 1) * POSTER_GAP;
		float blockHeight = rows * POSTER_HEIGHT + (rows - 1) * POSTER_GAP;
		float x = (FACE_WIDTH - rowWidth) * 0.5 + col * (POSTER_WIDTH + POSTER_GAP);
		float y = (FACE_HEIGHT - blockHeight) * 0.5 + row * (POSTER_HEIGHT + POSTER_GAP);
		return Vector(x, y, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Replace the posters and render them live, then freeze.
	//! \return Posters drawn.
	int Show(notnull array<string> entries)
	{
		if (!IsBuilt())
			return 0;

		ClearPosters();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return 0;

		int count = Math.Min(entries.Count(), LF_WantedPostersConfig.POSTERS_MAX);
		for (int i = 0; i < count; i++)
		{
			LF_WantedPosterEntry entry = LF_WantedPosters.Decode(entries[i]);
			if (!entry)
				continue;

			Widget poster = workspace.CreateWidgets(POSTER_LAYOUT, m_wPosters);
			if (!poster)
			{
				LF_Debug.Error(LF_WantedPosters.FEATURE, "poster layout does not load: " + POSTER_LAYOUT);
				break;
			}

			vector slot = SlotPosition(i, count);
			Place(poster, slot[0], slot[1], POSTER_WIDTH, POSTER_HEIGHT, 1);
			FillPerson(poster, entry);
			m_aPosters.Insert(poster);
		}

		Thaw(LIVE_MS);
		return m_aPosters.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Positions a widget at x, y with a size, in its parent's units times `scale`.
	protected static void Place(Widget w, float x, float y, float width, float height, float scale)
	{
		if (!w)
			return;

		FrameSlot.SetAnchorMin(w, 0, 0);
		FrameSlot.SetAnchorMax(w, 0, 0);
		FrameSlot.SetPos(w, x * scale, y * scale);
		FrameSlot.SetSize(w, width * scale, height * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! A text line of the poster, in the poster's design units; an empty text hides the line.
	protected static void Line(Widget poster, string widgetName, string text, float y, float height, int font)
	{
		TextWidget w = TextWidget.Cast(poster.FindAnyWidget(widgetName));
		if (!w)
			return;

		float scale = POSTER_WIDTH / DESIGN_WIDTH;
		Place(w, 16, y, DESIGN_WIDTH - 32, height, scale);
		w.SetExactFontSize(Math.Round(font * scale));
		w.SetTextWrapping(true);
		w.SetText(text);
		w.SetVisible(!text.IsEmpty());
	}

	//------------------------------------------------------------------------------------------------
	//! WANTED, the photo frame, the name (or UNKNOWN PERSON), the offence, what was seen, the level.
	protected void FillPerson(notnull Widget poster, notnull LF_WantedPosterEntry entry)
	{
		float scale = POSTER_WIDTH / DESIGN_WIDTH;
		Place(poster.FindAnyWidget("Paper"), 0, 0, DESIGN_WIDTH, 724, scale);
		Place(poster.FindAnyWidget("PhotoBack"), 96, 112, 320, 340, scale);
		Place(poster.FindAnyWidget("PhotoClip"), 96, 112, 320, 340, scale);
		Widget plate = poster.FindAnyWidget("PlateBack");
		if (plate)
			plate.SetVisible(false);

		string header = WidgetManager.Translate("#LF-Wanted_PosterHeader");
		header.ToUpper();
		string name = entry.m_sName;
		if (name.IsEmpty())
			name = WidgetManager.Translate("#LF-Wanted_Unknown");

		name.ToUpper();
		Line(poster, "Header", header, 10, 96, 88);
		Line(poster, "Name", name, 458, 70, 46);
		Line(poster, "Offence", LF_WantedBoard.ResolveKey(entry.m_sOffence), 528, 56, 36);
		Line(poster, "Detail", LF_WantedPosters.Description(entry.m_sClothing, entry.m_sVehicle), 584, 78, 24);
		Line(poster, "Reward", WidgetManager.Translate("#LF-Wanted_Level", entry.m_iLevel), 664, 50, 34);
	}

	//------------------------------------------------------------------------------------------------
	//! Render live again for `liveMs`, then freeze.
	protected void Thaw(int liveMs)
	{
		m_bFrozen = false;
		if (m_wRoot)
			m_wRoot.SetVisible(true);

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Freeze);
			GetGame().GetCallqueue().CallLater(Freeze, liveMs, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The settle refresh after a build: one more short live window.
	protected void Refresh()
	{
		if (IsBuilt() && m_bFrozen)
			Thaw(REFRESH_LIVE_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Stop drawing the target. The face keeps the last frame.
	void Freeze()
	{
		if (!m_wRoot)
			return;

		m_wRoot.SetVisible(false);
		m_bFrozen = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPosters()
	{
		foreach (Widget poster : m_aPosters)
		{
			if (poster)
				poster.RemoveFromHierarchy();
		}

		m_aPosters.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Free the target, the widgets and the face entity; the board shows its own cork again.
	void Release()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Freeze);
			GetGame().GetCallqueue().Remove(Refresh);
		}

		ClearPosters();
		if (m_wTexture && m_Face && !m_Face.IsDeleted())
			m_wTexture.RemoveRenderTarget(m_Face);

		m_wTexture = null;
		m_wPosters = null;
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		if (m_Face && !m_Face.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Face);

		m_Face = null;
		m_bFrozen = false;
	}
}

//------------------------------------------------------------------------------------------------
//! Client: builds the faces of the wanted boards near the viewer and releases far ones. One timer
//! for every board, once a second; a distance check, not a data poll (the data comes by event).
class LF_WantedPosterFaces
{
	static const int TICK_MS = 1000;	// lf-fixed: a distance check, not tuning
	//! Released this far beyond the draw radius, so walking along the edge does not rebuild.
	static const float RELEASE_MARGIN = 5;	// lf-fixed: hysteresis on the configured radius

	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	static void EnsureTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Build within `radius`, release beyond `radius` plus the margin, keep in between.
	//! \return 1 build, -1 release, 0 keep.
	static int Decide(float distance, float radius, bool built)
	{
		if (!built && distance <= radius)
			return 1;

		if (built && distance > radius + RELEASE_MARGIN)
			return -1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Where this client looks from: the active camera, else the local character.
	static bool ViewerPosition(out vector position)
	{
		if (!GetGame())
			return false;

		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && cameras.CurrentCamera())
		{
			position = cameras.CurrentCamera().GetOrigin();
			return true;
		}

		IEntity me = LF_Utils.GetLocalCharacter();
		if (!me)
			return false;

		position = me.GetOrigin();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		vector viewer;
		if (ViewerPosition(viewer))
			LF_WantedPosterBoardComponent.ViewAll(viewer);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Police", description: "Draws the wanted posters on the board's face for every player near it")]
class LF_WantedPosterBoardComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The wanted board's posters on the prop (docs/architecture/visual-boards-spike.md). The server
//! holds the list LF_WantedPosters builds and sends it: RplSave/RplLoad for a late joiner or a
//! board that streams in, RpcDo_SetPosters when it changes. Each client draws its own face within
//! the draw radius (LF_WantedPosterFace); a dedicated server draws nothing. The Read action stays
//! for the full card and the police-only lines.
class LF_WantedPosterBoardComponent : ScriptComponent
{
	//! RplSave/RplLoad layout; bump it when the fields change.
	static const int FORMAT_VERSION = 1;	// lf-fixed: wire format

	[Attribute("1", UIWidgets.CheckBox, "Draw the wanted posters on this board. Off on the service dispatch boards, which inherit the wanted board but show their own calls on Read")]
	protected bool m_bDrawPosters;

	[Attribute("0 0.555 -0.006", UIWidgets.EditBox, "Face position relative to the board: the cork's centre, just in front of it")]
	protected vector m_vFaceOffset;

	[Attribute("-90 0 0", UIWidgets.EditBox, "Face rotation relative to the board (yaw pitch roll, degrees); LF_BoardFace.xob faces +X, the board -Z")]
	protected vector m_vFaceAngles;

	[Attribute("0.965", UIWidgets.EditBox, "Face scale. 1 is 1.78 x 1 m; the cork board's cork is about 1.71 x 0.96 m")]
	protected float m_fFaceScale;

	//! Every wanted board in play on this machine, server and client roles alike.
	protected static ref array<LF_WantedPosterBoardComponent> s_aBoards;

	protected ref array<string> m_aEntries = {};
	protected float m_fRadius = LF_WantedPostersConfig.DEFAULT_DRAW_RADIUS;
	protected ref LF_WantedPosterFace m_Face;
	protected bool m_bClient;
	//! A face that failed to build (a missing resource, logged once) is not tried again.
	protected bool m_bBuildFailed;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!m_bDrawPosters || !GetGame() || !GetGame().InPlayMode())
			return;

		if (!s_aBoards)
			s_aBoards = {};

		s_aBoards.Insert(this);
		m_bClient = RplSession.Mode() != RplMode.Dedicated && GetGame().GetWorkspace() != null;
		if (m_bClient)
			LF_WantedPosterFaces.EnsureTicking();

		if (Replication.IsServer())
			LF_WantedPosters.BoardJoined(this);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBoards = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every board sends the list. \return Boards that sent it.
	static int SendToAll(notnull array<string> entries, float radius)
	{
		if (!s_aBoards)
			return 0;

		int sent;
		foreach (LF_WantedPosterBoardComponent board : s_aBoards)
		{
			if (!board)
				continue;

			board.ServerSet(entries, radius, true);
			sent++;
		}

		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the distance gate for every board (LF_WantedPosterFaces).
	static void ViewAll(vector viewer)
	{
		if (!s_aBoards)
			return;

		foreach (LF_WantedPosterBoardComponent board : s_aBoards)
		{
			if (board && board.m_bClient)
				board.UpdateView(viewer);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest wanted board this machine knows, as "posters/face" ("2/frozen"); "-" with none.
	//! The harness's `seen` reads it.
	static string DescribeNearest(vector origin)
	{
		LF_WantedPosterBoardComponent nearest;
		float best = float.MAX;
		if (s_aBoards)
		{
			foreach (LF_WantedPosterBoardComponent board : s_aBoards)
			{
				if (!board || !board.GetOwner())
					continue;

				float distance = vector.Distance(origin, board.GetOwner().GetOrigin());
				if (distance < best)
				{
					best = distance;
					nearest = board;
				}
			}
		}

		if (!nearest)
			return "-";

		return string.Format("%1/%2", nearest.GetEntries().Count(), nearest.FaceState());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the list for every client. `send` false sets it without a broadcast (a board that
	//! registers late takes the current list; its RplSave carries it).
	void ServerSet(notnull array<string> entries, float radius, bool send)
	{
		m_aEntries = {};
		m_aEntries.Copy(entries);
		m_fRadius = radius;
		if (send)
			Rpc(RpcDo_SetPosters, entries, radius);

		// A listen server or Workbench draws too; a broadcast does not run on its sender.
		if (m_bClient)
			Changed("host");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPosters(array<string> entries, float radius)
	{
		Apply(entries, radius, "rpc");
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(array<string> entries, float radius, string source)
	{
		m_aEntries = {};
		if (entries)
		{
			foreach (string entry : entries)
			{
				if (m_aEntries.Count() < LF_WantedPostersConfig.POSTERS_MAX)
					m_aEntries.Insert(entry);
			}
		}

		m_fRadius = Math.Clamp(radius, LF_WantedPostersConfig.RADIUS_MIN, LF_WantedPostersConfig.RADIUS_MAX);
		Changed(source);
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		// A board that draws no posters (a dispatch board) sends nothing; both sides read the same
		// prefab, so its RplLoad reads nothing.
		if (!m_bDrawPosters)
			return true;

		writer.WriteInt(FORMAT_VERSION);
		writer.WriteFloat(m_fRadius);
		writer.WriteInt(m_aEntries.Count());
		foreach (string entry : m_aEntries)
			writer.WriteString(entry);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!m_bDrawPosters)
			return true;

		int version;
		float radius;
		int count;
		if (!reader.ReadInt(version) || !reader.ReadFloat(radius) || !reader.ReadInt(count))
			return false;

		// The strings are read whatever the format, so the stream stays aligned for what follows.
		array<string> entries = {};
		for (int i = 0; i < count; i++)
		{
			string entry;
			if (!reader.ReadString(entry))
				return false;

			entries.Insert(entry);
		}

		if (version != FORMAT_VERSION)
		{
			LF_Debug.Error(LF_WantedPosters.FEATURE, string.Format("board posters dropped: the server sends format %1, this build reads %2", version, FORMAT_VERSION));
			return true;
		}

		Apply(entries, radius, "jip");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: new posters. A built face redraws; a far board only keeps the list.
	protected void Changed(string source)
	{
		LF_Debug.Info(LF_WantedPosters.FEATURE, string.Format("client: %1 poster(s) from %2 at %3", m_aEntries.Count(), source, GetOwner().GetOrigin()));
		if (m_Face && m_Face.IsBuilt())
			m_Face.Show(m_aEntries);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the distance gate (LF_WantedPosterFaces).
	void UpdateView(vector viewer)
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		if (m_bBuildFailed)
			return;

		bool built = m_Face && m_Face.IsBuilt();
		int decision = LF_WantedPosterFaces.Decide(vector.Distance(viewer, owner.GetOrigin()), m_fRadius, built);
		if (decision > 0)
			BuildFace();
		else if (decision < 0)
			ReleaseFace("out of range");
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildFace()
	{
		int started = System.GetTickCount();
		m_Face = new LF_WantedPosterFace();
		if (!m_Face.Build(GetOwner(), m_vFaceOffset, m_vFaceAngles, m_fFaceScale))
		{
			m_Face = null;
			m_bBuildFailed = true;
			return;
		}

		int drawn = m_Face.Show(m_aEntries);
		LF_Debug.Info(LF_WantedPosters.FEATURE, string.Format("client: face built at %1 with %2 poster(s) in %3 ms", GetOwner().GetOrigin(), drawn, System.GetTickCount() - started));
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseFace(string why)
	{
		if (!m_Face)
			return;

		m_Face.Release();
		m_Face = null;
		LF_Debug.Info(LF_WantedPosters.FEATURE, string.Format("client: face released at %1 (%2)", GetOwner().GetOrigin(), why));
	}

	//------------------------------------------------------------------------------------------------
	//! The list this machine holds. Tests read it.
	array<string> GetEntries()
	{
		return m_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: whether the face is drawn and whether it has frozen. Proofs read it.
	string FaceState()
	{
		if (!m_Face || !m_Face.IsBuilt())
			return "released";

		if (m_Face.IsFrozen())
			return "frozen";

		return "live";
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aBoards)
			s_aBoards.RemoveItem(this);

		ReleaseFace("deleted");
		super.OnDelete(owner);
	}
}
