//------------------------------------------------------------------------------------------------
//! Playtest test call (LF_SpawnLogic.m_bDebugTestDispatch, on in GameMode_Roleplay_Debug only):
//! one player can place a police or medic call by hand and then take it at the dispatch board,
//! without a second client at a payphone. It goes through the same LF_ServiceCalls.Place a
//! payphone uses, so it needs a receiver on duty for that service (clock in first, then fire
//! the call) and rides the same radio, board and pager paths. Off in the flagship game mode.
class LF_DebugDispatch
{
	static const string FEATURE = "DebugDispatch";

	//! LF_Places kind of a place a test call comes from (LF_PlaceComponent); the id is the place id.
	static const string PLACE_KIND = "DispatchSource";

	//------------------------------------------------------------------------------------------------
	//! Where a test call comes from: the placed DispatchSource placeId, else fallback (the console).
	//! An id that is set but not placed warns and falls back; an empty id is the console by design.
	static vector PlacePosition(string placeId, vector fallback)
	{
		if (placeId.IsEmpty())
			return fallback;

		vector origin;
		if (LF_Places.FindOrigin(PLACE_KIND, placeId, origin))
			return origin;

		LF_Debug.Warn(FEATURE, string.Format("dispatch place '%1' is not placed: calling from the console", placeId));
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: is the test call switched on in this game mode?
	static bool Enabled()
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		return spawnLogic && spawnLogic.IsDebugTestDispatch();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast key for a refusal; empty when the call went out.
	static string RefusalKey(bool enabled, bool known, bool answered)
	{
		if (!enabled)
			return "#LF-Dispatch_TestOff";

		if (!known || !answered)
			return LF_ServiceCalls.NO_ANSWER_KEY;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: place the call. Nothing is charged. \return The toast key to show: empty when the call went out.
	static string Fire(bool enabled, string callerUid, string service, string summary, vector position, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return string.Empty;

		if (!enabled)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 asked for a test %2 call: not enabled in this game mode", callerUid, service));
			return RefusalKey(false, true, true);
		}

		string place = LF_WantedReports.DescribePlace(position);
		LF_ServiceRoute route = LF_ServiceCalls.Place(callerUid, service, place, summary, position, players);
		string refusal = RefusalKey(true, route && route.m_bKnown, route && route.IsAnswered());
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("%1 test %2 call from '%3': nobody on duty to receive it", callerUid, service, place));
			return refusal;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 test %2 call from '%3' placed", callerUid, service, place));
		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! "Test police call" / "Test medic call" on the dispatch console. The client only asks; the
//! server re-checks the debug gate and places the call from m_sPlaceId (a placed DispatchSource,
//! so the call has a place worth driving to) or, when that is empty or missing, from the console.
class LF_DebugDispatchAction : ScriptedUserAction
{
	[Attribute("police", UIWidgets.EditBox, "Service key from Configs/Comms/LF_ServiceCalls.conf")]
	protected string m_sService;

	[Attribute("", UIWidgets.EditBox, "What happened: a localization key sent to the service as the summary")]
	protected string m_sSummary;

	[Attribute("", UIWidgets.EditBox, "Id of the placed DispatchSource (LF_PlaceComponent) the call comes from; empty uses the console's own position")]
	protected string m_sPlaceId;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return user != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		vector position = pOwnerEntity.GetOrigin();
		position = LF_DebugDispatch.PlacePosition(m_sPlaceId, position);

		string refusal = LF_DebugDispatch.Fire(LF_DebugDispatch.Enabled(), LF_Utils.GetPlayerUid(pUserEntity), m_sService, m_sSummary, position);
		if (!refusal.IsEmpty())
		{
			LF_Utils.NotifyEntity(pUserEntity, refusal, "#LF-Dispatch_TestTitle", 4.0);
			return;
		}

		LF_Utils.NotifyEntity(pUserEntity, "#LF-Dispatch_TestSent", "#LF-Dispatch_TestTitle", 4.0);
	}
}
