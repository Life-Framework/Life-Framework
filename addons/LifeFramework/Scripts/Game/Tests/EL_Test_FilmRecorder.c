// red-proof: 2026-11-08, shot count 10 -> 9, cli test --no-build --only EL_Test_FilmRecorder:
//   [ELTEST] FAIL film/recorder
//   [ELTEST]   - one second at 10 fps takes 10 shots (expected 9, got 10)
// Restored.

// tier: LOGIC
class EL_Test_FilmRecorder : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "film/recorder";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		// 10 fps is a shot every 100 ms; frames every 20 ms in between.
		LF_FilmPacer pacer = new LF_FilmPacer();
		pacer.Begin(10, 1000);
		int shots = 0;
		for (int t = 1020; t <= 1980; t += 20)
		{
			if (pacer.OnFrame(t))
				shots++;
		}
		ctx.Equal(10, shots, "one second at 10 fps takes 10 shots");
		ctx.Equal(49, pacer.GetFrames(), "every frame is counted");
		ctx.Equal(0, pacer.GetStalls(), "20 ms frames are not stalls");

		// A 500 ms stall takes one shot, not five catch-up shots, and counts.
		ctx.True(pacer.OnFrame(2500), "the late frame takes the overdue shot");
		ctx.False(pacer.OnFrame(2520), "no burst after a stall");
		ctx.Equal(11, pacer.GetShots(), "one shot for the stall");
		ctx.Equal(1, pacer.GetStalls(), "the 520 ms frame is a stall");
		ctx.Equal(520, pacer.GetMaxGapMs(), "max gap is the stall");
		ctx.True(pacer.OnFrame(2600), "the schedule resumes 100 ms after the late shot");

		// fps 0 measures frame times and never shoots (the baseline run).
		LF_FilmPacer baseline = new LF_FilmPacer();
		baseline.Begin(0, 0);
		ctx.False(baseline.OnFrame(16), "fps 0 never shoots");
		ctx.False(baseline.OnFrame(33), "fps 0 never shoots, second frame");
		ctx.Equal(0, baseline.GetShots(), "baseline takes no shots");
		ctx.True(baseline.Summary(1000).Contains("gameFps=2"), "summary reports the game frame rate");

		// The harness verb.
		EL_AgentCommand cmd;
		ctx.True(EL_AgentCommand.Parse("7 film start journey_atm 12 90", cmd), "film start parses");
		ctx.EqualStr("start", cmd.Arg1, "film mode");
		ctx.EqualStr("journey_atm", cmd.Arg2, "film name");
		ctx.EqualStr("12", cmd.Arg3, "film fps");
		ctx.EqualStr("90", cmd.Arg4, "film max seconds");
		ctx.True(EL_AgentCommand.Parse("8 film stop", cmd), "film stop parses");
		ctx.False(EL_AgentCommand.Parse("9 film start bad-name 12", cmd), "a name with a hyphen is refused (it becomes a path)");
		ctx.EqualStr("bad_film_name", cmd.Error, "bad name error");
		ctx.False(EL_AgentCommand.Parse("9 film start clip fast", cmd), "fps must be a number");
		ctx.False(EL_AgentCommand.Parse("9 film rewind", cmd), "unknown film mode");

		// The suite runs on a headless server: recording is refused, never started.
		string result = LF_FilmRecorder.Start("suite_clip", 12, 5);
		ctx.True(result.StartsWith("error:"), "a server without a renderer refuses to record: " + result);
		ctx.False(LF_FilmRecorder.IsRecording(), "a refused start leaves the recorder idle");
	}
}
