//------------------------------------------------------------------------------------------------
//! The NPC's voice and mouth on one machine: the talker's, and every bystander the server told.
//! A line plays from the NPC's head through the dialogue's audio project, and while it plays the
//! jaw bone follows the line's loudness envelope, which the pipeline measured from the recording
//! (docs/architecture/npc-dialogue.md "Lip sync"). Nothing here runs on a dedicated server.
//------------------------------------------------------------------------------------------------
class LF_DialogueUtterance : Managed
{
	EntityID m_NpcId;
	AudioHandle m_Handle = AudioHandle.Invalid;
	string m_sMouth;
	int m_iStartMs;
	float m_fSeconds;
	//! Entries still to say after this one (a bystander hears the whole answer).
	ref array<string> m_aQueue;
	ref LF_DialogueConfig m_Config;
}

//------------------------------------------------------------------------------------------------
//! One bone of the mouth pose: how far it turns and moves, relative to its rest, at the loudest step.
class LF_DialogueMouthBone : Managed
{
	string m_sBone;
	vector m_vAngles;
	vector m_vTranslation;
}

class LF_DialogueVoice
{
	static const string HEAD_BONE = "Head";
	//! The open-mouth pose, as "Bone:ax,ay,az,tx,ty,tz;..." (degrees, metres, each bone's own axes).
	//! Found by eye on Head_White_02 with the play-session harness (`dialogue rig`); see
	//! docs/architecture/npc-dialogue.md "Lip sync". Empty: the mouth is left alone.
	//! The lower lip drops 8 mm in the middle and 6 mm at the sides, the upper lip lifts 2 mm.
	static const string DEFAULT_RIG = "Orbicularis_bottomCenterLips:0,0,0,0,0.008,0;Orbicularis_bottomLipsLeft:0,0,0,0,0.006,0;Orbicularis_bottomLipsRight:0,0,0,0,0.006,0;Orbicularis_upperCenterLips:0,0,0,0,-0.002,0";

	protected static ref array<ref LF_DialogueMouthBone> s_aRig;
	//! Pause between two lines of one answer.
	static const float GAP_SECONDS = 0.35; // lf-fixed: speech pacing

	protected static ref array<ref LF_DialogueUtterance> s_aPlaying;
	protected static bool s_bTicking;
	protected static int s_iPlayed;
	protected static int s_iMouthFrames;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aPlaying = null;
		s_aRig = null;
		s_bTicking = false;
		s_iPlayed = 0;
		s_iMouthFrames = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Lines started on this machine, and frames the jaw was driven: what the harness reads.
	static int PlayedCount()
	{
		return s_iPlayed;
	}

	//------------------------------------------------------------------------------------------------
	static int MouthFrames()
	{
		return s_iMouthFrames;
	}

	//------------------------------------------------------------------------------------------------
	//! World position of an entity's head, or 1.65 m above its feet when it has no such bone.
	static vector HeadOf(IEntity entity)
	{
		if (!entity)
			return vector.Zero;

		Animation animation = entity.GetAnimation();
		if (animation)
		{
			TNodeId bone = animation.GetBoneIndex(HEAD_BONE);
			vector mat[4];
			if (bone != -1 && animation.GetBoneMatrix(bone, mat))
				return entity.CoordToParent(mat[3]);
		}

		return entity.GetOrigin() + "0 1.65 0";
	}

	//------------------------------------------------------------------------------------------------
	//! Say one spoken entry (a line id, or a bare caption key that has no voice).
	//! \return Seconds the caption should hold.
	static float Say(IEntity npc, LF_DialogueConfig config, string entry, array<string> queue = null)
	{
		Stop(npc);
		LF_DialogueLine line = LF_DialogueFlow.FindLine(config, entry);
		float hold = LF_DialogueFlow.HoldSeconds(line, WidgetManager.Translate(entry).Length());
		if (!npc || !config)
			return hold;

		LF_DialogueUtterance utterance = new LF_DialogueUtterance();
		utterance.m_NpcId = npc.GetID();
		utterance.m_iStartMs = System.GetTickCount();
		utterance.m_fSeconds = hold;
		utterance.m_aQueue = queue;
		utterance.m_Config = config;
		if (line)
		{
			utterance.m_sMouth = line.m_sMouth;
			vector mat[4];
			npc.GetWorldTransform(mat);
			mat[3] = HeadOf(npc);
			if (!config.m_sSoundProject.IsEmpty())
				utterance.m_Handle = AudioSystem.PlayEvent(config.m_sSoundProject, line.m_sEvent, mat);

			s_iPlayed++;
			if (utterance.m_Handle == AudioHandle.Invalid)
				LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("line '%1/%2' has no sound: event %3 did not start in %4 (the caption still shows)", config.m_sId, line.m_sId, line.m_sEvent, config.m_sSoundProject));
			else
				LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("line '%1/%2' plays: event %3, %4 s, mouth envelope %5 steps", config.m_sId, line.m_sId, line.m_sEvent, line.m_fSeconds, line.m_sMouth.Length()));
		}
		else
		{
			LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("'%1' says %2 as a caption only: no line is voiced for it", config.m_sId, entry));
		}

		if (!s_aPlaying)
			s_aPlaying = {};

		s_aPlaying.Insert(utterance);
		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, 0, true);
		}

		return hold;
	}

	//------------------------------------------------------------------------------------------------
	//! A bystander: say every entry of an answer, one after another.
	static void SayList(IEntity npc, string spoken)
	{
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		if (!config || spoken.IsEmpty())
			return;

		array<string> entries = {};
		spoken.Split(LF_DialogueFlow.SEPARATOR, entries, true);
		if (entries.IsEmpty())
			return;

		string first = entries[0];
		entries.RemoveOrdered(0);
		Say(npc, config, first, entries);
	}

	//------------------------------------------------------------------------------------------------
	//! Cut off whatever this NPC is saying on this machine and close its mouth.
	static void Stop(IEntity npc)
	{
		if (!npc || !s_aPlaying)
			return;

		for (int i = s_aPlaying.Count() - 1; i >= 0; i--)
		{
			LF_DialogueUtterance utterance = s_aPlaying[i];
			if (utterance.m_NpcId != npc.GetID())
				continue;

			if (utterance.m_Handle != AudioHandle.Invalid)
				AudioSystem.TerminateSound(utterance.m_Handle);

			SetMouth(npc, 0);
			s_aPlaying.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The mouth pose in use. Parsed from DEFAULT_RIG on first use; the harness can replace it.
	static array<ref LF_DialogueMouthBone> Rig()
	{
		if (!s_aRig)
			SetRig(DEFAULT_RIG);

		return s_aRig;
	}

	//------------------------------------------------------------------------------------------------
	//! Replace the mouth pose: "Bone:ax,ay,az,tx,ty,tz;Bone:...". A malformed entry is skipped.
	//! eturn How many bones the pose moves.
	static int SetRig(string spec)
	{
		s_aRig = {};
		array<string> entries = {};
		spec.Split(";", entries, true);
		foreach (string entry : entries)
		{
			int colon = entry.IndexOf(":");
			if (colon <= 0)
				continue;

			array<string> numbers = {};
			entry.Substring(colon + 1, entry.Length() - colon - 1).Split(",", numbers, false);
			if (numbers.Count() != 6)
				continue;

			LF_DialogueMouthBone bone = new LF_DialogueMouthBone();
			bone.m_sBone = entry.Substring(0, colon);
			bone.m_vAngles = Vector(numbers[0].ToFloat(), numbers[1].ToFloat(), numbers[2].ToFloat());
			bone.m_vTranslation = Vector(numbers[3].ToFloat(), numbers[4].ToFloat(), numbers[5].ToFloat());
			s_aRig.Insert(bone);
		}

		return s_aRig.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Open the mouth of an NPC by 0..1: the pose's bones on the body and on every worn part that
	//! carries the skeleton (the head is its own entity).
	static void SetMouth(IEntity npc, float open)
	{
		if (!npc || Rig().IsEmpty())
			return;

		open = Math.Clamp(open, 0, 1);
		PoseMouth(npc, open);
		IEntity child = npc.GetChildren();
		while (child)
		{
			PoseMouth(child, open);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void PoseMouth(IEntity entity, float open)
	{
		Animation animation = entity.GetAnimation();
		if (!animation)
			return;

		foreach (LF_DialogueMouthBone bone : s_aRig)
		{
			TNodeId id = animation.GetBoneIndex(bone.m_sBone);
			if (id != -1)
				animation.SetBone(entity, id, bone.m_vAngles * open, bone.m_vTranslation * open, 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while a line plays anywhere on this machine.
	protected static void Tick()
	{
		BaseWorld world = GetGame().GetWorld();
		int now = System.GetTickCount();
		if (s_aPlaying && world)
		{
			for (int i = s_aPlaying.Count() - 1; i >= 0; i--)
			{
				LF_DialogueUtterance utterance = s_aPlaying[i];
				IEntity npc = world.FindEntityByID(utterance.m_NpcId);
				float seconds = (now - utterance.m_iStartMs) / 1000.0;
				if (npc && seconds < utterance.m_fSeconds)
				{
					if (!utterance.m_sMouth.IsEmpty() && !Rig().IsEmpty())
					{
						SetMouth(npc, LF_DialogueFlow.MouthAt(utterance.m_sMouth, seconds));
						s_iMouthFrames++;
					}

					continue;
				}

				if (npc)
					SetMouth(npc, 0);

				// The next entry of a bystander's queue starts after a breath.
				if (npc && utterance.m_aQueue && !utterance.m_aQueue.IsEmpty() && seconds < utterance.m_fSeconds + GAP_SECONDS)
					continue;

				s_aPlaying.Remove(i);
				if (npc && utterance.m_aQueue && !utterance.m_aQueue.IsEmpty())
				{
					string next = utterance.m_aQueue[0];
					utterance.m_aQueue.RemoveOrdered(0);
					Say(npc, utterance.m_Config, next, utterance.m_aQueue);
				}
			}
		}

		if (!s_aPlaying || s_aPlaying.IsEmpty())
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bTicking = false;
		}
	}
}
