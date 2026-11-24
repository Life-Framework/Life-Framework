//------------------------------------------------------------------------------------------------
//! One hurt resident the server spawned, as the server remembers it. Session-local: a restart
//! clears casualties like it clears the calls on the board.
class LF_Casualty : Managed
{
	//! Server only: never sent over the network.
	EntityID m_Entity = EntityID.INVALID;
	//! The call's caller uid, "casualty:<n>", unique per casualty so its map mark closes alone.
	string m_sCallerUid;
	string m_sSummary;
	vector m_vPosition;
	//! System tick (ms) at which the call leaves the board and the casualty is cleared.
	int m_iExpiresMs;
}

//------------------------------------------------------------------------------------------------
//! Casualty calls (#4827): work for a medic on a quiet server. While at least one medic is on duty,
//! every m_iIntervalSeconds the server picks a row of Configs/Medic/LF_CasualtyCalls.conf by weight,
//! finds a place of that row's kind through LF_Places, spawns a resident character lying hurt near
//! it and places a medic service call with the position (LF_ServiceCalls.Place): the clinic's medic
//! board, the pager and the dispatch map mark all come from that one call path. The casualty is
//! cleared when the call leaves the board (the same expiry). One open casualty per on-duty medic,
//! m_iMaxOpen in all. The character is a server-spawned prefab with an RplComponent, so clients and
//! late joiners see it through normal replication; vanilla replicates the unconscious state.
class LF_CasualtyCalls
{
	static const string FEATURE = "CasualtyCall";
	static const string SERVICE = "medic";
	static const string CALLER_PREFIX = "casualty:";
	//! How often the server checks whether a roll is due.
	static const int CHECK_MS = 10000;
	//! How long clearing waits again while a medic is moving the patient.
	static const int DEFER_MS = 60000;

	protected static ref array<ref LF_Casualty> s_aOpen;
	protected static int s_iSequence;
	//! System tick of the next roll; 0 = the next check rolls.
	protected static int s_iNextDueMs;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ServerTick);
		s_bTicking = false;
		s_aOpen = null;
		s_iSequence = 0;
		s_iNextDueMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start the periodic check (idempotent). Nothing starts while the config is off.
	static void Start()
	{
		if (s_bTicking || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		if (!LF_CasualtyCallConfig.Get().m_bEnabled)
		{
			LF_Debug.Info(FEATURE, "casualty calls are off in the config");
			return;
		}

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(ServerTick, CHECK_MS, true);
		LF_Debug.Info(FEATURE, "casualty calls started");
	}

	//------------------------------------------------------------------------------------------------
	protected static void ServerTick()
	{
		Tick(System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: clear what expired, then roll one casualty when a roll is due and there is room.
	//! \param players The routing candidates (tests); null reads the connected players.
	//! \return The casualty spawned this tick, or null.
	static LF_Casualty Tick(int nowMs, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return null;

		Expire(nowMs);

		LF_CasualtyCallConfig config = LF_CasualtyCallConfig.Get();
		if (!config.m_bEnabled)
			return null;

		if (s_iNextDueMs != 0 && nowMs - s_iNextDueMs < 0)
			return null;

		// No medic: the roll stays due, so the first medic to clock in gets a call at the next check.
		LF_ServiceRoute check = LF_ServiceCalls.Check(SERVICE, players);
		int medics = check.m_aReceivers.Count();
		if (medics <= 0)
			return null;

		s_iNextDueMs = nowMs + LF_Utils.MaxInt(config.m_iIntervalSeconds, 1) * 1000;
		if (s_iNextDueMs == 0)
			s_iNextDueMs = 1;

		int cap = LF_Utils.MinInt(config.m_iMaxOpen, medics * config.m_iPerMedic);
		if (Open().Count() >= cap)
		{
			LF_Debug.Info(FEATURE, string.Format("no new casualty: %1 open, cap %2 for %3 medic(s) on duty", Open().Count(), cap, medics));
			return null;
		}

		return Raise(config, nowMs, players);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pick a row and a place, spawn the casualty and place the call. \return The casualty, or null.
	protected static LF_Casualty Raise(notnull LF_CasualtyCallConfig config, int nowMs, array<ref LF_ServiceCandidate> players)
	{
		IEntity place;
		LF_CasualtyCallRow row = PickRow(config.m_aRows, place);
		if (!row)
		{
			LF_Debug.Warn(FEATURE, string.Format("no casualty: none of the %1 row(s) has a place in this world", config.m_aRows.Count()));
			return null;
		}

		vector position = Spot(place.GetOrigin(), config.m_fSpotRadius);
		IEntity body = SpawnCasualty(config.m_sPrefab, position);
		if (!body)
			return null;

		s_iSequence++;
		LF_Casualty casualty = new LF_Casualty();
		casualty.m_Entity = body.GetID();
		casualty.m_sCallerUid = CALLER_PREFIX + s_iSequence.ToString();
		casualty.m_sSummary = row.m_sSummary;
		casualty.m_vPosition = position;
		casualty.m_iExpiresMs = nowMs + LF_ServiceCallsConfig.BoardExpirySeconds() * 1000;

		LF_ServiceRoute route = LF_ServiceCalls.Place(casualty.m_sCallerUid, SERVICE, LF_WantedReports.GridPlace(position), row.m_sSummary, position, players);
		if (!route || !route.IsAnswered())
		{
			LF_Debug.Warn(FEATURE, string.Format("casualty %1 at %2 got no answer; removed", casualty.m_sCallerUid, row.m_sPlaceKind));
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return null;
		}

		Open().Insert(casualty);
		LF_Debug.Info(FEATURE, string.Format("casualty %1 '%2' at %3 (%4); %5 open", casualty.m_sCallerUid, row.m_sSummary, row.m_sPlaceKind, position, Open().Count()));
		return casualty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: delete the casualties whose call left the board and close their map marks.
	//! \return How many were cleared.
	static int Expire(int nowMs)
	{
		if (!s_aOpen)
			return 0;

		int cleared;
		for (int i = s_aOpen.Count() - 1; i >= 0; i--)
		{
			LF_Casualty casualty = s_aOpen[i];
			if (nowMs - casualty.m_iExpiresMs < 0)
				continue;

			// A medic is moving the patient: never delete a body out of a seat or a carry.
			IEntity body = Body(casualty);
			if (body && (body.GetParent() || LF_Carry.IsSeatedIn(body)))
			{
				casualty.m_iExpiresMs = nowMs + DEFER_MS;
				LF_Debug.Info(FEATURE, string.Format("casualty %1 is being moved; clearing deferred", casualty.m_sCallerUid));
				continue;
			}

			Clear(casualty);
			s_aOpen.Remove(i);
			cleared++;
		}
		return cleared;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: delete every casualty (world reset, tests). \return How many were cleared.
	static int ClearAll()
	{
		if (!s_aOpen)
			return 0;

		int cleared = s_aOpen.Count();
		foreach (LF_Casualty casualty : s_aOpen)
		{
			Clear(casualty);
		}
		s_aOpen.Clear();
		return cleared;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Clear(notnull LF_Casualty casualty)
	{
		IEntity body;
		if (GetGame() && GetGame().GetWorld())
			body = GetGame().GetWorld().FindEntityByID(casualty.m_Entity);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character && character.LF_IsDowned())
			LF_BleedOut.End(character);
		if (body)
			SCR_EntityHelper.DeleteEntityAndChildren(body);

		LF_EventBus.Publish(LF_OnServiceCallClosed.Create(SERVICE, casualty.m_sCallerUid));
		LF_Debug.Info(FEATURE, string.Format("casualty %1 cleared untreated (call expired)", casualty.m_sCallerUid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the open casualties, oldest first.
	static array<ref LF_Casualty> Open()
	{
		if (!s_aOpen)
			s_aOpen = {};
		return s_aOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the live entity of a casualty, or null when it is gone.
	static IEntity Body(notnull LF_Casualty casualty)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;
		return GetGame().GetWorld().FindEntityByID(casualty.m_Entity);
	}

	//------------------------------------------------------------------------------------------------
	//! A row by weight among those with at least one live place, and that place. \return null when none fits.
	protected static LF_CasualtyCallRow PickRow(notnull array<ref LF_CasualtyCallRow> rows, out IEntity place)
	{
		array<LF_CasualtyCallRow> usable = {};
		int total;
		foreach (LF_CasualtyCallRow row : rows)
		{
			if (!row || row.m_iWeight <= 0 || row.m_sPlaceKind.IsEmpty())
				continue;

			array<IEntity> places = {};
			if (LF_Places.FindAll(row.m_sPlaceKind, places) <= 0)
				continue;

			usable.Insert(row);
			total += row.m_iWeight;
		}

		if (usable.IsEmpty())
			return null;

		int roll = Math.RandomInt(0, total);
		LF_CasualtyCallRow picked = usable[usable.Count() - 1];
		foreach (LF_CasualtyCallRow candidate : usable)
		{
			if (roll < candidate.m_iWeight)
			{
				picked = candidate;
				break;
			}
			roll -= candidate.m_iWeight;
		}

		array<IEntity> spots = {};
		if (LF_Places.FindAll(picked.m_sPlaceKind, spots) <= 0)
			return null;

		place = spots[Math.RandomInt(0, spots.Count())];
		return picked;
	}

	//------------------------------------------------------------------------------------------------
	//! A ground point within radius of origin.
	protected static vector Spot(vector origin, float radius)
	{
		vector spot = origin;
		if (radius > 0)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(radius * 0.5, radius);
			spot[0] = origin[0] + Math.Cos(angle) * distance;
			spot[2] = origin[2] + Math.Sin(angle) * distance;
		}

		if (GetGame() && GetGame().GetWorld())
			spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawn the resident and lay them down. \return The character, or null (logged).
	protected static IEntity SpawnCasualty(ResourceName prefab, vector position)
	{
		if (prefab.IsEmpty())
			prefab = LF_CasualtyCallConfig.DEFAULT_PREFAB;

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			LF_Debug.Error(FEATURE, "casualty prefab does not load: " + prefab);
			return null;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(Math.RandomFloat(0, 360), 0, 0), params.Transform);
		params.Transform[3] = position + "0 0.1 0";
		IEntity body = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!body)
		{
			LF_Debug.Error(FEATURE, "casualty prefab did not spawn: " + prefab);
			return null;
		}

		if (!RplComponent.Cast(body.FindComponent(RplComponent)))
			LF_Debug.Error(FEATURE, "casualty prefab has no RplComponent; clients will not see it: " + prefab);

		// The medic's downed state, so revive, carry and the bed take the patient, the inventory is locked
		// while downed and the bleed-out runs: an untreated casualty dies. LF_IsDowned is an RplProp and the
		// unconscious life state is vanilla's, so a late joiner sees the body lying down.
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(body.FindComponent(SCR_CharacterDamageManagerComponent));
		HitZone health;
		if (damage)
			health = damage.GetDefaultHitZone();
		if (!character || !damage || !health)
		{
			LF_Debug.Error(FEATURE, "casualty prefab is not a character with a damage manager: " + prefab);
			SCR_EntityHelper.DeleteEntityAndChildren(body);
			return null;
		}

		damage.ForceUnconsciousness();
		LF_Downed.Down(character, damage, health, null);
		return body;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the server checks for casualty rolls from the world's start.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_CasualtyCalls.Start();
	}
}

//------------------------------------------------------------------------------------------------
//! The world reset clears the calls, so the casualties go with them.
modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_Casualties());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_Casualties : LF_WorldResetStep
{
	override string GetName()
	{
		return "casualties";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		return LF_CasualtyCalls.ClearAll();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CasualtyCalls' statics (LF_StaticReset finds it by reflection).
class LF_CasualtyCalls_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CasualtyCalls";
	}

	override void Apply()
	{
		LF_CasualtyCalls.ResetStatics();
	}
}
