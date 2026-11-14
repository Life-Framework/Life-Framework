//------------------------------------------------------------------------------------------------
//! Harness `seen [radius]` (read-only, runs on the asking client): what THIS client reads of the
//! replicated world, so a proof can judge what a player experiences instead of what the server holds.
//! Replies one line of space-separated tokens:
//!   me=faction/duty/wanted from the replicated account state, cash=carried cash as the client counts it,
//!   downed=D life=L bleed=S the own body's replicated downed flag, life state and bleed-out seconds left (-1 not downed)
//!   veh=<prefab>:d=M:eng=E:lock=L:ign=I:stolen=S  one entry per vehicle within the radius, nearest first, joined by "|"
//!   door=<prefab>:d=M:locked=L                    one entry per LF_LockComponent door within the radius, nearest first
//!   npc=<name>:d=M:stun=S:up=Y                  one entry per town NPC within the radius: its replicated stun flag and how upright it stands (1 standing, 0 lying)
//!   bags=N                                        loose LF_MoneyBag entities within the radius
//!   chop=P                                        the chop shop's replicated percent, -2 with no yard in range
//!   toast=last toast shown, dispatches=N and lastdispatch=radio calls this client heard
//! A value is what this client's copy of the entity says; a broken RplProp or a missed BumpMe shows here.
class LF_HarnessSeen
{
	static const float DEFAULT_RADIUS = 12; // lf-fixed: a read-only harness default, not a gameplay value
	static const float MAX_RADIUS = 60; // lf-fixed: harness query cap
	static const int MAX_ENTRIES = 8; // lf-fixed: harness line length cap

	// Query state only: strings and numbers, never an entity.
	protected static vector s_vOrigin;
	protected static EntityID s_SelfId;
	protected static ref array<float> s_aVehicleDist = {};
	protected static ref array<string> s_aVehicleText = {};
	protected static ref array<float> s_aDoorDist = {};
	protected static ref array<string> s_aDoorText = {};
	protected static ref array<float> s_aNpcDist = {};
	protected static ref array<string> s_aNpcText = {};
	protected static int s_iBags;
	protected static int s_iChop;

	//------------------------------------------------------------------------------------------------
	static string Short(string prefab)
	{
		array<string> parts = {};
		prefab.Split("/", parts, true);
		if (parts.IsEmpty())
			return "none";

		string name = parts[parts.Count() - 1];
		int dot = name.IndexOf(".");
		if (dot > 0)
			name = name.Substring(0, dot);

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the entries nearest first, so a proof judges the first one it stands beside.
	protected static void InsertSorted(array<float> dists, array<string> texts, float dist, string text)
	{
		int at = dists.Count();
		for (int i = 0; i < dists.Count(); i++)
		{
			if (dist < dists[i])
			{
				at = i;
				break;
			}
		}

		dists.InsertAt(dist, at);
		texts.InsertAt(text, at);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Join(array<string> texts)
	{
		if (texts.IsEmpty())
			return "none";

		string joined;
		for (int i = 0; i < texts.Count() && i < MAX_ENTRIES; i++)
		{
			if (!joined.IsEmpty())
				joined += "|";

			joined += texts[i];
		}

		return joined;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectEntity(IEntity entity)
	{
		if (!entity || entity.GetID() == s_SelfId)
			return true;

		string prefab = LF_Utils.GetPrefabName(entity);
		if (prefab.Contains("LF_MoneyBag"))
			s_iBags++;

		LF_NpcBaseEntity npc = LF_NpcBaseEntity.Cast(entity);
		if (npc)
		{
			int stun = 0;
			if (npc.LF_IsStunned())
				stun = 1;

			float npcAt = vector.Distance(s_vOrigin, entity.GetOrigin());
			float up = Math.Round(entity.GetTransformAxis(1)[1] * 100) / 100;
			InsertSorted(s_aNpcDist, s_aNpcText, npcAt, string.Format("%1:d=%2:stun=%3:up=%4", entity.GetName(), Math.Round(npcAt * 10) / 10, stun, up));
		}

		LF_ChopShopComponent shop = LF_Component<LF_ChopShopComponent>.Find(entity);
		if (shop)
			s_iChop = shop.GetPercent();

		float at = vector.Distance(s_vOrigin, entity.GetOrigin());
		float rounded = Math.Round(at * 10) / 10;
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(entity);
		if (doorLock)
		{
			int locked = 0;
			if (doorLock.IsLocked())
				locked = 1;

			InsertSorted(s_aDoorDist, s_aDoorText, at, string.Format("%1:d=%2:locked=%3", Short(prefab), rounded, locked));
		}

		if (!entity.IsInherited(Vehicle))
			return true;

		int engineOn = 0;
		VehicleControllerComponent engine = LF_Component<VehicleControllerComponent>.Find(entity);
		if (engine && engine.IsEngineOn())
			engineOn = 1;

		int vehicleLocked = 0;
		int ignition = 0;
		int stolen = 0;
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(entity);
		if (vehicleLock)
		{
			if (vehicleLock.IsVehicleLocked())
				vehicleLocked = 1;
			if (vehicleLock.IsIgnitionLocked())
				ignition = 1;
			if (vehicleLock.IsStolen())
				stolen = 1;
		}

		InsertSorted(s_aVehicleDist, s_aVehicleText, at, string.Format("%1:d=%2:eng=%3:lock=%4:ign=%5:stolen=%6", Short(prefab), rounded, engineOn, vehicleLocked, ignition, stolen));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string Read(float radius)
	{
		IEntity me = LF_Utils.GetLocalCharacter();
		if (!me)
			return "no_character";

		if (radius <= 0)
			radius = DEFAULT_RADIUS;
		if (radius > MAX_RADIUS)
			radius = MAX_RADIUS;

		int state = LF_AccountReplication.GetState(me);
		int duty = 0;
		if (LF_AccountStateBits.IsOnDuty(state))
			duty = 1;

		int downed = 0;
		if (LF_Downed.IsDowned(me))
			downed = 1;

		int bleed = -1;
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(me);
		if (chimera)
			bleed = chimera.LF_GetBleedSecondsLeft();

		string line = string.Format("me=%1/%2/%3 cash=%4 downed=%5 life=%6 bleed=%7", typename.EnumToString(LF_Faction, LF_AccountStateBits.GetFaction(state)), duty, LF_AccountStateBits.GetWantedLevel(state), LF_MoneyUtils.GetCash(me), downed, LF_AgentHarness.LifeStateToken(me), bleed);

		s_vOrigin = me.GetOrigin();
		s_SelfId = me.GetID();
		s_aVehicleDist.Clear();
		s_aVehicleText.Clear();
		s_aDoorDist.Clear();
		s_aDoorText.Clear();
		s_aNpcDist.Clear();
		s_aNpcText.Clear();
		s_iBags = 0;
		s_iChop = -2;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, radius, CollectEntity, null, EQueryEntitiesFlags.ALL);

		string toast = SCR_PlayerController.LF_GetLastToast();
		toast.Replace(" ", "_");
		string dispatch = SCR_PlayerController.LF_GetLastDispatch();
		dispatch.Replace(" ", "_");
		string seen = string.Format("%1 veh=%2 door=%3 bags=%4 chop=%5 toast=%6 dispatches=%7 lastdispatch=%8 posters=%9", line, Join(s_aVehicleText), Join(s_aDoorText), s_iBags, s_iChop, toast, SCR_PlayerController.LF_GetDispatchCount(), dispatch, LF_WantedPosterBoardComponent.DescribeNearest(s_vOrigin));
		return seen + " cases=" + LF_CaseBoardComponent.DescribeNearest(s_vOrigin) + " npc=" + Join(s_aNpcText);
	}
}
