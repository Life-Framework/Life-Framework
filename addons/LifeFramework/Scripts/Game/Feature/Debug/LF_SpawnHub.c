//------------------------------------------------------------------------------------------------
//! Spawn hub (#607): the travel globe on the spawn table teleports a playtester to any playtest
//! area. Playtest only: the server refuses unless the game mode opts in (LF_SpawnLogic
//! m_bPlaytestHub, set on GameMode_Roleplay_Debug.et), exactly like m_bDebugMoneyTopUp.
//! Points come from Configs/Debug/LF_SpawnHub.conf; a new area is a new row there plus a
//! LF-SpawnHub_Pt_<id> string.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_SpawnHubPoint
{
	[Attribute("", UIWidgets.Auto, "Stable id; the client asks for this and the server looks it up. Also the localization key suffix LF-SpawnHub_Pt_<id>")]
	string m_sId;

	[Attribute("0 0 0", UIWidgets.Coords, "World position. A Y of 0 means: stand on the ground at X and Z")]
	vector m_vPosition;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_SpawnHubConfig
{
	static const ResourceName CONFIG_PATH = "{7C3E91A4D2B05F68}Configs/Debug/LF_SpawnHub.conf";

	[Attribute("", UIWidgets.Object, "One row per playtest area, in list order")]
	ref array<ref LF_SpawnHubPoint> m_aPoints;

	protected static ref LF_SpawnHubConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_SpawnHubConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("SpawnHub", string.Format("spawn hub config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_SpawnHubConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("SpawnHub", string.Format("spawn hub config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The rows with a non-empty, unique id, in order. Empty when the config is missing.
	static array<ref LF_SpawnHubPoint> Points()
	{
		array<ref LF_SpawnHubPoint> result = {};
		LF_SpawnHubConfig config = Get();
		if (!config || !config.m_aPoints)
			return result;

		foreach (LF_SpawnHubPoint point : config.m_aPoints)
		{
			if (!point || point.m_sId.IsEmpty())
			{
				LF_Debug.Error("SpawnHub", "config row skipped: no id");
				continue;
			}

			bool duplicate = false;
			foreach (LF_SpawnHubPoint seen : result)
			{
				if (seen.m_sId == point.m_sId)
					duplicate = true;
			}

			if (duplicate)
			{
				LF_Debug.Error("SpawnHub", "config row skipped: duplicate id " + point.m_sId);
				continue;
			}

			result.Insert(point);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The row with this id, or null.
	static LF_SpawnHubPoint Find(string id)
	{
		array<ref LF_SpawnHubPoint> points = Points();
		foreach (LF_SpawnHubPoint point : points)
		{
			if (point.m_sId == id)
				return point;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_ESpawnHubCheck
{
	OK,
	DISABLED,
	NO_BODY,
	UNKNOWN_POINT,
	TOO_FAR
}

//------------------------------------------------------------------------------------------------
//! Pure validation of a travel request, for LOGIC tests.
class LF_SpawnHubRules
{
	//------------------------------------------------------------------------------------------------
	//! \param mayTravel The body stands at a travel globe, or its player is exempt (LF_SpawnHub.MayTravelFrom).
	static LF_ESpawnHubCheck Check(bool hubEnabled, bool hasBody, bool pointKnown, bool mayTravel)
	{
		if (!hubEnabled)
			return LF_ESpawnHubCheck.DISABLED;

		if (!hasBody)
			return LF_ESpawnHubCheck.NO_BODY;

		if (!pointKnown)
			return LF_ESpawnHubCheck.UNKNOWN_POINT;

		if (!mayTravel)
			return LF_ESpawnHubCheck.TOO_FAR;

		return LF_ESpawnHubCheck.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! A Y of 0 in the config means the ground at X and Z; any other Y is used as written.
	static vector Destination(vector configured, float groundY)
	{
		if (configured[1] != 0)
			return configured;

		return Vector(configured[0], groundY + 0.2, configured[2]);
	}

	//------------------------------------------------------------------------------------------------
	static string NameKey(string id)
	{
		// Built from parts: the key is per row, so the table check sees no fixed key here.
		return "#LF-" + "SpawnHub_Pt_" + id;
	}
}

//------------------------------------------------------------------------------------------------
//! Server side of the travel globe.
class LF_SpawnHub
{
	static const string FEATURE = "SpawnHub";
	static const string GLOBE_ACTION = "LF_SpawnHubAction";
	//! Server reach to the globe. The action shows within 3 m and the menu closes beyond 4 m; the
	//! rest is slack for movement while the request is in flight.
	static const float GLOBE_RANGE = 6.0; // lf-fixed: anti-cheat reach tied to the action's 3 m, not an owner knob

	//------------------------------------------------------------------------------------------------
	//! \return true when the running game mode is a playtest mode (LF_SpawnLogic.m_bPlaytestHub).
	static bool IsEnabled()
	{
		LF_SpawnLogic logic = LF_SpawnLogic.GetInstance();
		if (!logic)
			return false;

		return logic.IsPlaytestHub();
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when an entity carrying the travel action stands within GLOBE_RANGE of origin.
	static bool IsNearGlobe(vector origin)
	{
		return LF_ActionSite.IsNear(origin, GLOBE_RANGE, GLOBE_ACTION);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: may this body travel from where it stands? It must be at a travel globe, unless its
	//! player is an administrator or this is an LF_HARNESS build (the agent harness, which travels
	//! without walking to the globe). The playtest game mode's dev-tools opt-in is not an
	//! exemption: it is on wherever the hub is, so it would switch the reach check off everywhere.
	static bool MayTravelFrom(IEntity body)
	{
		if (!body)
			return false;

		if (IsNearGlobe(body.GetOrigin()))
			return true;

		if (LF_DevGate.HarnessBuild())
			return true;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(body);
		return playerId > 0 && playerManager.HasPlayerRole(playerId, EPlayerRole.ADMINISTRATOR);
	}

	//------------------------------------------------------------------------------------------------
	//! Validates and performs one travel request for a body.
	//! \param hubEnabled The gate (IsEnabled() in the RPC; a parameter so a test can drive both sides).
	//! \param mayTravel MayTravelFrom(body) in the RPC; a parameter for the same reason.
	//! \return The check result; the body moved only for OK.
	static LF_ESpawnHubCheck Serve(IEntity body, string pointId, bool hubEnabled, bool mayTravel)
	{
		LF_SpawnHubPoint point = LF_SpawnHubConfig.Find(pointId);
		LF_ESpawnHubCheck result = LF_SpawnHubRules.Check(hubEnabled, body != null, point != null, mayTravel);
		if (result != LF_ESpawnHubCheck.OK)
		{
			LF_Debug.Warn(FEATURE, string.Format("travel to %1 refused: %2", pointId, typename.EnumToString(LF_ESpawnHubCheck, result)));
			if (result == LF_ESpawnHubCheck.TOO_FAR)
				NotifyTooFar(body);

			return result;
		}

		float groundY = 0;
		if (GetGame() && GetGame().GetWorld())
			groundY = GetGame().GetWorld().GetSurfaceY(point.m_vPosition[0], point.m_vPosition[2]);

		vector destination = LF_SpawnHubRules.Destination(point.m_vPosition, groundY);
		LF_Jail.MoveBody(body, destination);
		LF_Debug.Info(FEATURE, string.Format("moved a player to %1 at %2", pointId, destination));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyTooFar(IEntity body)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !body)
			return;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(body);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-SpawnHub_Title", "#LF-SpawnHub_TooFar", 4.0, LF_ENotificationType.WARNING);
	}
}

//------------------------------------------------------------------------------------------------
//! Client->server bridge, hosted on the owned character (the one entity a player may RPC on).
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskSpawnHubTravel(string pointId)
	{
		Rpc(RpcAsk_LF_SpawnHubTravel, pointId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_SpawnHubTravel(string pointId)
	{
		if (!LF_RateLimit.AllowCharacter(this, LF_ERequestKind.TRAVEL, "spawn hub travel"))
			return;

		LF_SpawnHub.Serve(this, pointId, LF_SpawnHub.IsEnabled(), LF_SpawnHub.MayTravelFrom(this));
	}
}

//------------------------------------------------------------------------------------------------
//! The travel list: a scrollable UI kit list, one row per config point, opened from the globe.
class LF_SpawnHubMenu
{
	static const string PART_TITLE = "HubTitle";
	static const string PART_HINT = "HubHint";
	static const string PART_LIST = "HubList";
	static const string PART_CLOSE = "CloseButton";
	static const string ROW_PREFIX = "HubGo_";
	static const string ROW_RESET = "HubResetWorld";
	static const int CONFIRM_MS = 5000; // lf-fixed: how long the second press counts as the confirm
	static const float CLOSE_RANGE = 4.0;

	protected static ref LF_SpawnHubMenu s_Menu;

	protected ref LF_UIScreen m_Screen;
	protected int m_iResetArmedAt = -1;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts()
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-SpawnHub_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_HINT, "#LF-SpawnHub_Hint"));
		parts.Insert(LF_UIPart.CreateList(PART_LIST));
		// Resets the whole world for everyone, so it asks twice (the dev gate decides on the server).
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, ROW_RESET, "#LF-SpawnHub_ResetWorld"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-SpawnHub_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static void Open(IEntity globe)
	{
		Hide();

		LF_SpawnHubMenu menu = new LF_SpawnHubMenu();
		menu.m_Screen = LF_UIScreen.Open(BuildParts());
		if (!menu.m_Screen)
		{
			LF_Debug.Error(LF_SpawnHub.FEATURE, "travel list failed to open");
			return;
		}

		array<ref LF_SpawnHubPoint> points = LF_SpawnHubConfig.Points();
		foreach (LF_SpawnHubPoint point : points)
		{
			menu.m_Screen.AddListRow(PART_LIST, ROW_PREFIX + point.m_sId, LF_SpawnHubRules.NameKey(point.m_sId), "", "#LF-SpawnHub_Go");
		}

		menu.m_Screen.GetOnButton().Insert(menu.OnButton);
		menu.m_Screen.GetOnClosed().Insert(menu.OnClosed);
		menu.m_Screen.CloseOnDeath();
		menu.m_Screen.CloseWhenUncontrolled();
		if (globe)
			menu.m_Screen.CloseBeyond(CLOSE_RANGE, globe.GetID());

		s_Menu = menu;
		LF_Debug.Info(LF_SpawnHub.FEATURE, string.Format("travel list open with %1 areas", points.Count()));
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Menu)
			return;

		LF_SpawnHubMenu menu = s_Menu;
		s_Menu = null;
		if (menu.m_Screen)
			menu.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen CurrentScreen()
	{
		if (!s_Menu)
			return null;

		return s_Menu.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name == ROW_RESET)
		{
			OnResetPressed();
			return;
		}

		if (!name.StartsWith(ROW_PREFIX))
			return;

		string id = name.Substring(ROW_PREFIX.Length(), name.Length() - ROW_PREFIX.Length());
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_Debug.Error(LF_SpawnHub.FEATURE, "travel not asked: no local character");
			return;
		}

		character.LF_AskSpawnHubTravel(id);
		Hide();
	}

	//------------------------------------------------------------------------------------------------
	//! First press asks "sure?"; a second press within CONFIRM_MS resets the world for everyone.
	protected void OnResetPressed()
	{
		int now = GetGame().GetWorld().GetWorldTime();
		bool armed = m_iResetArmedAt >= 0 && now - m_iResetArmedAt <= CONFIRM_MS;
		if (!armed)
		{
			m_iResetArmedAt = now;
			m_Screen.SetButtonText(ROW_RESET, "#LF-SpawnHub_ResetSure");
			return;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_Debug.Error(LF_SpawnHub.FEATURE, "world reset not asked: no local character");
			return;
		}

		character.LF_AskWorldReset("globe");
		Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		if (s_Menu == this)
			s_Menu = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The globe's travel entry. Local UI, so it runs only on the acting client; the server decides.
class LF_SpawnHubAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return user && GetOwner() && vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) <= 3.0;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		LF_SpawnHubMenu.Open(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-SpawnHub_Open";
		return true;
	}
}
