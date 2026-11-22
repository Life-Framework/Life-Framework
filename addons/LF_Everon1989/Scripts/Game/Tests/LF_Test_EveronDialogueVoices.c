// red-proof: observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_EveronDialogueVoices`: RedProof() skips loading the dialogue, so each `has a playable dialogue` assertion fails, RED PROOF OK. Before the fix the first run failed on the cafe owner rumour lines (no sound event, length or mouth envelope); green after.
//! 2026-11-22 ("can we ensure all dialogs are voiced"): under the Everon 1989 header (the pack leg of `tools\cli test`)
//! every dialogue an island NPC talks through resolves, with the pack's patches over it, to a conversation whose every
//! line carries a sound: the config names an audio project and each line an event, a length and a mouth envelope. The
//! offline twin is `validate-dialogue-voices` (clip on disk, voice, hash); this one reads what the game loads.
//! Role conversations (built at runtime from strings, no event) are listed in that validator's baseline, not here.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronDialogueVoices : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/dialogue-voices";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDialogue(LF_TestContext ctx, string id)
	{
		string path = LF_DialogueRegistry.Resolve(id);
		ctx.True(!path.IsEmpty(), id + ": the id resolves through the index");
		if (path.IsEmpty())
			return;

		// The act: load the conversation the way an NPC does, the core file with the pack's patches over it.
		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_DialogueConfig.Layered(path);

		ctx.NotNull(config, id + ": has a playable dialogue");
		if (!config)
			return;

		ctx.True(!config.m_sSoundProject.IsEmpty(), id + ": the dialogue names an audio project");
		if (!config.m_aLines)
		{
			ctx.True(false, id + ": the dialogue has no lines");
			return;
		}

		foreach (LF_DialogueLine line : config.m_aLines)
		{
			if (!line)
				continue;

			string where = id + "/" + line.m_sId;
			ctx.True(!line.m_sEvent.IsEmpty(), where + ": has a sound event");
			ctx.True(line.m_fSeconds > 0, where + ": has a length");
			ctx.True(!line.m_sMouth.IsEmpty(), where + ": has a mouth envelope");
		}

		// Every node's line resolves to one of those lines, so what a player hears has a clip.
		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node || node.m_sLine.IsEmpty() || LF_DialogueFlow.IsRaw(node.m_sLine))
				continue;

			ctx.NotNull(LF_DialogueFlow.FindLine(config, node.m_sLine), id + "/" + node.m_sId + ": its line " + node.m_sLine + " exists");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_MissionHeader.GetCurrent(), "the pack leg runs under an LF mission header");

		array<string> ids = {
			"anniversary_mayor", "cafe_owner", "clerk", "dockworker", "fuel_clerk", "gendarme", "harbour_master",
			"night_shift", "old_man", "passerby", "relocated_walker", "resident_anselme", "resident_mireille",
			"resident_store_clerk", "resident_teller", "shipment_go_between", "shopkeeper"
		};
		foreach (string id : ids)
			CheckDialogue(ctx, id);
	}
}
