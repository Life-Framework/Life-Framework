// red-proof: 2026-11-23: `toolsi test --no-build --red LF_Test_DispatchVoice` -> RED PROOF OK (the RedProof() hook skips the pick; every pick assertion fails); green with `--only`.
//! The dispatcher's voice on a radio call (Feature/Police/LF_DispatchVoice.c, #4422): a call of each
//! type picks its type line and its place line and logs `dispatch voice <type> <place>`; a missing line
//! warns and the call still lands with the lines it has. Uses a neutral fixture, not the pack's recordings.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DispatchVoice : LF_Test
{
	override string GetName()
	{
		return "police/dispatch-voice";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueLine Line(string id, string forKey, string soundEvent = "")
	{
		LF_DialogueLine line = new LF_DialogueLine();
		line.m_sId = id;
		line.m_sTextKey = "#T_" + id;
		line.m_sForKey = forKey;
		line.m_sEvent = soundEvent;
		// A line with no event and no length is a caption only (LF_DialogueLine.IsTextOnly).
		if (!soundEvent.IsEmpty())
			line.m_fSeconds = 2.0;

		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueConfig Config()
	{
		LF_DialogueConfig config = new LF_DialogueConfig();
		config.m_sId = "dispatcher";
		config.m_aLines = {};
		config.m_aLines.Insert(Line("type_robbery", "#K_Robbery", "SOUND_DLG_T_ROBBERY"));
		config.m_aLines.Insert(Line("type_murder", "#K_Murder", "SOUND_DLG_T_MURDER"));
		config.m_aLines.Insert(Line("type_text_only", "#K_TextOnly"));
		config.m_aLines.Insert(Line("place_store", "#K_Store", "SOUND_DLG_P_STORE"));
		config.m_aLines.Insert(Line("place_bank", "#K_Bank", "SOUND_DLG_P_BANK"));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_DialogueConfig config = Config();

		// Each call type picks its own type line and the place's line.
		LF_DispatchVoicePick robbery = null;
		LF_DispatchVoicePick murder = null;
		if (!RedProof())
		{
			robbery = LF_DispatchVoice.Pick(config, "#K_Robbery", "#K_Store");
			murder = LF_DispatchVoice.Pick(config, "#K_Murder", "#K_Bank");
		}
		else
		{
			robbery = new LF_DispatchVoicePick();
			murder = new LF_DispatchVoicePick();
		}

		ctx.True(robbery.m_Type != null && robbery.m_Type.m_sId == "type_robbery", "a robbery call picks the robbery line");
		ctx.True(robbery.m_Place != null && robbery.m_Place.m_sId == "place_store", "the store's place line follows it");
		ctx.True(murder.m_Type != null && murder.m_Type.m_sId == "type_murder", "a murder call picks the murder line");
		ctx.True(murder.m_Place != null && murder.m_Place.m_sId == "place_bank", "the bank's place line follows it");
		ctx.EqualStr("dispatch voice type_robbery place_store", LF_DispatchVoice.LogText(robbery), "the log line names the type and place lines");
		ctx.EqualStr("dispatch voice type_murder place_bank", LF_DispatchVoice.LogText(murder), "the log line follows the call");

		// A missing place line: the call lands, the type is still said.
		LF_DispatchVoicePick noPlace = LF_DispatchVoice.Pick(config, "#K_Robbery", "#LF-Wanted_PlaceGrid 002-001");
		ctx.NotNull(noPlace, "a call with a place nobody recorded still lands");
		ctx.True(noPlace.m_Type != null && noPlace.m_Place == null, "an unrecorded place leaves only the type line");
		ctx.EqualStr("dispatch voice type_robbery none", LF_DispatchVoice.LogText(noPlace), "a missing place reads none");

		// A missing type line, an empty place, a text-only line and no dialogue at all never fail the call.
		LF_DispatchVoicePick noType = LF_DispatchVoice.Pick(config, "#K_Unknown", "#K_Store");
		ctx.True(noType.m_Type == null && noType.m_Place != null, "an offence with no line leaves only the place line");
		ctx.True(LF_DispatchVoice.Pick(config, "#K_TextOnly", "").m_Type == null, "a text-only line is not voiced");
		LF_DispatchVoicePick nothing = LF_DispatchVoice.Pick(null, "#K_Robbery", "#K_Store");
		ctx.True(nothing != null && nothing.m_Type == null && nothing.m_Place == null, "no dispatcher dialogue: an empty pick, no crash");
		ctx.EqualStr("dispatch voice none none", LF_DispatchVoice.LogText(nothing), "nothing picked reads none none");
	}
}
