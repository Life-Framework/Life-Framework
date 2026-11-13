//------------------------------------------------------------------------------------------------
//! One moving part of an asset: a bone of its model and how it moves.
[BaseContainerProps()]
class LF_AssetAnimBone
{
	[Attribute(desc: "Bone of the model (the asset spec's anim.bones name).")]
	string m_sBone;

	[Attribute("0", UIWidgets.ComboBox, desc: "SPIN turns while active; HINGE and SLIDE move to their end and back.", enums: ParamEnumArray.FromEnum(LF_EAssetAnimMotion))]
	LF_EAssetAnimMotion m_eMotion;

	[Attribute("0 0 0", desc: "SPIN: degrees per second. HINGE: the open angle. As pitch (about x), yaw (about y), roll (about z).")]
	vector m_vAngles;

	[Attribute("0 0 0", desc: "SLIDE: how far the part moves, metres, model space.")]
	vector m_vOffset;

	[Attribute("1", desc: "HINGE and SLIDE: seconds from one end to the other.")]
	float m_fSeconds;

	//! Client state: 0..1 of a hinge or slide.
	float m_fProgress;
}

//------------------------------------------------------------------------------------------------
//! A particle effect that runs at a socket of the model while the asset is active.
[BaseContainerProps()]
class LF_AssetAnimEffect
{
	[Attribute(desc: "Socket of the model (the asset spec's anim.sockets name).")]
	string m_sSocket;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Particle effect.", params: "ptc")]
	ResourceName m_sParticle;
}

//------------------------------------------------------------------------------------------------
//! A looped sound that plays from the asset while it is active.
[BaseContainerProps()]
class LF_AssetAnimSound
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Sound project.", params: "acp")]
	ResourceName m_sProject;

	[Attribute(desc: "Event in the project; a loop, named SOUND_LF_..._LP.")]
	string m_sEvent;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/AssetAnim", description: "Moving parts, effects and looped sounds of an asset, switched on and off by the server. Written by tools\\cli asset anim from the asset's spec.")]
class LF_AssetAnimComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! What an asset does by itself while it is in use (#340): parts that spin or swing, sparks at a
//! socket, a looped sound. The server owns two replicated facts, "active" and an optional pose that
//! holds the asset still in the air (a drill held against a wall); every machine that renders plays
//! the rest from them, so a player who joins mid-use sees and hears the same as everyone else.
//! The block in the prefab is generated from tools/assets/<slug>/spec.json (anim) and checked by
//! tools\cli validate.
class LF_AssetAnimComponent : ScriptComponent
{
	[Attribute()]
	protected ref array<ref LF_AssetAnimBone> m_aBones;

	[Attribute()]
	protected ref array<ref LF_AssetAnimEffect> m_aEffects;

	[Attribute()]
	protected ref array<ref LF_AssetAnimSound> m_aSounds;

	[RplProp(onRplName: "OnStateReplicated")]
	protected bool m_bActive;

	//! While true the asset stays at m_vPosePosition / m_vPoseAngles with its physics asleep.
	[RplProp(onRplName: "OnStateReplicated")]
	protected bool m_bPosed;

	[RplProp(onRplName: "OnStateReplicated")]
	protected vector m_vPosePosition;

	[RplProp(onRplName: "OnStateReplicated")]
	protected vector m_vPoseAngles;

	//! How often a machine that could not start the sounds (out of earshot, just joined) tries again.
	// lf-fixed: a retry interval, not a gameplay knob
	static const int SOUND_RETRY_MS = 2000;

	//! What this machine currently shows.
	protected bool m_bShownActive;
	protected bool m_bShownPosed;
	protected float m_fActiveSeconds;
	protected ref array<EntityID> m_aEffectEntities = {};
	protected ref array<ref SCR_AudioSource> m_aAudio = {};

	//! Every component alive on this machine (the harness asks what the nearest one shows).
	protected static ref array<LF_AssetAnimComponent> s_aAll = {};

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_aAll.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest animated asset within `radius` of a point, or null. One carried in an inventory
	//! (it has a parent) does not count: nobody sees it.
	static LF_AssetAnimComponent Nearest(vector from, float radius)
	{
		LF_AssetAnimComponent best;
		float bestDistance = radius;
		foreach (LF_AssetAnimComponent component : s_aAll)
		{
			if (!component || !component.GetOwner() || component.GetOwner().GetParent())
				continue;

			float distance = vector.Distance(component.GetOwner().GetOrigin(), from);
			if (distance <= bestDistance)
			{
				bestDistance = distance;
				best = component;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	static LF_AssetAnimComponent Of(IEntity entity)
	{
		if (!entity)
			return null;

		return LF_AssetAnimComponent.Cast(entity.FindComponent(LF_AssetAnimComponent));
	}

	//------------------------------------------------------------------------------------------------
	bool IsRunning()
	{
		return m_bActive;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPosed()
	{
		return m_bPosed;
	}

	//------------------------------------------------------------------------------------------------
	//! What this machine shows (false on a dedicated server, which renders nothing).
	bool IsShownActive()
	{
		return m_bShownActive;
	}

	//------------------------------------------------------------------------------------------------
	int GetBoneCount()
	{
		return m_aBones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Name of a moving part's bone, or empty.
	string GetBoneName(int index)
	{
		if (!m_aBones.IsIndexValid(index))
			return string.Empty;

		return m_aBones[index].m_sBone;
	}

	//------------------------------------------------------------------------------------------------
	int GetEffectCount()
	{
		return m_aEffects.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetSoundCount()
	{
		return m_aSounds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Effects and sounds running on this machine right now.
	int GetLiveEffectCount()
	{
		return m_aEffectEntities.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetLiveSoundCount()
	{
		return m_aAudio.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: switch the asset on or off. Replicated to every client, late joiners included.
	bool SetActive(bool active)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn(LF_AssetAnim.FEATURE, "SetActive refused: only the server switches an asset");
			return false;
		}

		if (m_bActive == active)
			return true;

		m_bActive = active;
		LF_AssetAnim.CountSwitch(active);
		Replication.BumpMe();
		LF_Debug.Info(LF_AssetAnim.FEATURE, string.Format("%1 active=%2", LF_Utils.GetPrefabName(GetOwner()), active));
		Apply();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hold the asset still at a world position and yaw/pitch/roll, physics asleep (a tool held
	//! against a wall). Release() lets it go where it is.
	bool Pose(vector position, vector angles)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn(LF_AssetAnim.FEATURE, "Pose refused: only the server poses an asset");
			return false;
		}

		m_bPosed = true;
		m_vPosePosition = position;
		m_vPoseAngles = angles;
		Replication.BumpMe();
		Apply();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stop holding the asset and set it down at a world position and angles, physics awake
	//! again. Every machine moves it there itself, so nobody is left with a tool hanging in the air.
	void Release(vector position, vector angles)
	{
		if (!Replication.IsServer() || !m_bPosed)
			return;

		m_bPosed = false;
		m_vPosePosition = position;
		m_vPoseAngles = angles;
		Replication.BumpMe();
		Apply();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStateReplicated()
	{
		Apply();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;

		s_aAll.Insert(this);
		// A late joiner gets the replicated values with the entity; show them once it is in the world.
		GetGame().GetCallqueue().CallLater(Apply, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RetrySounds);

		s_aAll.RemoveItem(this);
		StopEffects();
		StopSounds();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Bring this machine in line with the replicated state.
	protected void Apply()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		ApplyPose(owner);
		if (m_bPosed)
			SetEventMask(owner, EntityEvent.FRAME);

		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		if (m_bActive == m_bShownActive)
			return;

		m_bShownActive = m_bActive;
		if (m_bActive)
		{
			m_fActiveSeconds = 0;
			StartEffects(owner);
			StartSounds(owner);
		}
		else
		{
			StopEffects();
			StopSounds();
		}

		// Frames run while anything still moves; EOnFrame switches them off again.
		SetEventMask(owner, EntityEvent.FRAME);
		LF_Debug.Info(LF_AssetAnim.FEATURE, string.Format("%1 shows active=%2 here: %3 effect(s), %4 sound(s)", LF_Utils.GetPrefabName(owner), m_bActive, m_aEffectEntities.Count(), m_aAudio.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPose(IEntity owner)
	{
		// In an inventory (an officer bagged it mid-use) the storage owns where it is.
		if (owner.GetParent())
		{
			m_bShownPosed = m_bPosed;
			return;
		}

		Physics physics = owner.GetPhysics();
		if (m_bPosed)
		{
			if (physics)
				physics.ChangeSimulationState(SimulationState.NONE);

			owner.SetOrigin(m_vPosePosition);
			owner.SetAngles(m_vPoseAngles);
			owner.Update();
		}
		else if (m_bShownPosed)
		{
			// Released: m_vPosePosition now holds where it was set down.
			owner.SetOrigin(m_vPosePosition);
			owner.SetAngles(m_vPoseAngles);
			owner.Update();
			if (physics)
				physics.ChangeSimulationState(SimulationState.SIMULATION);
		}

		m_bShownPosed = m_bPosed;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		Animation animation = owner.GetAnimation();

		if (m_bShownActive)
			m_fActiveSeconds += timeSlice;

		// A held asset stays held: an item that has just left an inventory is put down and woken by
		// the engine after the pose arrived (seen on the client that dropped the drill, 2026-11-14).
		bool moving = m_bPosed;
		if (m_bPosed && !owner.GetParent() && vector.DistanceSq(owner.GetOrigin(), m_vPosePosition) > 0.0001)
			ApplyPose(owner);

		foreach (LF_AssetAnimBone bone : m_aBones)
		{
			if (!animation)
				break;

			TNodeId id = animation.GetBoneIndex(bone.m_sBone);
			if (id == -1)
				continue;

			if (bone.m_eMotion == LF_EAssetAnimMotion.SPIN)
			{
				if (!m_bShownActive)
					continue;

				moving = true;
				animation.SetBone(owner, id, LF_AssetAnim.SpinAngles(bone.m_vAngles, m_fActiveSeconds), vector.Zero, 1);
				continue;
			}

			bone.m_fProgress = LF_AssetAnim.Progress(bone.m_fProgress, m_bShownActive, timeSlice, bone.m_fSeconds);
			float eased = LF_AssetAnim.Ease(bone.m_fProgress);
			animation.SetBone(owner, id, bone.m_vAngles * eased, bone.m_vOffset * eased, 1);
			if (!LF_AssetAnim.Settled(bone.m_fProgress, m_bShownActive))
				moving = true;
		}

		if (!moving)
			ClearEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartEffects(IEntity owner)
	{
		StopEffects();
		Animation animation = owner.GetAnimation();
		foreach (LF_AssetAnimEffect effect : m_aEffects)
		{
			if (effect.m_sParticle.IsEmpty())
				continue;

			ParticleEffectEntitySpawnParams params = new ParticleEffectEntitySpawnParams();
			params.Parent = owner;
			params.PlayOnHeadlessClient = false;
			if (animation && !effect.m_sSocket.IsEmpty())
			{
				TNodeId socket = animation.GetBoneIndex(effect.m_sSocket);
				if (socket == -1)
					LF_Debug.Error(LF_AssetAnim.FEATURE, string.Format("%1 has no socket %2; the effect plays at its origin", LF_Utils.GetPrefabName(owner), effect.m_sSocket));
				else
					params.PivotID = socket;
			}

			ParticleEffectEntity particles = ParticleEffectEntity.SpawnParticleEffect(effect.m_sParticle, params);
			if (!particles)
			{
				LF_Debug.Error(LF_AssetAnim.FEATURE, string.Format("effect %1 did not spawn on %2", effect.m_sParticle, LF_Utils.GetPrefabName(owner)));
				continue;
			}

			m_aEffectEntities.Insert(particles.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StopEffects()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;

		foreach (EntityID id : m_aEffectEntities)
		{
			IEntity particles = GetGame().GetWorld().FindEntityByID(id);
			if (particles)
				SCR_EntityHelper.DeleteEntityAndChildren(particles);
		}

		m_aEffectEntities.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartSounds(IEntity owner)
	{
		StopSounds();
		if (m_aSounds.IsEmpty())
			return;

		SCR_SoundManagerModule sound = SCR_SoundManagerModule.GetInstance(owner.GetWorld());
		if (!sound)
		{
			LF_Debug.Warn(LF_AssetAnim.FEATURE, "no sound manager in this world; the asset is silent");
			return;
		}

		foreach (LF_AssetAnimSound entry : m_aSounds)
		{
			SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
			config.m_sSoundProject = entry.m_sProject;
			config.m_sSoundEventName = entry.m_sEvent;
			// Not static: a posed tool is moved when its use ends.
			config.m_eFlags = 0;

			// Null when the listener is out of the sound's range, or when the project did not load.
			SCR_AudioSource audio = sound.CreateAudioSource(owner, config);
			if (!audio)
			{
				LF_Debug.Info(LF_AssetAnim.FEATURE, string.Format("%1 not started: out of earshot here (or %2 failed to load)", entry.m_sEvent, entry.m_sProject));
				continue;
			}

			sound.PlayAudioSource(audio);
			m_aAudio.Insert(audio);
		}

		// A listener that was out of range, or a player who has only just joined, hears nothing yet:
		// try again while the asset stays on.
		if (m_aAudio.Count() < m_aSounds.Count())
			GetGame().GetCallqueue().CallLater(RetrySounds, SOUND_RETRY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void RetrySounds()
	{
		IEntity owner = GetOwner();
		if (!owner || !m_bShownActive || m_aAudio.Count() >= m_aSounds.Count())
			return;

		StartSounds(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopSounds()
	{
		foreach (SCR_AudioSource audio : m_aAudio)
		{
			if (audio)
				audio.Terminate();
		}

		m_aAudio.Clear();
	}
}
