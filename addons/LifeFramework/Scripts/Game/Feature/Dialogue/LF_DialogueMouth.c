//------------------------------------------------------------------------------------------------
//! One NPC's face while it talks or is talked to: the mouth, a blink and a small head nod. All
//! numbers the mouth reads are the state of this one object, stepped by LF_DialogueMouth.Step.
class LF_DialogueFaceState : Managed
{
	EntityID m_NpcId = EntityID.INVALID;
	//! The scene is open with this NPC: it blinks while it listens too.
	bool m_bAttended;
	//! The line's loudness envelope now, 0..1 (0 in a pause or when nothing is said).
	float m_fRaw;
	//! The envelope after the attack and release smoothing, before the limits.
	float m_fSmooth;
	//! What the bones get: smoothed, clamped, and exactly 0 when the mouth is shut.
	float m_fOpen;
	int m_iSyllable;
	bool m_bInSyllable;
	bool m_bStressed;
	float m_fSyllableScale = 1;
	//! Seconds into the nod, or -1 when none runs.
	float m_fNodAge = -1;
	//! Seconds into the blink, or -1 when none runs.
	float m_fBlinkAge = -1;
	float m_fNextBlink = 1.5;
	int m_iBlinks;
}

//------------------------------------------------------------------------------------------------
//! The maths of a natural-looking mouth (docs/architecture/npc-dialogue.md "Lip sync"). The raw
//! loudness envelope flaps the lip; this smooths it (quick attack, slower release), caps the
//! opening, gives each syllable its own size, shuts the mouth fully in pauses, and adds a head nod
//! on stressed peaks and a blink. Pure: no entity is touched here.
class LF_DialogueMouth
{
	static const float ATTACK_SECONDS = 0.04; // lf-fixed: mouth feel, the same for every NPC
	static const float RELEASE_SECONDS = 0.1; // lf-fixed: mouth feel
	//! The mouth never opens further than this fraction of the rig's pose.
	static const float MAX_OPEN = 0.85; // lf-fixed: mouth feel
	//! Below this the mouth is shut (a pause closes the lips fully, no lingering gap).
	static const float CLOSED_BELOW = 0.06; // lf-fixed: mouth feel
	static const float SYLLABLE_ON = 0.45; // lf-fixed: face feel
	static const float SYLLABLE_OFF = 0.2; // lf-fixed: face feel
	//! An envelope peak at or above this is stressed and nods the head.
	static const float STRESS_AT = 0.85; // lf-fixed: face feel
	static const float NOD_SECONDS = 0.3; // lf-fixed: face feel
	static const float BLINK_SECONDS = 0.16; // lf-fixed: face feel
	static const float BLINK_GAP_MIN = 2.2; // lf-fixed: face feel
	static const float BLINK_GAP_MAX = 5.0; // lf-fixed: face feel
	//! Each syllable opens between SYLLABLE_MIN_SCALE and 1 times the envelope.
	static const float SYLLABLE_MIN_SCALE = 0.7; // lf-fixed: face feel

	//------------------------------------------------------------------------------------------------
	//! Pure: one smoothing step toward a target, quick when opening, slower when closing.
	static float Smooth(float current, float target, float dt)
	{
		float tau = RELEASE_SECONDS;
		if (target > current)
			tau = ATTACK_SECONDS;

		return Math.Lerp(current, target, Math.Clamp(dt / tau, 0, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the opening that reaches the bones: capped, and fully shut below the threshold.
	static float Limit(float value)
	{
		if (value < CLOSED_BELOW)
			return 0;

		return Math.Min(value, MAX_OPEN);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the size of the Nth syllable, a fixed spread between SYLLABLE_MIN_SCALE and 1.
	static float SyllableScale(int index)
	{
		int slot = (index * 5 + 3) % 7;
		return SYLLABLE_MIN_SCALE + (1 - SYLLABLE_MIN_SCALE) * (slot / 6.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: seconds from the end of one blink to the start of the next.
	static float BlinkGap(int index)
	{
		int slot = (index * 5 + 2) % 7;
		return BLINK_GAP_MIN + (BLINK_GAP_MAX - BLINK_GAP_MIN) * (slot / 6.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: 0..1..0 over a motion that runs `age` seconds of `length`; 0 when none runs.
	static float Arc(float age, float length)
	{
		if (age < 0 || age >= length)
			return 0;

		return Math.Sin(Math.PI * age / length);
	}

	//------------------------------------------------------------------------------------------------
	//! Step one face by `dt` seconds with the envelope value `raw` now. Sets m_fOpen, and starts and
	//! ends syllables, nods and blinks.
	static void Step(LF_DialogueFaceState state, float raw, float dt)
	{
		raw = Math.Clamp(raw, 0, 1);
		state.m_fRaw = raw;
		if (!state.m_bInSyllable && raw >= SYLLABLE_ON)
		{
			state.m_bInSyllable = true;
			state.m_bStressed = false;
			state.m_iSyllable++;
			state.m_fSyllableScale = SyllableScale(state.m_iSyllable);
		}
		else if (state.m_bInSyllable && raw < SYLLABLE_OFF)
		{
			state.m_bInSyllable = false;
		}

		if (state.m_bInSyllable && !state.m_bStressed && raw >= STRESS_AT)
		{
			state.m_bStressed = true;
			state.m_fNodAge = 0;
		}

		state.m_fSmooth = Smooth(state.m_fSmooth, raw * state.m_fSyllableScale, dt);
		state.m_fOpen = Limit(state.m_fSmooth);

		if (state.m_fNodAge >= 0)
		{
			state.m_fNodAge += dt;
			if (state.m_fNodAge >= NOD_SECONDS)
				state.m_fNodAge = -1;
		}

		if (state.m_fBlinkAge >= 0)
		{
			state.m_fBlinkAge += dt;
			if (state.m_fBlinkAge >= BLINK_SECONDS)
			{
				state.m_fBlinkAge = -1;
				state.m_fNextBlink = BlinkGap(state.m_iBlinks);
			}
		}
		else
		{
			state.m_fNextBlink -= dt;
			if (state.m_fNextBlink <= 0)
			{
				state.m_fBlinkAge = 0;
				state.m_iBlinks++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static float NodAmount(LF_DialogueFaceState state)
	{
		return Arc(state.m_fNodAge, NOD_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	static float BlinkAmount(LF_DialogueFaceState state)
	{
		return Arc(state.m_fBlinkAge, BLINK_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	//! True when nothing about this face moves and nobody is attending: it can be dropped.
	static bool IsIdle(LF_DialogueFaceState state)
	{
		return !state.m_bAttended && state.m_fRaw <= 0 && state.m_fOpen <= 0 && state.m_fNodAge < 0 && state.m_fBlinkAge < 0;
	}
}
