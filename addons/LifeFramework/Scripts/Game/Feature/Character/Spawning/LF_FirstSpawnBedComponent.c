// LF_FirstSpawnBedComponent.c — marks a bed a brand-new character wakes in on its first spawn
// (#1965, chapter Story: The relocated #1015; first spawn only, #1758). Put it on a bed prop
// (Prefabs/Medic/LF_FirstSpawnBed.et). Beds register by EntityID and are re-resolved on every use.
// This list is separate from the hospital wards: a first-spawn bed is never a hospital respawn
// point and a hospital bed is never a first-spawn bed. With no such bed placed, the normal spawn applies.

[ComponentEditorProps(category: "LifeFramework/Respawn", description: "A bed a new character wakes in on its first spawn")]
class LF_FirstSpawnBedComponentClass : ScriptComponentClass
{
}

class LF_FirstSpawnBedComponent : ScriptComponent
{
	[Attribute("0 0 0", UIWidgets.EditBox, "Where the waking body stands, relative to the bed origin (bed space, metres)")]
	protected vector m_vWakeOffset;

	[Attribute("0", UIWidgets.EditBox, "Yaw of the waking body relative to the bed, degrees")]
	protected float m_fWakeYaw;

	[Attribute("0.85 0 0", UIWidgets.EditBox, "Where the woken body steps down to beside the bed, relative to the bed origin (bed space, metres)")]
	protected vector m_vStandOffset;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		LF_FirstSpawnBeds.Register(owner.GetID());
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		LF_FirstSpawnBeds.Unregister(owner.GetID());
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! World transform the waking body takes: the bed origin plus the offset, facing the bed's yaw
	//! plus the configured yaw.
	void GetWakeTransform(out vector transform[4])
	{
		IEntity owner = GetOwner();
		vector bed[4];
		owner.GetWorldTransform(bed);

		vector angles = owner.GetYawPitchRoll();
		Math3D.AnglesToMatrix(Vector(angles[0] + m_fWakeYaw, 0, 0), transform);
		transform[3] = bed[3] + m_vWakeOffset.Multiply3(bed);
	}

	//------------------------------------------------------------------------------------------------
	//! World position on the floor beside the bed where the woken body ends up.
	vector GetStandPosition()
	{
		vector bed[4];
		GetOwner().GetWorldTransform(bed);
		return bed[3] + m_vStandOffset.Multiply3(bed);
	}
}

//------------------------------------------------------------------------------------------------
//! The first-spawn bed list and the pick (#1965). Server only: LF_SpawnLogic asks it for a spawn
//! position while a character's first-spawn mark (LF_FirstSpawn, #1964) is still pending, so a
//! respawn after a death, a rejoin or a restart never lands here.
class LF_FirstSpawnBeds
{
	static const string FEATURE = "FirstSpawnBed";
	//! A living character this close (metres, horizontal) to a bed's wake point has taken the bed.
	static const float BED_CLEARANCE = 1.0;
	//! Delay before the owner is told to wake: ownership of the new body has to reach the client.
	static const int WAKE_NOTIFY_DELAY_MS = 1000; // lf-fixed: engine handover timing, the hospital wake uses the same

	protected static ref array<EntityID> s_aBeds;
	protected static ref map<int, bool> s_mWokeInBed;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBeds = null;
		s_mWokeInBed = null;
	}

	//------------------------------------------------------------------------------------------------
	static void Register(EntityID id)
	{
		if (!s_aBeds)
			s_aBeds = new array<EntityID>();

		if (s_aBeds.Find(id) < 0)
			s_aBeds.Insert(id);
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(EntityID id)
	{
		if (s_aBeds)
			s_aBeds.RemoveItem(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Every live first-spawn bed in the world, in registration order. Stale ids are skipped.
	static void GetBeds(notnull array<LF_FirstSpawnBedComponent> outBeds)
	{
		outBeds.Clear();
		if (!s_aBeds)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		foreach (EntityID id : s_aBeds)
		{
			IEntity entity = world.FindEntityByID(id);
			if (!entity)
				continue;

			LF_FirstSpawnBedComponent bed = LF_FirstSpawnBedComponent.Cast(entity.FindComponent(LF_FirstSpawnBedComponent));
			if (bed)
				outBeds.Insert(bed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The first bed no one lies in.
	//! \param beds Wake position of each bed.
	//! \param occupants Positions of living characters.
	//! \return Index into beds, or -1 when there is no bed or every bed is taken.
	static int PickFree(array<vector> beds, array<vector> occupants)
	{
		if (!beds)
			return -1;

		for (int i = 0; i < beds.Count(); i++)
		{
			if (!IsTaken(beds[i], occupants))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsTaken(vector bed, array<vector> occupants)
	{
		if (!occupants)
			return false;

		foreach (vector occupant : occupants)
		{
			if (vector.DistanceSqXZ(bed, occupant) < BED_CLEARANCE * BED_CLEARANCE && Math.AbsFloat(bed[1] - occupant[1]) < 2)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.GetCreationPosition: a character whose first spawn is still pending
	//! spawns on a free first-spawn bed.
	//! \param occupants Positions that already take a bed; null reads the living characters.
	//! \return False when this is not a first spawn, no first-spawn bed is placed or every one is
	//!         taken: the caller takes the normal spawn.
	static bool Resolve(int playerId, string characterId, out vector position, out vector yawPitchRoll, array<vector> occupants = null)
	{
		if (!LF_FirstSpawn.IsPending(characterId))
			return false;

		array<LF_FirstSpawnBedComponent> beds = {};
		GetBeds(beds);
		if (beds.IsEmpty())
			return false;

		array<vector> positions = {};
		foreach (LF_FirstSpawnBedComponent bed : beds)
		{
			vector wake[4];
			bed.GetWakeTransform(wake);
			positions.Insert(wake[3]);
		}

		array<vector> living = {};
		if (occupants)
			living.Copy(occupants);
		else
			CollectLivingCharacters(living);

		int index = PickFree(positions, living);
		if (index < 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("every first-spawn bed is taken; player %1 takes the normal spawn", playerId));
			return false;
		}

		vector transform[4];
		beds[index].GetWakeTransform(transform);
		position = transform[3];
		yawPitchRoll = Math3D.MatrixToAngles(transform);

		if (!s_mWokeInBed)
			s_mWokeInBed = new map<int, bool>();
		s_mWokeInBed.Set(playerId, true);

		LF_Debug.Info(FEATURE, string.Format("player %1 wakes in first-spawn bed %2 of %3 at %4", playerId, index + 1, beds.Count(), position));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectLivingCharacters(notnull array<vector> outPositions)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			if (!controlled)
				continue;

			CharacterControllerComponent controller = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
			if (controller && controller.GetLifeState() == ECharacterLifeState.DEAD)
				continue;

			outPositions.Insert(controlled.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.OnFirstSpawn: a body that woke in a bed tells its owner to play
	//! the get-out-of-bed animation (the hospital wake with no bill). Any other first spawn does nothing.
	static void OnFirstSpawn(int playerId, IEntity character)
	{
		bool inBed;
		if (s_mWokeInBed)
		{
			inBed = s_mWokeInBed.Contains(playerId);
			s_mWokeInBed.Remove(playerId);
		}

		if (!inBed || !character)
			return;

		int loiterType;
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		if (config)
			loiterType = config.m_iWakeLoiterType;

		GetGame().GetCallqueue().CallLater(NotifyOwner, WAKE_NOTIFY_DELAY_MS, false, character, loiterType);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyOwner(IEntity character, int loiterType)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(character);
		if (!body)
		{
			LF_Debug.Warn(FEATURE, "woken body is gone before the owner could wake up");
			return;
		}

		body.LF_SendHospitalWake(loiterType, 0, 0, "", 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The floor position beside the nearest first-spawn bed to a body that woke on it.
	//! \return False when no first-spawn bed is within maxDistance.
	static bool FindStandPosition(vector position, float maxDistance, out vector stand)
	{
		array<LF_FirstSpawnBedComponent> beds = {};
		GetBeds(beds);
		float best = maxDistance * maxDistance;
		bool found;
		foreach (LF_FirstSpawnBedComponent bed : beds)
		{
			vector transform[4];
			bed.GetWakeTransform(transform);
			float distance = vector.DistanceSqXZ(transform[3], position);
			if (distance <= best)
			{
				best = distance;
				stand = bed.GetStandPosition();
				found = true;
			}
		}

		return found;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FirstSpawnBeds' statics (registered at script load; Core/LF_StaticReset.c).
class LF_FirstSpawnBeds_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_FirstSpawnBeds_StaticResetHook());

	override string Owner()
	{
		return "LF_FirstSpawnBeds";
	}

	override void Apply()
	{
		LF_FirstSpawnBeds.ResetStatics();
	}
}
