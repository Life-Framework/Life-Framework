// LF_KnownNames.c — names you have been told, shown only up close (decision 2026-11-08 "Names
// show up close, after an introduction"). Client-only. The server sends a name when someone
// introduces themselves to you and again after each spawn (LF_PlayerInteraction.SyncNames).
// Every TICK_MS the local camera looks for a known player within NAME_RANGE metres and inside a
// narrow cone; that one name shows under the crosshair. Strangers show nothing, and no name ever
// floats over a crowd.

class LF_KnownNames
{
	static const float NAME_RANGE = 4.0;
	//! cos(15 degrees): you have to look at them, not near them.
	static const float LOOK_COS = 0.966;
	static const float HEAD_HEIGHT = 1.6;
	static const int TICK_MS = 200;
	static const ResourceName LAYOUT = "{6C1E5A3F9B2D4E71}UI/Layouts/HUD/LF_KnownName.layout";

	protected static ref map<int, string> s_mNames;
	protected static bool s_bTicking;
	protected static Widget s_wRoot;
	protected static TextWidget s_wName;
	protected static string s_sShown;

	//------------------------------------------------------------------------------------------------
	//! Pure rule: is a known person at this distance and view angle close enough to name?
	static bool ShouldShow(float distance, float cosAngle)
	{
		if (distance > NAME_RANGE)
			return false;

		return cosAngle >= LOOK_COS;
	}

	//------------------------------------------------------------------------------------------------
	static void Learn(int playerId, string name)
	{
		if (playerId <= 0 || name.IsEmpty())
		{
			EL_Debug.Warn("Interaction", "name refused: empty player id or name");
			return;
		}

		if (!s_mNames)
			s_mNames = new map<int, string>();

		s_mNames.Set(playerId, name);
		EL_Debug.Info("Interaction", string.Format("this client knows player %1 (%2 known)", playerId, s_mNames.Count()));
		StartTick();
	}

	//------------------------------------------------------------------------------------------------
	static int KnownCount()
	{
		if (!s_mNames)
			return 0;

		return s_mNames.Count();
	}

	//------------------------------------------------------------------------------------------------
	static string NameOf(int playerId)
	{
		if (!s_mNames)
			return string.Empty;

		return s_mNames.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The name on screen now, or empty.
	static string GetShown()
	{
		return s_sShown;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		s_mNames = null;
		s_sShown = string.Empty;
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_wName = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTick()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		Show(FindLookedAt());
	}

	//------------------------------------------------------------------------------------------------
	//! The known name of the player the local camera looks at, or empty.
	protected static string FindLookedAt()
	{
		if (!s_mNames || s_mNames.IsEmpty())
			return string.Empty;

		SCR_ChimeraCharacter self = EL_Utils.GetLocalCharacter();
		if (!self || !LF_PlayerInteraction.IsAlive(self))
			return string.Empty;

		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras)
			return string.Empty;

		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return string.Empty;

		vector view[4];
		camera.GetWorldCameraTransform(view);
		vector eye = view[3];
		vector forward = view[2];

		PlayerManager players = GetGame().GetPlayerManager();
		string best = string.Empty;
		float bestCos = -1;
		foreach (int playerId, string name : s_mNames)
		{
			IEntity other = players.GetPlayerControlledEntity(playerId);
			if (!other || other == self)
				continue;

			vector toHead = other.GetOrigin() + Vector(0, HEAD_HEIGHT, 0) - eye;
			float distance = toHead.Length();
			if (distance <= 0.01)
				continue;

			float cosAngle = vector.Dot(forward, toHead * (1.0 / distance));
			if (!ShouldShow(distance, cosAngle) || cosAngle <= bestCos)
				continue;

			best = name;
			bestCos = cosAngle;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show(string name)
	{
		if (name == s_sShown)
			return;

		s_sShown = name;
		if (!s_wRoot)
		{
			WorkspaceWidget workspace = GetGame().GetWorkspace();
			if (!workspace)
				return;

			s_wRoot = workspace.CreateWidgets(LAYOUT);
			if (!s_wRoot)
			{
				EL_Debug.Error("Interaction", "known name label failed to load " + LAYOUT);
				return;
			}

			s_wName = TextWidget.Cast(s_wRoot.FindAnyWidget("Name"));
		}

		if (!s_wName)
			return;

		s_wName.SetText(name);
		s_wRoot.SetVisible(!name.IsEmpty());
	}
}
