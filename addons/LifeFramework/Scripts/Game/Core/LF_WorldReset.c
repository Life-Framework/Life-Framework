//------------------------------------------------------------------------------------------------
//! The world reset (#11 of the M2 review): in the debug and playtest world, a respawn or a menu
//! button puts the world back as a tester found it, so the next test starts fresh.
//!
//! The service owns no list. A feature that holds world state writes one file,
//! Feature/<Area>/LF_<Area>WorldReset.c, with a step class and a `modded class LF_WorldResetRegistry`
//! that adds it. Enforce Script stacks every modded layer, so the registry is the sum of the files
//! and no central list exists to conflict on.
//!
//! Gates: the mission must be a playtest mode (LF_SpawnHub.IsEnabled, set on the debug game mode
//! prefab, so never on a production server) AND the dev gate must be open for the player.
//! Respawning resets the world only when that player is alone on the server; the menu and globe
//! buttons reset it for everyone, after a confirm.
//------------------------------------------------------------------------------------------------

//! One feature's part of the reset. Stateless beyond what OnStart records. Server only.
class LF_WorldResetStep : Managed
{
	//! The name in the log and in the result line; one lower-case word.
	string GetName()
	{
		return "";
	}

	//! True when the step clears crime state (wanted, cooldowns, reports, evidence, loot). The
	//! harness's `resetcrime` runs only these, so a proof's reset never moves a car or regrows a rock.
	bool IsCrimeState()
	{
		return false;
	}

	//! The world has started: take a snapshot of what the step must restore (vehicle spots).
	void OnStart()
	{
	}

	//! Put it back.
	//! \return How many things were reset or removed.
	int Run()
	{
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The sum of every feature's `modded class LF_WorldResetRegistry`. Each adds its steps in Collect.
class LF_WorldResetRegistry : Managed
{
	void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset
{
	static const string FEATURE = "WorldReset";

	protected static ref array<ref LF_WorldResetStep> s_aSteps;
	protected static int s_iRuns;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aSteps = null;
		s_iRuns = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered step, collected once.
	static array<ref LF_WorldResetStep> Steps()
	{
		if (!s_aSteps)
		{
			s_aSteps = new array<ref LF_WorldResetStep>();
			LF_WorldResetRegistry registry = new LF_WorldResetRegistry();
			registry.Collect(s_aSteps);
		}

		return s_aSteps;
	}

	//------------------------------------------------------------------------------------------------
	//! How many times the world was reset in this session.
	static int RunCount()
	{
		return s_iRuns;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at game start: the steps record what they must restore. Only in a playtest mode.
	static void Start()
	{
		if (!Replication.IsServer() || !LF_SpawnHub.IsEnabled())
			return;

		foreach (LF_WorldResetStep step : Steps())
		{
			step.OnStart();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a respawn resets the world. A playtest mission, the dev gate open for this
	//! player, and nobody else online.
	static bool MayAutoReset(bool playtestMission, bool devGate, int playersOnline)
	{
		return playtestMission && devGate && playersOnline == 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a menu or globe request resets the world for everyone.
	static bool MayRequestReset(bool playtestMission, bool devGate)
	{
		return playtestMission && devGate;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the dev gate for a player id; a harness build opens it as the other harness gates do.
	protected static bool DevGateOpen(int playerId)
	{
		return LF_DevGate.HarnessBuild() || LF_DevGate.IsAllowedForPlayer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player pressed Respawn on the death screen. Resets the world when the gates allow.
	static bool OnRespawn(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		int online = 0;
		if (players)
			online = players.GetPlayerCount();

		if (!MayAutoReset(LF_SpawnHub.IsEnabled(), DevGateOpen(playerId), online))
			return false;

		RunAll(string.Format("respawn of player %1", playerId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a menu or globe request from a body's player. Logs a refusal.
	static bool OnRequest(IEntity requester, string source)
	{
		if (!Replication.IsServer())
			return false;

		int playerId = 0;
		PlayerManager players = GetGame().GetPlayerManager();
		if (players && requester)
			playerId = players.GetPlayerIdFromControlledEntity(requester);

		if (!MayRequestReset(LF_SpawnHub.IsEnabled(), DevGateOpen(playerId)))
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 refused for player %2: not a playtest mission, or dev tools are closed", source, playerId));
			return false;
		}

		RunAll(string.Format("%1 by player %2", source, playerId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: runs every step (or only the crime-state ones). The caller has done the gate; the
	//! harness gates itself.
	//! \return One line, "name=count" per step that ran, for the harness reply and the log.
	static string RunAll(string why, bool crimeOnly = false)
	{
		string result = "";
		int total = 0;
		foreach (LF_WorldResetStep step : Steps())
		{
			if (crimeOnly && !step.IsCrimeState())
				continue;

			int count = step.Run();
			total += count;
			if (!result.IsEmpty())
				result += " ";

			result += string.Format("%1=%2", step.GetName(), count);
			LF_Debug.Info(FEATURE, string.Format("%1: %2 reset", step.GetName(), count));
		}

		s_iRuns++;
		LF_Debug.Info(FEATURE, string.Format("world reset (%1): %2 things, %3", why, total, result));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the loose entities (not carried, not in a container) of the given prefabs.
	static array<IEntity> FindLoose(notnull array<ResourceName> prefabs)
	{
		array<IEntity> loose = {};
		if (!GetGame() || !GetGame().GetWorld())
			return loose;

		array<IEntity> found = LF_WorldResetScan.All();
		foreach (IEntity entity : found)
		{
			if (!entity || entity.GetParent())
				continue;

			EntityPrefabData data = entity.GetPrefabData();
			if (!data)
				continue;

			foreach (ResourceName prefab : prefabs)
			{
				if (!LF_Utils.SamePrefab(data.GetPrefabName(), prefab))
					continue;

				loose.Insert(entity);
				break;
			}
		}

		return loose;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records the placed (world layer) entities of the given prefabs, so a reset leaves them.
	static void RememberPlaced(notnull array<ResourceName> prefabs, notnull array<EntityID> outPlaced)
	{
		outPlaced.Clear();
		array<IEntity> placed = FindLoose(prefabs);
		foreach (IEntity entity : placed)
		{
			outPlaced.Insert(entity.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes every loose entity of the given prefabs that is not in `placed`, the ones the
	//! world started with.
	//! \return How many were removed.
	static int DeleteLoose(notnull array<ResourceName> prefabs, notnull array<EntityID> placed)
	{
		int removed = 0;
		array<IEntity> loose = FindLoose(prefabs);
		foreach (IEntity entity : loose)
		{
			if (placed.Find(entity.GetID()) >= 0)
				continue;

			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			removed++;
		}

		return removed;
	}
}

//------------------------------------------------------------------------------------------------
//! A one-shot sweep of the whole world for the steps that remove or regrow placed things.
class LF_WorldResetScan
{
	protected ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	//! Every entity in the world.
	static array<IEntity> All()
	{
		LF_WorldResetScan scan = new LF_WorldResetScan();
		BaseWorld world = GetGame().GetWorld();
		vector mins, maxs;
		world.GetBoundBox(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		float radius = vector.Distance(mins, maxs) * 0.5 + 100;
		world.QueryEntitiesBySphere(centre, radius, scan.Collect, null, EQueryEntitiesFlags.ALL);
		return scan.m_aFound;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Collect(IEntity entity)
	{
		if (entity)
			m_aFound.Insert(entity);

		return true;
	}
}
