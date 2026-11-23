//------------------------------------------------------------------------------------------------
//! Placed bird flocks (#4403). A vanilla `SCR_Bird` flock is an AI agent that sleeps with every one of
//! its birds hidden (VISIBLE flag clear). `AIAnimal.ManualActivation()` wakes it: its birds appear at
//! the flock, take off, fly one loop along a movement spline, land, and are hidden again until the
//! next activation. The world's AIBirdManagerComponent wakes the flocks it spawns itself; nothing
//! wakes a flock a layer places, so the pigeons on the square and the gulls on the quay never showed
//! in any picture (neither perched nor flying: the engine has no perched-and-visible state).
//!
//! The server wakes a layer-placed flock when a player's character walks up to it (within
//! WAKE_RADIUS_M, inside the flock's own 50 m danger range): the birds burst up in front of the player.
//! The flock is armed again once every player is farther than REARM_M, with at least MIN_GAP_MS between
//! two wakes, so a player who stands on the square is not showered every minute but one who leaves and
//! comes back sees the birds again.
//! Not at world start: the wake reaches the clients as one broadcast, and a client still loading when
//! it is sent never shows the birds. A character within 40 m has the flock streamed in.
//! Which: a layer-placed flock is a named entity (every row a town fit or a kit writes has a name); the
//! flocks the bird manager spawns at run time are unnamed and stay the manager's. No coordinate,
//! entity name or layer is read: any flock a world places is covered.
class LF_FlockActivation
{
	static const string FEATURE = "Birds";
	static const float WAKE_RADIUS_M = 40; // lf-fixed: inside the flock's own 50 m DangerEventRange
	static const float REARM_M = 80; // lf-fixed: far enough that a walker leaving the square has left it
	static const float MIN_GAP_MS = 45000; // lf-fixed: longer than a flock's flight loop and its 35 s reaction cooldown
	static const int TICK_MS = 1000; // lf-fixed: how often the server looks for a player near a sleeping flock
	static const float NO_PLAYER_M = 100000; // lf-fixed: "nobody in the world" distance

	//! Placed flocks by entity id (never the entity), whether each is armed, and the world time (ms) it was last woken.
	protected static ref array<EntityID> s_aIds = {};
	protected static ref array<bool> s_aArmed = {};
	protected static ref array<float> s_aWokeAt = {};
	protected static bool s_bTicking;
	protected static bool s_bWarned;

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a flock with this entity name is one a layer placed (the bird manager's are unnamed).
	static bool IsPlaced(string entityName)
	{
		return !entityName.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether an armed flock whose nearest player is `nearestM` away, last woken `sinceMs` ago, wakes now.
	static bool ShouldWake(bool armed, float nearestM, float sinceMs)
	{
		return armed && nearestM <= WAKE_RADIUS_M && sinceMs >= MIN_GAP_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a flock whose nearest player is `nearestM` away is armed again.
	static bool Rearms(float nearestM)
	{
		return nearestM > REARM_M;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a placed flock came up; it wakes whenever a player walks up to it.
	static void Register(IEntity flock)
	{
		if (!flock || !IsPlaced(flock.GetName()) || s_aIds.Find(flock.GetID()) != -1)
			return;

		s_aIds.Insert(flock.GetID());
		s_aArmed.Insert(true);
		s_aWokeAt.Insert(-MIN_GAP_MS);
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		BaseWorld world = GetGame().GetWorld();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!world || !playerManager)
			return;

		array<vector> players = {};
		array<int> ids = {};
		playerManager.GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity body = playerManager.GetPlayerControlledEntity(id);
			if (body)
				players.Insert(body.GetOrigin());
		}

		float now = world.GetWorldTime();
		for (int i = s_aIds.Count() - 1; i >= 0; i--)
		{
			SCR_Bird flock = SCR_Bird.Cast(world.FindEntityByID(s_aIds[i]));
			if (!flock)
			{
				s_aIds.Remove(i);
				s_aArmed.Remove(i);
				s_aWokeAt.Remove(i);
				continue;
			}

			float nearest = NO_PLAYER_M;
			foreach (vector at : players)
			{
				nearest = Math.Min(nearest, vector.Distance(at, flock.GetOrigin()));
			}

			if (Rearms(nearest))
				s_aArmed[i] = true;

			if (ShouldWake(s_aArmed[i], nearest, now - s_aWokeAt[i]))
			{
				s_aArmed[i] = false;
				s_aWokeAt[i] = now;
				Wake(flock);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Wake(SCR_Bird flock)
	{
		if (flock.ManualActivation())
		{
			LF_Debug.Info(FEATURE, string.Format("flock %1 woken at %2", flock.GetName(), flock.GetOrigin()));
			return;
		}

		if (s_bWarned)
			return;

		s_bWarned = true;
		LF_Debug.Warn(FEATURE, string.Format("flock %1 at %2 refused its activation (no AIWorld in this world?); further refusals are not logged", flock.GetName(), flock.GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_aIds.Clear();
		s_aArmed.Clear();
		s_aWokeAt.Clear();
		s_bTicking = false;
		s_bWarned = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FlockActivation's statics (LF_StaticReset finds it by reflection).
class LF_FlockActivation_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FlockActivation";
	}

	override void Apply()
	{
		LF_FlockActivation.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_Bird
{
	//------------------------------------------------------------------------------------------------
	override event protected void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (Replication.IsServer())
			LF_FlockActivation.Register(this);
	}
}
