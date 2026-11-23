// red-proof: batched run `tools\cli test --no-build --red LF_Test_RumorOverlay` (RED-OK line in the PR): RedProof() skips installing the pack source and skips the voice lookup, so no overlay row, crime giver or voice is merged and no line is found.
//! residents/rumor-overlay (#4569, slice of #4170, plan W5.4): the rumor config is an overlay list section.
//! A pack file of `LF_RumorsConfig` listed in the mission header adds its rows by id (the core ships none),
//! its crime givers and its recorded conversations once each; a later file patches a row's place and
//! removes another by id (the override verbs); no pack leaves the core file alone. The told rumor finds its
//! recorded line by text key in the listed conversations, holds its caption for the clip plus a short read,
//! and a key no conversation holds finds nothing (a caption-only toast).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RumorOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{5C57B83431D96B97}Configs/Tests/LF_Test_RumorOverlay.conf";
	static const ResourceName OVERLAY_EDIT = "{DE3B18E71B4D2CE4}Configs/Tests/LF_Test_RumorOverlayEdit.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/rumor-overlay";
	}

	//------------------------------------------------------------------------------------------------
	protected void UseHeader(array<ResourceName> overlays)
	{
		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		foreach (ResourceName overlay : overlays)
			header.m_aLF_ConfigOverlays.Insert(overlay);

		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_RumorsConfig.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RumorDef Row(LF_RumorsConfig config, string id)
	{
		if (!config || !config.m_aRumors)
			return null;

		foreach (LF_RumorDef def : config.m_aRumors)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected int Count(array<string> list, string item)
	{
		int found;
		if (!list)
			return 0;

		foreach (string entry : list)
		{
			if (entry == item)
				found++;
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_RumorsConfig.ResetStatics();
		LF_RumorVoice.ResetStatics();

		// No pack: the core file alone ships no rows and the bar and the informant as crime givers.
		LF_RumorsConfig core = LF_RumorsConfig.Get();
		ctx.NotNull(core, "the core rumor config loads");
		if (!core)
			return;

		ctx.Equal(0, core.m_aRumors.Count(), "without a pack the core ships no rumor rows");
		ctx.Equal(1, Count(core.m_aCrimeGivers, "informant"), "the informant is a core crime giver");
		ctx.Equal(0, Count(core.m_aCrimeGivers, "pawnbroker"), "a pawnbroker is no crime giver without the pack");

		// A pack file in the header adds its rows, crime givers and voices.
		array<ResourceName> overlays = {};
		overlays.Insert(OVERLAY);
		UseHeader(overlays);
		LF_RumorsConfig merged = LF_RumorsConfig.Get();
		ctx.NotNull(merged, "the layered rumor config loads");
		if (!merged)
		{
			LF_ConfigService.Reset();
			return;
		}

		ctx.Equal(2, merged.m_aRumors.Count(), "the pack file adds its two rows");
		LF_RumorDef one = Row(merged, "ov_one");
		ctx.NotNull(one, "the row ov_one is offered");
		if (one)
		{
			ctx.EqualStr("ov.place.one", one.m_sPlaceId, "ov_one points at its place");
			ctx.EqualStr("#LF_Test_Rumor_One", one.m_sLineKey, "ov_one keeps its line key");
			ctx.True(one.m_aGivers && one.m_aGivers.Find("gossip") != -1, "ov_one is told by a gossip");
		}

		LF_RumorDef two = Row(merged, "ov_two");
		ctx.True(two && two.m_bCrime, "the row ov_two is a crime row");
		ctx.Equal(1, Count(merged.m_aCrimeGivers, "informant"), "a crime giver the core has is not added twice");
		ctx.Equal(1, Count(merged.m_aCrimeGivers, "pawnbroker"), "the pack's crime giver is added once");
		ctx.Equal(1, merged.m_aVoices.Count(), "the pack's recorded conversation is listed once");

		// A later pack file patches and removes by id.
		overlays.Insert(OVERLAY_EDIT);
		UseHeader(overlays);
		LF_RumorsConfig edited = LF_RumorsConfig.Get();
		ctx.NotNull(edited, "the edited rumor config loads");
		if (!edited)
		{
			LF_ConfigService.Reset();
			return;
		}

		ctx.Equal(1, edited.m_aRumors.Count(), "the later file removed one row by id");
		ctx.True(Row(edited, "ov_two") == null, "ov_two is gone");
		LF_RumorDef moved = Row(edited, "ov_one");
		ctx.NotNull(moved, "ov_one is still offered");
		if (moved)
		{
			ctx.EqualStr("ov.place.moved", moved.m_sPlaceId, "the patch moved the place");
			ctx.EqualStr("#LF_Test_Rumor_One", moved.m_sLineKey, "a field the patch leaves alone stays");
		}

		// The told rumor finds its recorded line by text key in the listed conversations.
		ResourceName clerk = LF_DialogueRegistry.Resolve("clerk");
		LF_DialogueConfig conversation = LF_DialogueConfig.Layered(clerk);
		ctx.NotNull(conversation, "a core recorded conversation to look a line up in");
		if (conversation && conversation.m_aLines && !conversation.m_aLines.IsEmpty())
		{
			LF_DialogueLine wanted = conversation.m_aLines[0];
			LF_RumorsConfig voiced = new LF_RumorsConfig();
			voiced.m_aCrimeGivers = {};
			voiced.m_aRumors = {};
			voiced.m_aVoices = {};
			voiced.m_aVoices.Insert(clerk);
			LF_RumorsConfig.UseForTest(voiced);
			LF_RumorVoice.ResetStatics();

			LF_RumorDef known = LF_RumorDef.Make("v_known", "v.place", wanted.m_sTextKey, false);
			LF_RumorDef unknown = LF_RumorDef.Make("v_unknown", "v.place", "#LF_Test_No_Such_Line", false);
			LF_DialogueConfig found;
			LF_DialogueLine line;
			if (!RedProof())
				line = LF_RumorVoice.FindLine(known, found);

			ctx.True(line != null && line.m_sId == wanted.m_sId, "the row's line is found by its text key in a listed conversation");
			ctx.True(found != null && found.m_sId == conversation.m_sId, "the conversation holding it is named");

			LF_DialogueConfig none;
			ctx.True(LF_RumorVoice.FindLine(unknown, none) == null, "a key no conversation holds finds no line");

			if (line && !line.IsTextOnly())
				ctx.True(LF_RumorVoice.CaptionSeconds(line) > line.m_fSeconds, "the caption holds past the end of the clip");

			ctx.EqualFloat(LF_RumorVoice.CAPTION_SILENT_SECONDS, LF_RumorVoice.CaptionSeconds(null), 0.01, "a row with no recording holds its caption for a fixed read");
		}

		LF_RumorsConfig.ResetStatics();
		LF_RumorVoice.ResetStatics();
		LF_ConfigService.Reset();
	}
}
