// LF_LogoutBodyTag.c: the "Logged out" tag over a logged-out body (#1974, chapter Arrival #1007).
//
// The server (LF_LogoutBody) marks the body it keeps; the mark is a replicated flag on the character
// (SCR_ChimeraCharacter.LF_IsLoggedOut, an RplProp bumped on change, so a late joiner reads it with the
// entity snapshot). Every client with a screen draws a small world-space label from
// UI/Layouts/Character/LogoutBodyTag.layout over each marked body within range, until the body
// dies or leaves. Nothing is saved: the body never outlives its session.

//------------------------------------------------------------------------------------------------
//! The rules, with no world: whether a tag shows, and where.
class LF_LogoutBodyTag
{
	static const string FEATURE = "LogoutBody";

	//! The tag is drawn for a body this near the camera (metres).
	static const float RANGE_METERS = 12;	// lf-fixed: readable range of a small label, not gameplay tuning

	//------------------------------------------------------------------------------------------------
	//! Pure rule: the model of the tag. On for a logged-out body that is still alive; off for a
	//! character a player controls (connected, or rejoined as a new character) and for a corpse.
	static bool IsOn(bool loggedOutFlag, bool alive, bool controlled)
	{
		return loggedOutFlag && alive && !controlled;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: this body gets a label this frame (the tag is on, in range, in front of the camera).
	static bool Draws(bool tagOn, float meters, float depth)
	{
		return tagOn && meters <= RANGE_METERS && depth > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The tag for an entity as it is now. Reads the replicated flag, the life state and the controller.
	static bool IsOnFor(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		bool controlled;
		PlayerManager players = GetGame().GetPlayerManager();
		if (players)
			controlled = players.GetPlayerIdFromControlledEntity(entity) > 0;

		return IsOn(character.LF_IsLoggedOut(), LF_StorageSnapshot.IsAlive(entity), controlled);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put the tag on a body the server keeps, or take it off. A client does nothing.
	//! \return True when the flag now has that value.
	static bool Mark(IEntity body, bool on)
	{
		if (!Replication.IsServer())
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (!character)
		{
			LF_Debug.Warn(FEATURE, "logged-out tag: the body is not a character, no tag");
			return false;
		}

		character.LF_SetLoggedOut(on);
		return character.LF_IsLoggedOut() == on;
	}

	//------------------------------------------------------------------------------------------------
	//! Called on every machine when the flag of a character changes (the replication callback, and
	//! the server's own write). A dedicated server draws nothing.
	static void Sync(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return;

		if (character.LF_IsLoggedOut())
			LF_LogoutBodyTagHud.Track(character);
		else
			LF_LogoutBodyTagHud.Untrack(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset when a world ends.
	static void ResetStatics()
	{
		LF_LogoutBodyTagHud.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The replicated flag. Server-written; a client reads it.
modded class SCR_ChimeraCharacter
{
	[RplProp(onRplName: "LF_OnLoggedOutReplicated")]
	protected bool m_bLF_LoggedOut;

	//------------------------------------------------------------------------------------------------
	void LF_SetLoggedOut(bool on)
	{
		if (!Replication.IsServer() || on == m_bLF_LoggedOut)
			return;

		m_bLF_LoggedOut = on;
		Replication.BumpMe();
		LF_Debug.Info(LF_LogoutBodyTag.FEATURE, string.Format("logged-out tag %1 on %2", on, GetID()));
		LF_LogoutBodyTag.Sync(this);
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsLoggedOut()
	{
		return m_bLF_LoggedOut;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnLoggedOutReplicated()
	{
		LF_LogoutBodyTag.Sync(this);
	}
}

//------------------------------------------------------------------------------------------------
class LF_LogoutBodyTagMark : Managed
{
	EntityID m_Id = EntityID.INVALID;
	Widget m_wRoot;
	Widget m_wTag;
	bool m_bShown;
}

//------------------------------------------------------------------------------------------------
//! The client side: one label per marked body, placed over it every tick while any is tracked.
class LF_LogoutBodyTagHud
{
	static const ResourceName LAYOUT = "{3D39ECFB92713D5F}UI/Layouts/Character/LogoutBodyTag.layout";
	static const int TICK_MS = 33;	// lf-fixed: follows the camera smoothly
	static const float LIFT_METERS = 0.9;	// lf-fixed: floats over a body lying down

	protected static ref array<ref LF_LogoutBodyTagMark> s_aMarks;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_LogoutBodyTagMark> Marks()
	{
		if (!s_aMarks)
			s_aMarks = new array<ref LF_LogoutBodyTagMark>();

		return s_aMarks;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_LogoutBodyTagMark Find(EntityID id)
	{
		foreach (LF_LogoutBodyTagMark mark : Marks())
		{
			if (mark && mark.m_Id == id)
				return mark;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Start labelling this body. Only where something renders; a repeat call changes nothing.
	static void Track(notnull IEntity body)
	{
		if (!GetGame() || RplSession.Mode() == RplMode.Dedicated || Find(body.GetID()))
			return;

		LF_LogoutBodyTagMark mark = new LF_LogoutBodyTagMark();
		mark.m_Id = body.GetID();
		Marks().Insert(mark);
		LF_Debug.Info(LF_LogoutBodyTag.FEATURE, string.Format("tag tracks body %1 (%2 tracked)", mark.m_Id, Marks().Count()));
		StartTick();
	}

	//------------------------------------------------------------------------------------------------
	static void Untrack(notnull IEntity body)
	{
		LF_LogoutBodyTagMark mark = Find(body.GetID());
		if (!mark)
			return;

		Drop(mark);
		LF_Debug.Info(LF_LogoutBodyTag.FEATURE, string.Format("tag released body %1 (%2 tracked)", body.GetID(), Marks().Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Drop(notnull LF_LogoutBodyTagMark mark)
	{
		if (mark.m_wRoot)
			mark.m_wRoot.RemoveFromHierarchy();

		mark.m_wRoot = null;
		mark.m_wTag = null;
		Marks().RemoveItem(mark);
		if (Marks().IsEmpty())
			StopTick();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTick()
	{
		if (s_bTicking || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		s_bTicking = true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTick()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	//! How many labels are on screen now (the harness proof reads it).
	static int ShownCount()
	{
		int shown;
		foreach (LF_LogoutBodyTagMark mark : Marks())
		{
			if (mark && mark.m_bShown)
				shown++;
		}

		return shown;
	}

	//------------------------------------------------------------------------------------------------
	static string Describe()
	{
		return string.Format("tracked %1 shown %2", Marks().Count(), ShownCount());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		if (!workspace || !world)
			return;

		vector eye;
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && cameras.CurrentCamera())
			eye = cameras.CurrentCamera().GetOrigin();

		array<ref LF_LogoutBodyTagMark> gone = {};
		foreach (LF_LogoutBodyTagMark mark : Marks())
		{
			if (!mark)
				continue;

			IEntity body = world.FindEntityByID(mark.m_Id);
			if (!body)
			{
				gone.Insert(mark);
				continue;
			}

			vector spot = body.GetOrigin() + Vector(0, LIFT_METERS, 0);
			vector projected = workspace.ProjWorldToScreen(spot, world);
			bool draws = LF_LogoutBodyTag.Draws(LF_LogoutBodyTag.IsOnFor(body), vector.Distance(eye, spot), projected[2]);
			Show(mark, workspace, draws, projected);
		}

		foreach (LF_LogoutBodyTagMark mark : gone)
			Drop(mark);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show(notnull LF_LogoutBodyTagMark mark, notnull WorkspaceWidget workspace, bool draws, vector projected)
	{
		if (!draws)
		{
			if (mark.m_wRoot)
				mark.m_wRoot.SetVisible(false);

			mark.m_bShown = false;
			return;
		}

		if (!mark.m_wRoot)
		{
			mark.m_wRoot = workspace.CreateWidgets(LAYOUT);
			if (!mark.m_wRoot)
			{
				LF_Debug.Error(LF_LogoutBodyTag.FEATURE, "the logged-out tag layout did not load; the body has no tag: " + LAYOUT);
				return;
			}

			mark.m_wTag = mark.m_wRoot.FindAnyWidget("Tag");
		}

		if (!mark.m_wTag)
			return;

		mark.m_wRoot.SetVisible(true);
		FrameSlot.SetPos(mark.m_wTag, projected[0], projected[1]);
		mark.m_bShown = true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTick();
		if (s_aMarks)
		{
			foreach (LF_LogoutBodyTagMark mark : s_aMarks)
			{
				if (mark && mark.m_wRoot)
					mark.m_wRoot.RemoveFromHierarchy();
			}
		}

		s_aMarks = null;
	}
}

//------------------------------------------------------------------------------------------------
//! logoutbodytag state: what this client tracks and draws (agent-box/proofs/logout-body-tag-proof.mjs).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "logoutbodytag")
			return super.Handle(cmd);

		LF_AgentHarness.Answer(cmd.Id, "ok", LF_LogoutBodyTagHud.Describe());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LogoutBodyTag's statics (registered at script load; Core/LF_StaticReset.c).
class LF_LogoutBodyTag_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_LogoutBodyTag_StaticResetHook());

	override string Owner()
	{
		return "LF_LogoutBodyTag";
	}

	override void Apply()
	{
		LF_LogoutBodyTag.ResetStatics();
	}
}
