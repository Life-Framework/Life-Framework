// LF_DownedPatient.c - a practice patient for the playtest town (server only).
//
// A placed NPC (Prefabs/Medic/LF_DownedPatient.et, carrying LF_DownedPatientComponent) is put in the
// real downed state (`LF_Downed`) so a solo tester can run the whole medic flow: revive, first aid,
// carry, load into the ambulance, put in a bed. Nothing here is a second medic system: the patient is
// downed by lethal damage through the normal hit-zone path and treated with the normal actions.
//
// Playtest only: it does nothing unless the mission is a playtest mode (`LF_SpawnHub.IsEnabled`), so
// the prefab placed on a live map is inert. It never bleeds out (`LF_BleedOut.Advance` skips it). A
// treated patient is put back at its spot and downed again after `m_fPatientResetSeconds`, and the
// world reset (`LF_MedicWorldReset.c`) does the same at once.

enum LF_EPatientStep
{
	NONE,
	ARM,
	RESET,
	REPLACE
}

//------------------------------------------------------------------------------------------------
class LF_DownedPatientComponentClass : ScriptComponentClass
{
}

//! Marks a character prefab as the practice patient. Server: registers itself with LF_DownedPatient.
class LF_DownedPatientComponent : ScriptComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;

		SetEventMask(owner, EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		if (Replication.IsServer())
			LF_DownedPatient.Register(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! One patient the service looks after.
class LF_PatientSlot : Managed
{
	EntityID m_Id;
	vector m_aHome[4];
	ResourceName m_sPrefab;
	bool m_bWasDowned;
	float m_fIdle;
	int m_iArmTries;
}

//------------------------------------------------------------------------------------------------
class LF_DownedPatient
{
	static const string FEATURE = "Medic";
	static const int TICK_MS = 2000; // lf-fixed: engineering tick of the playtest-only patient service, not a game rule
	static const int MAX_ARM_TRIES = 5; // lf-fixed: stops a patient that cannot be downed from retrying forever

	protected static ref array<ref LF_PatientSlot> s_aSlots = {};
	protected static bool s_bTicking;
	protected static int s_iResets;

	//------------------------------------------------------------------------------------------------
	//! Pure: what the service does with one patient this tick.
	//! \param playtest The mission is a playtest mode (the only place the patient exists).
	//! \param present The patient entity is still in the world.
	//! \param dead The patient died for real.
	//! \param downed The patient is in the downed state.
	//! \param wasDowned The patient has been downed since the last reset (so a standing patient has been treated).
	//! \param idle Seconds the patient has stood treated.
	//! \param delay Seconds a treated patient is left alone before it is put back.
	static LF_EPatientStep Decide(bool playtest, bool present, bool dead, bool downed, bool wasDowned, float idle, float delay)
	{
		if (!playtest)
			return LF_EPatientStep.NONE;

		if (!present || dead)
			return LF_EPatientStep.REPLACE;

		if (downed)
			return LF_EPatientStep.NONE;

		if (!wasDowned)
			return LF_EPatientStep.ARM;

		if (idle >= delay)
			return LF_EPatientStep.RESET;

		return LF_EPatientStep.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The practice patient never bleeds out and may be downed though it is not a player.
	static bool IsPatient(IEntity entity)
	{
		if (!entity)
			return false;

		return entity.FindComponent(LF_DownedPatientComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a patient entity came up. Remembers where it stands and starts the tick.
	static void Register(IEntity patient)
	{
		if (!patient || !Replication.IsServer())
			return;

		if (!LF_SpawnHub.IsEnabled())
		{
			LF_Debug.Info(FEATURE, "practice patient left alone: not a playtest mission");
			return;
		}

		foreach (LF_PatientSlot known : s_aSlots)
		{
			if (known.m_Id == patient.GetID())
				return;
		}

		AddSlot(patient);
		if (!s_bTicking && GetGame().GetCallqueue())
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_PatientSlot AddSlot(IEntity patient)
	{
		LF_PatientSlot slot = new LF_PatientSlot();
		slot.m_Id = patient.GetID();
		patient.GetWorldTransform(slot.m_aHome);
		if (patient.GetPrefabData())
			slot.m_sPrefab = patient.GetPrefabData().GetPrefabName();

		s_aSlots.Insert(slot);
		LF_Debug.Info(FEATURE, string.Format("practice patient at %1", slot.m_aHome[3]));
		return slot;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, every TICK_MS: arm, reset or replace each patient as Decide says.
	protected static void Tick()
	{
		float delay = LF_MedicConfig.Get().m_fPatientResetSeconds;
		bool playtest = LF_SpawnHub.IsEnabled();
		for (int i = s_aSlots.Count() - 1; i >= 0; i--)
		{
			LF_PatientSlot slot = s_aSlots[i];
			IEntity patient = GetGame().GetWorld().FindEntityByID(slot.m_Id);
			bool downed = LF_Downed.IsDowned(patient);
			if (downed)
			{
				slot.m_bWasDowned = true;
				slot.m_fIdle = 0;
			}
			else if (slot.m_bWasDowned)
			{
				slot.m_fIdle += TICK_MS * 0.001;
			}

			LF_EPatientStep step = Decide(playtest, patient != null, IsDead(patient), downed, slot.m_bWasDowned, slot.m_fIdle, delay);
			if (step == LF_EPatientStep.ARM)
			{
				if (slot.m_iArmTries < MAX_ARM_TRIES)
					Arm(slot, patient);
			}
			else if (step == LF_EPatientStep.RESET)
			{
				Reset(slot, patient);
			}
			else if (step == LF_EPatientStep.REPLACE)
			{
				Replace(slot, patient);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDead(IEntity patient)
	{
		if (!patient)
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(patient.FindComponent(DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: lethal damage through the normal hit-zone path leaves the patient downed.
	static void Arm(LF_PatientSlot slot, IEntity patient)
	{
		slot.m_iArmTries++;
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
		{
			LF_Debug.Error(FEATURE, "practice patient has no character damage manager");
			slot.m_iArmTries = MAX_ARM_TRIES;
			return;
		}

		if (!LF_Downed.Enabled())
		{
			LF_Debug.Warn(FEATURE, "practice patient not downed: m_bDownedEnabled is off");
			slot.m_iArmTries = MAX_ARM_TRIES;
			return;
		}

		damage.Kill(Instigator.CreateInstigator(null));
		LF_Debug.Info(FEATURE, string.Format("practice patient downed (try %1), downed %2", slot.m_iArmTries, LF_Downed.IsDowned(patient)));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: back to the spot and healed; the tick downs it again once the move has settled.
	//! \return true when the patient was put back.
	static bool Reset(LF_PatientSlot slot, IEntity patient)
	{
		if (!slot || !patient)
			return false;

		if (LF_Downed.IsDowned(patient))
			LF_Downed.Clear(patient);

		LF_Jail.MoveBody(patient, slot.m_aHome[3]);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damage)
			damage.FullHeal();

		slot.m_bWasDowned = false;
		slot.m_fIdle = 0;
		slot.m_iArmTries = 0;
		s_iResets++;
		LF_Debug.Info(FEATURE, "practice patient put back at its spot");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a patient that died or vanished is replaced by a fresh one at its spot.
	protected static void Replace(LF_PatientSlot slot, IEntity dead)
	{
		if (slot.m_sPrefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "practice patient lost and its prefab is unknown");
			return;
		}

		Resource resource = Resource.Load(slot.m_sPrefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(FEATURE, "practice patient prefab does not load: " + slot.m_sPrefab);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = slot.m_aHome[0];
		params.Transform[1] = slot.m_aHome[1];
		params.Transform[2] = slot.m_aHome[2];
		params.Transform[3] = slot.m_aHome[3];
		IEntity fresh = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!fresh)
		{
			LF_Debug.Error(FEATURE, "practice patient could not be respawned");
			return;
		}

		if (dead)
			SCR_EntityHelper.DeleteEntityAndChildren(dead);

		slot.m_Id = fresh.GetID();
		slot.m_bWasDowned = false;
		slot.m_fIdle = 0;
		slot.m_iArmTries = 0;
		s_iResets++;
		LF_Debug.Info(FEATURE, "practice patient replaced at its spot");
	}

	//------------------------------------------------------------------------------------------------
	//! The world reset: every patient is put back at once. \return How many were.
	static int ResetAll()
	{
		int count = 0;
		foreach (LF_PatientSlot slot : s_aSlots)
		{
			IEntity patient = GetGame().GetWorld().FindEntityByID(slot.m_Id);
			if (!patient || IsDead(patient))
			{
				Replace(slot, patient);
				count++;
			}
			else if (Reset(slot, patient))
			{
				count++;
			}
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Stop looking after a patient (a test's stand-in); the entity is left to the caller.
	static void Forget(LF_PatientSlot slot)
	{
		int index = s_aSlots.Find(slot);
		if (index >= 0)
			s_aSlots.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	static int GetResetCount()
	{
		return s_iResets;
	}

	//------------------------------------------------------------------------------------------------
	static int GetSlotCount()
	{
		return s_aSlots.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_aSlots = {};
		s_bTicking = false;
		s_iResets = 0;
	}
}
