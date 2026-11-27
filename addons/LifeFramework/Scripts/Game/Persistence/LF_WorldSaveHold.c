//------------------------------------------------------------------------------------------------
//! Keeps session NPCs and their AI out of the engine's world save (walker-group-restart, 2026-11-27).
//! Our config inherits the vanilla Common.conf, which saves every Character_Base character (AIUnit),
//! every AIGroup and every AIWaypoint. The systems that spawn our NPCs (residents, event scenes, the
//! evidence convoy, the laundering buyer, casualties, workplace hands) put them out again from config
//! at every boot, so a saved copy only does harm:
//! - a restart restored every resident beside the one the spawner places (`register refused:
//!   Townsfolk 'cafe_owner' is already placed`, two of each townsfolk standing on the square);
//! - a saved SCR_AIGroup (Group_Base of a walk) VMEs on load: SCR_AIGroupSerializer.Deserialize
//!   calls GetGame().GetFactionManager().GetFactionByKey with no null check, and the island game
//!   mode has no faction manager. Its callsign serializer reads SCR_CallsignManagerComponent unchecked
//!   too (docs/debug-log-triage.md "Game mode components vanilla reads").
//! Hold takes an entity and its child entities (worn clothing, carried items) out of the save; the
//! engine may register an entity lazily after the spawn, so every held entity is held again right
//! before each save, as LF_VehicleWorldSave does for session cars. Server only.
class LF_WorldSaveHold
{
	static const string FEATURE = "Persistence";

	//! Every entity held this session, re-held before each save. Dead ids are dropped then.
	protected static ref array<EntityID> s_aHeld;
	protected static bool s_bSubscribed;

	//------------------------------------------------------------------------------------------------
	//! `entity` and its children leave the world save; a record they had is removed at the next save.
	//! \return True when the entity is not tracked afterwards (also when the world has no persistence system).
	static bool Hold(IEntity entity)
	{
		if (!entity)
			return false;

		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!system)
			return true;

		Remember(entity.GetID());
		Untrack(system, entity);
		if (system.IsTracked(entity))
		{
			LF_Debug.Error(FEATURE, string.Format("%1 is still in the world save: a restart would put back a second copy", LF_Utils.GetPrefabName(entity)));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the engine's world save writes this entity.
	static bool IsSaved(IEntity entity)
	{
		if (!entity)
			return false;

		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!system)
			return false;

		return system.IsTracked(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops tracking `entity` and every child below it. \return How many were tracked.
	protected static int Untrack(notnull PersistenceSystem system, notnull IEntity entity)
	{
		int taken;
		if (system.IsTracked(entity))
		{
			system.StopTracking(entity, true);
			taken++;
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			taken += Untrack(system, child);
			child = child.GetSibling();
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remember(EntityID id)
	{
		if (!s_aHeld)
			s_aHeld = new array<EntityID>();

		if (s_aHeld.Find(id) < 0)
			s_aHeld.Insert(id);

		if (s_bSubscribed)
			return;

		SCR_PersistenceSystem system = SCR_PersistenceSystem.GetScriptedInstance();
		if (!system)
			return;

		system.GetOnBeforeSave().Insert(OnBeforeSave);
		s_bSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnBeforeSave(ESaveGameType saveType)
	{
		HoldAgain();
	}

	//------------------------------------------------------------------------------------------------
	//! Holds every remembered entity that still exists again, so one the engine registered after its
	//! spawn (or a child added since, such as a garment) is not written. Public so a test drives it.
	//! \return How many entities had been tracked again and were taken out.
	static int HoldAgain()
	{
		PersistenceSystem system = PersistenceSystem.GetInstance();
		if (!s_aHeld || !system || !GetGame() || !GetGame().GetWorld())
			return 0;

		int retaken;
		for (int i = s_aHeld.Count() - 1; i >= 0; i--)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(s_aHeld[i]);
			if (!entity)
			{
				s_aHeld.Remove(i);
				continue;
			}

			retaken += Untrack(system, entity);
		}

		if (retaken > 0)
			LF_Debug.Info(FEATURE, string.Format("before the save: %1 session NPC or AI entit(ies) the engine had registered late were taken out of the world save again", retaken));

		return retaken;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bSubscribed)
		{
			SCR_PersistenceSystem system = SCR_PersistenceSystem.GetScriptedInstance();
			if (system)
				system.GetOnBeforeSave().Remove(OnBeforeSave);
		}

		s_bSubscribed = false;
		s_aHeld = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_WorldSaveHold's statics (LF_StaticReset finds it by reflection).
class LF_WorldSaveHold_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_WorldSaveHold";
	}

	override void Apply()
	{
		LF_WorldSaveHold.ResetStatics();
	}
}
