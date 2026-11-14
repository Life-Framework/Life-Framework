//------------------------------------------------------------------------------------------------
//! The generic workplace (#1111). One core prefab, `Prefabs/Jobs/LF_Workplace.et`, carries this
//! component, a punch clock with the clock-in action, and an optional NPC slot, so a pack makes a
//! workplace by inheriting the prefab and setting the job id. No script, no hand-wired actions.
//!
//!  - Job id: the track id (LF_JobTrackDef.m_sJob), for example POLICE or MEDIC. The clock-in
//!    action (LF_DutyAction) reads the duty role from it, so one prefab serves every duty job.
//!  - Place: the workplace registers in LF_Places under PLACE_KIND and its place id (the entity
//!    name when the field is empty), so a system finds it by `LF_Places.Find`, never by name.
//!  - NPC slot: a prefab spawned beside the desk on the server (a clerk, a dispatcher). Empty
//!    means no NPC.
//------------------------------------------------------------------------------------------------
class LF_Workplace
{
	static const string FEATURE = "Workplace";

	//! LF_Places kind of a placed workplace.
	static const string PLACE_KIND = "Workplace";

	static const string JOB_POLICE = "POLICE";
	static const string JOB_MEDIC = "MEDIC";

	//------------------------------------------------------------------------------------------------
	//! The duty role a job id clocks in as. Only the duty jobs have one; any other job id is a
	//! workplace that registers its place and shows no clock-in.
	//! \return true and role set when job is a duty job.
	static bool RoleFor(string job, out LF_Faction role)
	{
		if (job == JOB_POLICE)
		{
			role = LF_Faction.POLICE;
			return true;
		}

		if (job == JOB_MEDIC)
		{
			role = LF_Faction.MEDIC;
			return true;
		}

		return false;
	}

	//! How far from a workplace its punch clock may stand, metres. The clock is a separate world
	//! entity placed beside the desk (a prefab child has no parent at runtime).
	static const float CLOCK_RANGE = 3;

	protected static vector s_vOrigin;
	protected static float s_fBestDist;
	protected static LF_WorkplaceComponent s_pFound;

	//------------------------------------------------------------------------------------------------
	//! The workplace an entity belongs to: its own component, one on an ancestor, or the nearest
	//! workplace within CLOCK_RANGE (the punch clock beside the desk). Null when there is none.
	static LF_WorkplaceComponent FindFor(IEntity entity)
	{
		IEntity cursor = entity;
		while (cursor)
		{
			LF_WorkplaceComponent own = LF_WorkplaceComponent.Cast(cursor.FindComponent(LF_WorkplaceComponent));
			if (own)
				return own;

			cursor = cursor.GetParent();
		}

		if (!entity || !GetGame() || !GetGame().GetWorld())
			return null;

		s_pFound = null;
		s_vOrigin = entity.GetOrigin();
		s_fBestDist = CLOCK_RANGE;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, CLOCK_RANGE, CollectWorkplace, null, EQueryEntitiesFlags.ALL);

		LF_WorkplaceComponent found = s_pFound;
		s_pFound = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectWorkplace(IEntity entity)
	{
		if (!entity)
			return true;

		float dist = vector.Distance(entity.GetOrigin(), s_vOrigin);
		if (dist > s_fBestDist)
			return true;

		LF_WorkplaceComponent workplace = LF_WorkplaceComponent.Cast(entity.FindComponent(LF_WorkplaceComponent));
		if (!workplace)
			return true;

		s_pFound = workplace;
		s_fBestDist = dist;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every placed workplace of a job, appended to into. \return how many were added.
	static int FindAll(string job, notnull array<IEntity> into)
	{
		array<IEntity> all = {};
		LF_Places.FindAll(PLACE_KIND, all);

		int added = 0;
		foreach (IEntity entity : all)
		{
			LF_WorkplaceComponent workplace = LF_WorkplaceComponent.Cast(entity.FindComponent(LF_WorkplaceComponent));
			if (workplace && workplace.GetJob() == job)
			{
				into.Insert(entity);
				added++;
			}
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "A workplace for a job: place registration, the clock-in role and an optional NPC.")]
class LF_WorkplaceComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_WorkplaceComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Job id this workplace is for (the job track id), for example POLICE or MEDIC.")]
	protected string m_sJob;

	[Attribute("", UIWidgets.EditBox, desc: "Stable place id for LF_Places. Empty uses the entity name.")]
	protected string m_sPlaceId;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Optional NPC spawned at the workplace (a clerk, a dispatcher). Empty for none.", params: "et")]
	protected ResourceName m_NpcPrefab;

	[Attribute("0 0 -1", UIWidgets.Coords, desc: "Where the NPC stands, relative to the workplace.")]
	protected vector m_vNpcOffset;

	[Attribute("0", UIWidgets.EditBox, desc: "NPC heading relative to the workplace, degrees.")]
	protected float m_fNpcYaw;

	protected EntityID m_NpcId = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		string placeId = GetPlaceId();
		if (placeId.IsEmpty())
			LF_Debug.Warn(LF_Workplace.FEATURE, string.Format("workplace for %1 has no place id and the entity is unnamed; it is not registered", m_sJob));
		else
			LF_Places.Register(LF_Workplace.PLACE_KIND, placeId, owner);

		if (!Replication.IsServer())
			return;

		if (m_sJob.IsEmpty())
			LF_Debug.Error(LF_Workplace.FEATURE, string.Format("workplace %1 has no job id", placeId));

		if (!m_NpcPrefab.IsEmpty())
			GetGame().GetCallqueue().CallLater(SpawnNpc, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the optional NPC, spawned once. A bad prefab logs and leaves the workplace without it.
	protected void SpawnNpc()
	{
		IEntity owner = GetOwner();
		if (!owner || m_NpcId != EntityID.INVALID)
			return;

		vector at = owner.CoordToParent(m_vNpcOffset);
		vector yaw = owner.GetYawPitchRoll();
		yaw[0] = yaw[0] + m_fNpcYaw;
		IEntity npc = LF_Utils.SpawnEntityPrefab(m_NpcPrefab, at, yaw);
		if (!npc)
		{
			LF_Debug.Error(LF_Workplace.FEATURE, string.Format("workplace %1: NPC prefab did not spawn: %2", GetPlaceId(), m_NpcPrefab));
			return;
		}

		m_NpcId = npc.GetID();
		LF_Debug.Log(LF_Workplace.FEATURE, string.Format("workplace %1 (%2): NPC placed", GetPlaceId(), m_sJob));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the workplace's NPC, or null when the slot is empty or the NPC is gone.
	IEntity GetNpc()
	{
		if (m_NpcId == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(m_NpcId);
	}

	//------------------------------------------------------------------------------------------------
	string GetJob()
	{
		return m_sJob;
	}

	//------------------------------------------------------------------------------------------------
	bool HasNpcSlot()
	{
		return !m_NpcPrefab.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	string GetPlaceId()
	{
		if (!m_sPlaceId.IsEmpty())
			return m_sPlaceId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}
}
