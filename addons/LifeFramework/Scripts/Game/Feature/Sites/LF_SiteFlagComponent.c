//------------------------------------------------------------------------------------------------
//! LF_SiteFlagComponent.c: the flag pole of a contested site, with a painted board below it that
//! names the organisation holding the site, or "Unclaimed" (#2685, gang war #1022).
//!
//! The server owns the name. It sets it from `LF_OnSiteCaptured` (for the site's hideout id) and,
//! every RECONCILE_MS, from the hideout's lessee, so a release, a lapse or a give-up reaches the
//! board too. The name is one short `RplProp` string, `Replication.BumpMe()` only when it changes,
//! so a late joiner reads the current holder from the entity's initial state. Each rendering
//! machine paints it on the plate child; a listen host paints it itself because a
//! `RplProp` callback never runs on the server (docs/architecture/rpc-routing.md rule 3).
[ComponentEditorProps(category: "LifeFramework/Feature/Sites", description: "Flag pole of a contested site: the painted board names the holder.")]
class LF_SiteFlagComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_SiteFlagComponent : ScriptComponent
{
	static const string FEATURE = "Sites";
	//! The painted plate's layout, shared with the pole-plate signs.
	static const ResourceName FACE_LAYOUT = "{5783648244305C88}UI/Layouts/Signs/LF_SignPaintFace.layout";
	//! The board's words when nobody holds the site.
	static const string UNCLAIMED_KEY = "#LF-SiteFlag_Unclaimed";
	//! How often the server compares the board with the hideout's lessee.
	static const int RECONCILE_MS = 5000;	// lf-fixed: a catch-up interval for releases and lapses, not a balance knob
	// The plate as on LF_Sign_PolePlate: layout units of its 1024 x 733 face.
	static const float FACE_WIDTH = 1024;	// lf-fixed: the layout size
	static const float FACE_HEIGHT = 733;	// lf-fixed: the layout size
	static const float FACE_RIM = 40;	// lf-fixed: the layout's rim
	static const int FACE_FONT_MAX = 200;	// lf-fixed: the layout's biggest lettering

	[Attribute("", UIWidgets.EditBox, "Hideout id of the contested site this flag marks (the id of its LF_ContestedSiteComponent).")]
	protected string m_sHideoutId;

	[Attribute("0.35 0.06 0.06", UIWidgets.ColorPicker, "Board paint colour (RGB 0..1).")]
	protected vector m_vPaintColor;

	//! The holder's organisation name, "" while nobody holds the site. The only replicated state.
	[RplProp(onRplName: "OnHolderReplicated")]
	protected string m_sHolderName;

	protected int m_iBumps;
	protected IEntity m_FaceTarget;
	protected Widget m_wFaceRoot;
	protected RTTextureWidget m_wFaceTexture;
	protected string m_sShownName = "\n";

	protected static ref array<LF_SiteFlagComponent> s_aFlags = {};
	protected static ref LF_SiteFlagListener s_Listener;

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_aFlags.Clear();
		if (s_Listener)
			LF_EventBus.Unsubscribe(LF_OnSiteCaptured, s_Listener);

		s_Listener = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The name a board shows for a holder: the organisation's name, "" when nobody or an unknown one.
	static string HolderNameOf(string orgId)
	{
		if (orgId.IsEmpty())
			return string.Empty;

		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (!org)
			return string.Empty;

		return org.GetName();
	}

	//------------------------------------------------------------------------------------------------
	//! The words on the board for a replicated name.
	static string BoardText(string holderName)
	{
		string text = holderName;
		if (text.IsEmpty())
			text = WidgetManager.Translate(UNCLAIMED_KEY);

		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	string GetHideoutId()
	{
		return m_sHideoutId;
	}

	//------------------------------------------------------------------------------------------------
	//! A flag spawned by script (a test, a site builder) names its hideout here; one placed in a
	//! world carries it as an attribute.
	void BindHideout(string hideoutId)
	{
		m_sHideoutId = hideoutId;
	}

	//------------------------------------------------------------------------------------------------
	//! The name this machine's copy holds (the replicated value).
	string GetHolderName()
	{
		return m_sHolderName;
	}

	//------------------------------------------------------------------------------------------------
	//! How many times the server bumped the name (tests read it).
	int GetBumpCount()
	{
		return m_iBumps;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the name; bumps replication only when it changed.
	//! \return True when the name changed.
	bool SetHolderName(string name)
	{
		if (!Replication.IsServer())
			return false;

		if (m_sHolderName == name)
			return false;

		m_sHolderName = name;
		m_iBumps++;
		Replication.BumpMe();
		LF_Debug.Info(FEATURE, string.Format("flag %1 holder='%2'", m_sHideoutId, name));
		// A RplProp callback never runs on the server: a listen host's own player paints it here.
		ShowName();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a site changed hands. Another site's event is ignored.
	//! \return True when this flag's name changed.
	bool ApplyCapture(notnull LF_OnSiteCaptured captured)
	{
		if (captured.m_sHideoutId != m_sHideoutId)
			return false;

		return SetHolderName(HolderNameOf(captured.m_sToOrgId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the name from the hideout's lessee (a release, a lapse, a give-up, a rename).
	//! \return True when this flag's name changed.
	bool Reconcile()
	{
		if (!Replication.IsServer() || m_sHideoutId.IsEmpty())
			return false;

		return SetHolderName(HolderNameOf(LF_Hideout.GetOwner(m_sHideoutId)));
	}

	//------------------------------------------------------------------------------------------------
	//! Every flag alive on this machine. LF_SiteFlagListener walks it on a capture.
	static array<LF_SiteFlagComponent> All()
	{
		return s_aFlags;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Listen()
	{
		if (s_Listener)
			return;

		s_Listener = new LF_SiteFlagListener();
		LF_EventBus.Subscribe(LF_OnSiteCaptured, s_Listener);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		s_aFlags.Insert(this);
		if (m_sHideoutId.IsEmpty())
			LF_Debug.Warn(FEATURE, "site flag without a hideout id: its board stays unclaimed until BindHideout");

		if (Replication.IsServer())
		{
			Listen();
			if (GetGame().GetCallqueue())
				GetGame().GetCallqueue().CallLater(ReconcileTick, RECONCILE_MS, true);
		}

		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		if (Replication.IsServer())
			Reconcile();

		if (RplSession.Mode() == RplMode.Dedicated || !GetGame().GetWorkspace())
			return;

		// After the world finishes loading, so the plate child is placed.
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(BuildFace, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aFlags.RemoveItem(this);
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(ReconcileTick);
			GetGame().GetCallqueue().Remove(BuildFace);
		}

		ClearFace();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReconcileTick()
	{
		Reconcile();
	}

	//------------------------------------------------------------------------------------------------
	//! A client: the replicated name arrived or changed.
	protected void OnHolderReplicated()
	{
		ShowName();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildFace()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted() || m_wFaceRoot)
			return;

		IEntity plate = owner.GetChildren();
		if (!plate)
		{
			LF_Debug.Error(FEATURE, "site flag " + m_sHideoutId + " has no plate child to paint");
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wFaceRoot = workspace.CreateWidgets(FACE_LAYOUT);
		if (!m_wFaceRoot)
		{
			LF_Debug.Error(FEATURE, "site flag board layout failed to load: " + FACE_LAYOUT);
			return;
		}

		m_wFaceTexture = RTTextureWidget.Cast(m_wFaceRoot.FindAnyWidget("RTTexture0"));
		Widget rim = m_wFaceRoot.FindAnyWidget("Rim");
		Widget paint = m_wFaceRoot.FindAnyWidget("Paint");
		if (!m_wFaceTexture || !rim || !paint || !m_wFaceRoot.FindAnyWidget("Title"))
		{
			LF_Debug.Error(FEATURE, "site flag board layout is missing RTTexture0, Rim, Paint or Title");
			ClearFace();
			return;
		}

		FrameSlot.SetAnchorMin(rim, 0, 0);
		FrameSlot.SetAnchorMax(rim, 0, 0);
		FrameSlot.SetPos(rim, 0, 0);
		FrameSlot.SetSize(rim, FACE_WIDTH, FACE_HEIGHT);
		rim.SetColor(Color.White);

		FrameSlot.SetAnchorMin(paint, 0, 0);
		FrameSlot.SetAnchorMax(paint, 0, 0);
		FrameSlot.SetPos(paint, FACE_RIM, FACE_RIM);
		FrameSlot.SetSize(paint, FACE_WIDTH - 2 * FACE_RIM, FACE_HEIGHT - 2 * FACE_RIM);
		paint.SetColor(Color.FromVector(m_vPaintColor));

		m_wFaceTexture.SetRenderTarget(plate);
		m_FaceTarget = plate;
		m_sShownName = "\n";
		ShowName();
	}

	//------------------------------------------------------------------------------------------------
	//! Paint the current name on the plate, when this machine has one and the name changed.
	protected void ShowName()
	{
		if (!m_wFaceRoot || m_sShownName == m_sHolderName)
			return;

		TextWidget title = TextWidget.Cast(m_wFaceRoot.FindAnyWidget("Title"));
		if (!title)
			return;

		float width = FACE_WIDTH - 2 * FACE_RIM - 40;
		float height = FACE_HEIGHT - 2 * FACE_RIM - 40;
		string lines;
		int font = LF_SignComponent.PaintedTitleFit(BoardText(m_sHolderName), width, height, FACE_FONT_MAX, lines);

		float scale = 1;
		#ifdef WORKBENCH
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && workspace.DPIScale(1) > 0)
			scale = workspace.DPIScale(1);
		#endif

		FrameSlot.SetAnchorMin(title, 0, 0);
		FrameSlot.SetAnchorMax(title, 0, 0);
		FrameSlot.SetPos(title, (FACE_RIM + 20) * scale, (FACE_RIM + 20) * scale);
		FrameSlot.SetSize(title, width * scale, height * scale);
		title.SetExactFontSize(Math.Round(font * scale));
		title.SetTextWrapping(false);
		title.SetColor(Color.White);
		title.SetText(lines);
		m_sShownName = m_sHolderName;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearFace()
	{
		if (m_wFaceTexture && m_FaceTarget && !m_FaceTarget.IsDeleted())
			m_wFaceTexture.RemoveRenderTarget(m_FaceTarget);

		m_wFaceTexture = null;
		m_FaceTarget = null;
		if (m_wFaceRoot)
			m_wFaceRoot.RemoveFromHierarchy();

		m_wFaceRoot = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Hands each capture to the flag of the site it happened on.
class LF_SiteFlagListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnSiteCaptured captured = LF_OnSiteCaptured.Cast(busEvent);
		if (!captured)
			return true;

		foreach (LF_SiteFlagComponent flag : LF_SiteFlagComponent.All())
		{
			if (flag)
				flag.ApplyCapture(captured);
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SiteFlagComponent's statics (LF_StaticReset finds it by reflection).
class LF_SiteFlagComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SiteFlagComponent";
	}

	override void Apply()
	{
		LF_SiteFlagComponent.ResetStatics();
	}
}
