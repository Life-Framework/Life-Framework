// LF_ResidentWorkProps.c - the box in a carrier's hands and the broom in a sweeper's (#4644, after #4396).
// The base game's loiter command already owns "a prop that lives exactly as long as a pose": the pose's
// loiter data carries an item preset id (SCR_EAnimationItemID, replicated with the data and to late joiners),
// and the command on EVERY machine that shows the character (the server, each client, a late joiner) spawns
// that preset's items locally (SpawnEntityPrefabLocal), attaches them to a bone and deletes them when the pose
// ends. So nothing here is replicated or spawned by the server: no RplComponent on a prop, no layer override on
// the NPC, no static to reset, and the prop goes with the pose when the resident's day changes, the loop stops
// or the world ends. The vanilla presets come from one fixed config (Configs/Character/LoiterItemPresets.conf),
// so this file adds two ids with `modded enum` and answers them from a modded SCR_LoiterItemPresets.GetData with our own item data
// (LF_WorkLoiterItem, the vanilla SCR_LoiterItemData's spawn with plain fields instead of a config).
// Hammer has no prop: the base game has no hammer.

//------------------------------------------------------------------------------------------------
modded enum SCR_EAnimationItemID
{
	LF_WORK_CARRY,
	LF_WORK_SWEEP
}

//------------------------------------------------------------------------------------------------
//! One prop on one bone of the animated character, spawned locally. The same spawn as the vanilla
//! SCR_LoiterItemData (spawn at the character, attach to the bone with the default flags, then set the
//! transform in the bone's space).
class LF_WorkLoiterItem : SCR_LoiterItemDataBase
{
	protected ResourceName m_sPrefab;
	protected string m_sBone;
	protected vector m_vPosition;
	protected vector m_vAngles;

	//------------------------------------------------------------------------------------------------
	void LF_WorkLoiterItem(ResourceName prefab, string bone, vector position, vector angles)
	{
		m_sPrefab = prefab;
		m_sBone = bone;
		m_vPosition = position;
		m_vAngles = angles;
	}

	//------------------------------------------------------------------------------------------------
	override array<IEntity> SpawnPrefabs(notnull IEntity animationOwner, int seed)
	{
		Resource resource = Resource.Load(m_sPrefab);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(LF_ResidentWorkProps.FEATURE, "work prop prefab does not load, the resident works empty-handed: " + m_sPrefab);
			return null;
		}

		Animation animation = animationOwner.GetAnimation();
		TNodeId bone = -1;
		if (animation)
			bone = animation.GetBoneIndex(m_sBone);

		if (bone < 0)
		{
			LF_Debug.Error(LF_ResidentWorkProps.FEATURE, string.Format("work prop %1: no bone '%2' on the character, the resident works empty-handed", m_sPrefab, m_sBone));
			return null;
		}

		EntitySpawnParams params();
		animationOwner.GetTransform(params.Transform);
		IEntity item = GetGame().SpawnEntityPrefabLocal(resource, animationOwner.GetWorld(), params);
		if (!item)
			return null;

		animationOwner.AddChild(item, bone);
		vector local[4];
		Math3D.AnglesToMatrix(m_vAngles, local);
		local[3] = m_vPosition;
		item.SetLocalTransform(local);
		return {item};
	}
}

//------------------------------------------------------------------------------------------------
//! Which job holds which prop and where in the hand. Rules only; no state.
class LF_ResidentWorkProps
{
	static const string FEATURE = "Residents";
	static const ResourceName BOX_PREFAB = "{3D6E2B8F51A7C940}Prefabs/Props/Work/LF_WorkBox.et";
	static const ResourceName BROOM_PREFAB = "{7B1F94C2A6E03D58}Prefabs/Props/Work/LF_WorkBroom.et";
	//! The bone vanilla attaches a held item to (a "RightHand" bone does not exist, LF_Restraint.WORN_BONE).
	static const string HAND_BONE = "RightHandProp";

	//------------------------------------------------------------------------------------------------
	//! The item preset a job's pose carries (NONE for a job with nothing in hand).
	static SCR_EAnimationItemID PresetOf(string job)
	{
		if (job == LF_ResidentWorkLoop.CARRY)
			return SCR_EAnimationItemID.LF_WORK_CARRY;

		if (job == LF_ResidentWorkLoop.SWEEP)
			return SCR_EAnimationItemID.LF_WORK_SWEEP;

		return SCR_EAnimationItemID.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The prop item data of one of our presets, or null for any other id.
	static SCR_LoiterItemDataBase Item(SCR_EAnimationItemID preset)
	{
		// The clips key the hand prop bone to where each prop sits (the "prop" key in tools/assets/work-anim/clips, #4644),
		// so the prop hangs on the bone with no offset of its own.
		if (preset == SCR_EAnimationItemID.LF_WORK_CARRY)
			return new LF_WorkLoiterItem(BOX_PREFAB, HAND_BONE, vector.Zero, vector.Zero);

		if (preset == SCR_EAnimationItemID.LF_WORK_SWEEP)
			return new LF_WorkLoiterItem(BROOM_PREFAB, HAND_BONE, vector.Zero, vector.Zero);

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The loiter command looks a pose's item preset up here: ours answer with their own item data, every other id
//! with the base game's config. (The pose data class SCR_LoiterCustomAnimData is not modded: it is an RPC
//! parameter and a modded copy loses its replication codec.)
[BaseContainerProps(configRoot: true)]
modded class SCR_LoiterItemPresets
{
	//------------------------------------------------------------------------------------------------
	override SCR_LoiterItemDataBase GetData(SCR_EAnimationItemID id)
	{
		SCR_LoiterItemDataBase work = LF_ResidentWorkProps.Item(id);
		if (work)
			return work;

		return super.GetData(id);
	}
}
