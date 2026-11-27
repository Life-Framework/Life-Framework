//------------------------------------------------------------------------------------------------
//! Harness `seen [radius]` (read-only, runs on the asking client): what THIS client reads of the
//! replicated world, so a proof can judge what a player experiences instead of what the server holds.
//! Replies one line of space-separated tokens:
//!   me=faction/duty/wanted from the replicated account state, cash=carried cash as the client counts it,
//!   downed=D life=L bleed=S the own body's replicated downed flag, life state and bleed-out seconds left (-1 not downed)
//!   veh=<prefab>:d=M:eng=E:lock=L:ign=I:stolen=S  one entry per vehicle within the radius, nearest first, joined by "|"
//!   door=<prefab>:d=M:locked=L                    one entry per LF_LockComponent door within the radius, nearest first
//!   breach=<id>:open=O:yaw=Y                     one entry per breach target within the radius: the replicated open flag and the yaw its swing child stands at here
//!   npc=<name>:d=M:stun=S:up=Y:wear=A+B          one entry per town NPC within the radius: its replicated stun flag, how upright it stands (1 standing, 0 lying) and the clothing in its loadout slots
//!   bags=N                                        loose LF_MoneyBag entities within the radius
//!   evidence=<prefab>:d=M                         one entry per scene-evidence entity (an LF_Evidence* prefab: marker, casing, bag) within the radius, nearest first: the prefab this client's copy was spawned from, so a pack's slot swap is read on the client
//!   chop=P                                        the chop shop's replicated percent, -2 with no yard in range
//!   toast=last toast shown, dispatches=N and lastdispatch=radio calls this client heard
//!   clock=<game hour>/<day seconds>               this client's in-game clock and the day length it was told (day-rate proof, #1303)
//!   (veh entries end :plate=<text>: the registration this client's plates show, "-" when none)
//!   doors=<prefab>:d=M:state=S                   one entry per door (BaseDoorComponent) within the radius, nearest first: its door state here (0 shut, +-1 open)
//!   housesign=<house id>:sold=S                   the nearest house sale sign's replicated owned flag
//!   house=<id>:owned=O:locked=L                   the nearest house's replicated owner (set or not) and lock
//!   siteflag=<hideout>:holder=<name>:flag=N       the nearest site flag's replicated holder name and flag number
//!   hideout=<id>:owner=<name>                     the nearest hideout door's replicated owning organisation name
//! A value is what this client's copy of the entity says; a broken RplProp or a missed BumpMe shows here.
class LF_HarnessSeen
{
	static const float DEFAULT_RADIUS = 12; // lf-fixed: a read-only harness default, not a gameplay value
	static const float MAX_RADIUS = 60; // lf-fixed: harness query cap
	static const int MAX_ENTRIES = 8; // lf-fixed: harness line length cap

	// Query state only: strings and numbers, never an entity.
	protected static vector s_vOrigin;
	protected static EntityID s_SelfId;
	protected static ref array<float> s_aVehicleDist;
	protected static ref array<string> s_aVehicleText;
	protected static ref array<float> s_aDoorDist;
	protected static ref array<string> s_aDoorText;
	protected static ref array<float> s_aNpcDist;
	protected static ref array<string> s_aNpcText;
	protected static ref array<float> s_aBreachDist;
	protected static ref array<string> s_aBreachText;
	protected static ref array<EntityID> s_aBreaches;
	protected static ref array<float> s_aBreachAt;
	protected static ref array<float> s_aEvidenceDist;
	protected static ref array<string> s_aEvidenceText;
	protected static int s_iBags;
	protected static int s_iChop;
	// Late-joiner sweep reads (2026-11-27): every vanilla door's open state, and the nearest house sign,
	// house, site flag and hideout door as this client's copy holds them.
	protected static ref array<float> s_aOpenDoorDist;
	protected static ref array<string> s_aOpenDoorText;
	protected static float s_fHouseSignAt;
	protected static string s_sHouseSign;
	protected static float s_fHouseAt;
	protected static string s_sHouse;
	protected static float s_fSiteFlagAt;
	protected static string s_sSiteFlag;
	protected static float s_fHideoutAt;
	protected static string s_sHideout;

	//------------------------------------------------------------------------------------------------
	//! The query lists are created on the first read and emptied on the later ones: a static `= {}` initializer
	//! here would add twelve to the module's one shared static-init function (AGENTS.md rule 10, #2755).
	protected static void ResetLists()
	{
		if (!s_aVehicleDist)
		{
			s_aVehicleDist = {};
			s_aVehicleText = {};
			s_aDoorDist = {};
			s_aDoorText = {};
			s_aNpcDist = {};
			s_aNpcText = {};
			s_aBreachDist = {};
			s_aBreachText = {};
			s_aBreaches = {};
			s_aBreachAt = {};
			s_aEvidenceDist = {};
			s_aEvidenceText = {};
			s_aOpenDoorDist = {};
			s_aOpenDoorText = {};
			ResetNearest();
			return;
		}

		s_aOpenDoorDist.Clear();
		s_aOpenDoorText.Clear();
		ResetNearest();

		s_aVehicleDist.Clear();
		s_aVehicleText.Clear();
		s_aDoorDist.Clear();
		s_aDoorText.Clear();
		s_aNpcDist.Clear();
		s_aNpcText.Clear();
		s_aBreachDist.Clear();
		s_aBreachText.Clear();
		s_aBreaches.Clear();
		s_aBreachAt.Clear();
		s_aEvidenceDist.Clear();
		s_aEvidenceText.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ResetNearest()
	{
		s_fHouseSignAt = float.MAX;
		s_sHouseSign = "none";
		s_fHouseAt = float.MAX;
		s_sHouse = "none";
		s_fSiteFlagAt = float.MAX;
		s_sSiteFlag = "none";
		s_fHideoutAt = float.MAX;
		s_sHideout = "none";
	}

	//------------------------------------------------------------------------------------------------
	//! A replicated name as one token: spaces become "_", empty is "-".
	protected static string Word(string text)
	{
		if (text.IsEmpty())
			return "-";

		string word = text;
		word.Replace(" ", "_");
		return word;
	}

	//------------------------------------------------------------------------------------------------
	//! The registration this client's plates show (the text the plate apply wrote), "-" when none shows.
	protected static string PlateShown(IEntity vehicle)
	{
		LF_LicensePlateManagerComponent plates = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
		if (!plates || !plates.m_Plates)
			return "-";

		foreach (LF_LicensePlatePointInfo plate : plates.m_Plates)
		{
			if (plate && !plate.GetAppliedText().IsEmpty())
				return Word(plate.GetAppliedText());
		}

		return "-";
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest house sign, house, site flag and hideout door, as this client's copy reads them.
	protected static void CollectNearest(IEntity entity, float at)
	{
		LF_HouseSaleComponent sign = LF_Component<LF_HouseSaleComponent>.Find(entity);
		if (sign && at < s_fHouseSignAt)
		{
			int sold = 0;
			if (sign.IsOwned())
				sold = 1;

			s_fHouseSignAt = at;
			s_sHouseSign = string.Format("%1:sold=%2", Word(sign.GetHouseId()), sold);
		}

		LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(entity);
		if (house && at < s_fHouseAt)
		{
			int hasOwner = 0;
			if (!house.GetOwnerKey().IsEmpty())
				hasOwner = 1;

			int houseLocked = 0;
			if (house.IsHouseLocked())
				houseLocked = 1;

			s_fHouseAt = at;
			s_sHouse = string.Format("%1:owned=%2:locked=%3", Word(house.GetHouseIdentifier()), hasOwner, houseLocked);
		}

		LF_SiteFlagComponent flag = LF_Component<LF_SiteFlagComponent>.Find(entity);
		if (flag && at < s_fSiteFlagAt)
		{
			s_fSiteFlagAt = at;
			s_sSiteFlag = string.Format("%1:holder=%2:flag=%3", Word(flag.GetHideoutId()), Word(flag.GetHolderName()), flag.GetFlag());
		}

		LF_HideoutDoorComponent hideout = LF_Component<LF_HideoutDoorComponent>.Find(entity);
		if (hideout && at < s_fHideoutAt)
		{
			s_fHideoutAt = at;
			s_sHideout = string.Format("%1:owner=%2", Word(hideout.GetHideoutId()), Word(hideout.GetOwnerName()));
		}

		BaseDoorComponent door = LF_Component<BaseDoorComponent>.Find(entity);
		if (door)
		{
			float state = Math.Round(door.GetDoorState() * 100) / 100;
			InsertSorted(s_aOpenDoorDist, s_aOpenDoorText, at, string.Format("%1:d=%2:state=%3", Short(LF_Utils.GetPrefabName(entity)), Math.Round(at * 10) / 10, state));
		}
	}

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
	//! The clothing this machine has in an NPC's loadout slots, as the item prefab file names joined
	//! by "+", or "none". Two clients that draw the same outfit read the same text.
	protected static string Wear(IEntity npc)
	{
		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(npc.FindComponent(EquipedLoadoutStorageComponent));
		if (!storage)
			return "none";

		string wear;
		for (int slot = 0; slot < storage.GetSlotsCount(); slot++)
		{
			IEntity item = storage.Get(slot);
			if (!item)
				continue;

			if (!wear.IsEmpty())
				wear += "+";

			wear += Short(LF_Utils.GetPrefabName(item));
		}

		if (wear.IsEmpty())
			return "none";

		return wear;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectEntity(IEntity entity)
	{
		if (!entity || entity.GetID() == s_SelfId)
			return true;

		string prefab = LF_Utils.GetPrefabName(entity);
		if (prefab.Contains("LF_MoneyBag"))
			s_iBags++;

		if (prefab.Contains("Prefabs/Investigation/LF_Evidence"))
		{
			float evidenceAt = vector.Distance(s_vOrigin, entity.GetOrigin());
			InsertSorted(s_aEvidenceDist, s_aEvidenceText, evidenceAt, string.Format("%1:d=%2", Short(prefab), Math.Round(evidenceAt * 10) / 10));
		}

		LF_NpcBaseEntity npc = LF_NpcBaseEntity.Cast(entity);
		if (npc)
		{
			int stun = 0;
			if (npc.LF_IsStunned())
				stun = 1;

			float npcAt = vector.Distance(s_vOrigin, entity.GetOrigin());
			float up = Math.Round(entity.GetTransformAxis(1)[1] * 100) / 100;
			InsertSorted(s_aNpcDist, s_aNpcText, npcAt, string.Format("%1:d=%2:stun=%3:up=%4:wear=%5", entity.GetName(), Math.Round(npcAt * 10) / 10, stun, up, Wear(entity)));
		}

		LF_BreachTargetComponent breach = LF_Component<LF_BreachTargetComponent>.Find(entity);
		if (breach)
		{
			float breachAt = vector.Distance(s_vOrigin, entity.GetOrigin());
			// Described after the query: DescribeSwing runs its own sphere query.
			s_aBreaches.Insert(entity.GetID());
			s_aBreachAt.Insert(breachAt);
		}

		LF_ChopShopComponent shop = LF_Component<LF_ChopShopComponent>.Find(entity);
		if (shop)
			s_iChop = shop.GetPercent();

		float at = vector.Distance(s_vOrigin, entity.GetOrigin());
		float rounded = Math.Round(at * 10) / 10;
		CollectNearest(entity, at);
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

		InsertSorted(s_aVehicleDist, s_aVehicleText, at, string.Format("%1:d=%2:eng=%3:lock=%4:ign=%5:stolen=%6:plate=%7", Short(prefab), rounded, engineOn, vehicleLocked, ignition, stolen, PlateShown(entity)));
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
		ResetLists();
		s_iBags = 0;
		s_iChop = -2;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, radius, CollectEntity, null, EQueryEntitiesFlags.ALL);
		for (int i = 0; i < s_aBreaches.Count(); i++)
		{
			LF_BreachTargetComponent target = LF_Component<LF_BreachTargetComponent>.Find(GetGame().GetWorld().FindEntityByID(s_aBreaches[i]));
			if (target)
				InsertSorted(s_aBreachDist, s_aBreachText, s_aBreachAt[i], string.Format("%1:%2", target.GetTargetId(), target.DescribeSwing()));
		}

		s_aBreaches.Clear();

		string toast = SCR_PlayerController.LF_GetLastToast();
		toast.Replace(" ", "_");
		string dispatch = SCR_PlayerController.LF_GetLastDispatch();
		dispatch.Replace(" ", "_");
		string seen = string.Format("%1 veh=%2 door=%3 bags=%4 chop=%5 toast=%6 dispatches=%7 lastdispatch=%8 posters=%9", line, Join(s_aVehicleText), Join(s_aDoorText), s_iBags, s_iChop, toast, SCR_PlayerController.LF_GetDispatchCount(), dispatch, LF_WantedPosterBoardComponent.DescribeNearest(s_vOrigin));
		seen = seen + " cases=" + LF_CaseBoardComponent.DescribeNearest(s_vOrigin) + " townboard=" + LF_TownBoardComponent.DescribeNearest(s_vOrigin) + " waypoint=" + LF_Waypoint.Describe(s_vOrigin) + " breach=" + Join(s_aBreachText);
		seen = seen + " npc=" + Join(s_aNpcText) + " evidence=" + Join(s_aEvidenceText) + " clock=" + ClockToken() + " jailclock=" + LF_JailClock.Describe();
		string downedHud = LF_DownedHud.Describe();
		downedHud.Replace(" ", "_");
		seen = seen + " downedhud=" + downedHud;
		return seen + LateJoinTokens();
	}

	//------------------------------------------------------------------------------------------------
	//! The late-joiner sweep tokens (2026-11-27), one statement each: a single long `+` chain is "Formula too complex".
	protected static string LateJoinTokens()
	{
		string tokens = " doors=" + Join(s_aOpenDoorText);
		tokens += " housesign=" + s_sHouseSign;
		tokens += " house=" + s_sHouse;
		tokens += " siteflag=" + s_sSiteFlag;
		tokens += " hideout=" + s_sHideout;
		return tokens;
	}

	//------------------------------------------------------------------------------------------------
	//! This client's in-game clock: `<game hour>/<day seconds>` (the day-rate proof reads it twice), -1/-1 with no manager.
	protected static string ClockToken()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || !world.GetTimeAndWeatherManager())
			return "-1/-1";

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		return string.Format("%1/%2", clock.GetTimeOfTheDay(), clock.GetDayDuration());
	}
}
