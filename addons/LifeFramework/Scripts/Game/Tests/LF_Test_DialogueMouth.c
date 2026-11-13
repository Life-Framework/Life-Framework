// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_DialogueMouth`: raising LF_DialogueMouth.MAX_OPEN to 1.5 failed `dialogue/mouth` with `- the opening is clamped to the maximum (never more than 0.9 of the pose)`. Restored; green.
// tier: LOGIC
//! The NPC's mouth maths (Feature/Dialogue/LF_DialogueMouth.c): the loudness envelope is smoothed
//! (quick attack, slower release), the opening is capped, syllables differ a little, a pause shuts
//! the mouth fully, and the idle face nods on stressed peaks and blinks.
class LF_Test_DialogueMouth : LF_Test
{
	override string GetName()
	{
		return "dialogue/mouth";
	}

	//------------------------------------------------------------------------------------------------
	//! Step a face for `seconds` at 60 fps with a steady envelope value; returns the opening at the end.
	protected float Hold(LF_DialogueFaceState face, float raw, float seconds)
	{
		int steps = Math.Round(seconds * 60);
		for (int i = 0; i < steps; i++)
		{
			LF_DialogueMouth.Step(face, raw, 1.0 / 60);
		}

		return face.m_fOpen;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Smoothing: a 1 step does not snap the mouth open, and closing is slower than opening.
		float attack = LF_DialogueMouth.Smooth(0, 1, 0.02);
		float release = LF_DialogueMouth.Smooth(1, 0, 0.02);
		ctx.True(attack > 0.3 && attack < 0.7, "20 ms into an attack the lip is about half open, not snapped open");
		ctx.True(release > 0.7 && release < 0.9, "20 ms into a release the lip has barely closed");
		ctx.True(1 - release < attack, "the mouth closes slower than it opens");
		ctx.EqualFloat(1, LF_DialogueMouth.Smooth(0, 1, 0.1), 0.001, "an attack finishes within about 40 ms");
		ctx.EqualFloat(0, LF_DialogueMouth.Smooth(1, 0, 0.3), 0.001, "a release finishes within about 100 ms");

		// A loud steady sound opens the mouth but never past the cap.
		LF_DialogueFaceState loud = new LF_DialogueFaceState();
		loud.m_iSyllable = 1; // the next syllable is the one that opens fully (scale 1)
		float open = Hold(loud, 1, 1);
		ctx.True(open > 0.6, "a loud sound opens the mouth wide");
		ctx.True(open <= 0.9, "the opening is clamped to the maximum (never more than 0.9 of the pose)");

		// A pause shuts it fully: exactly 0, no lingering gap.
		float shut = Hold(loud, 0, 0.5);
		ctx.EqualFloat(0, shut, 0.0001, "a pause closes the mouth fully");
		LF_DialogueFaceState quiet = new LF_DialogueFaceState();
		ctx.EqualFloat(0, Hold(quiet, 0.04, 1), 0.0001, "a whisper of noise below the threshold keeps the lips shut");

		// Syllables differ: the same loud peak opens a little differently, always in bounds.
		bool differs = false;
		for (int i = 1; i < 8; i++)
		{
			float scale = LF_DialogueMouth.SyllableScale(i);
			ctx.True(scale >= LF_DialogueMouth.SYLLABLE_MIN_SCALE - 0.0001 && scale <= 1.0001, "a syllable's size stays between the minimum and full");
			if (Math.AbsFloat(scale - LF_DialogueMouth.SyllableScale(i + 1)) > 0.01)
				differs = true;
		}

		ctx.True(differs, "successive syllables are not all the same size");
		LF_DialogueFaceState words = new LF_DialogueFaceState();
		float first = Hold(words, 0.7, 0.4);
		Hold(words, 0, 0.3);
		float second = Hold(words, 0.7, 0.4);
		ctx.True(Math.AbsFloat(first - second) > 0.01, "two syllables of the same loudness open the mouth by different amounts");

		// A stressed peak nods the head; ordinary speech does not.
		LF_DialogueFaceState calm = new LF_DialogueFaceState();
		Hold(calm, 0.6, 0.5);
		ctx.True(calm.m_fNodAge < 0, "ordinary loudness does not nod");
		LF_DialogueFaceState stress = new LF_DialogueFaceState();
		LF_DialogueMouth.Step(stress, 0.95, 1.0 / 60);
		ctx.True(stress.m_fNodAge >= 0, "a stressed peak starts a nod");
		ctx.EqualFloat(0, LF_DialogueMouth.NodAmount(new LF_DialogueFaceState()), 0.0001, "no nod running moves nothing");

		// Idle: it blinks every few seconds, each blink an arc 0..1..0.
		LF_DialogueFaceState idle = new LF_DialogueFaceState();
		float most = 0;
		for (int s = 0; s < 60 * 12; s++)
		{
			LF_DialogueMouth.Step(idle, 0, 1.0 / 60);
			most = Math.Max(most, LF_DialogueMouth.BlinkAmount(idle));
		}

		ctx.True(idle.m_iBlinks >= 2 && idle.m_iBlinks <= 6, "an idle face blinks every few seconds");
		ctx.True(most > 0.9 && most <= 1.0001, "a blink closes the lids fully, then opens them");
		ctx.EqualFloat(0, idle.m_fOpen, 0.0001, "an idle face keeps its mouth shut");
		ctx.True(LF_DialogueMouth.BlinkGap(1) >= LF_DialogueMouth.BLINK_GAP_MIN - 0.0001 && LF_DialogueMouth.BlinkGap(1) <= LF_DialogueMouth.BLINK_GAP_MAX + 0.0001, "blinks are spaced inside the configured gap");
	}
}
