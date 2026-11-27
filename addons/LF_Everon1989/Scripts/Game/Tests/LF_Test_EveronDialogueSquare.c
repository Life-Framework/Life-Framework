// red-proof: observed 2026-11-23 (#4463): the first run, before any spec or recording existed, failed every `resolves through the pack's dialogue index` assertion (no index row for gossip_odile, gossip_colette or gendarme_evening); green after the three conversations were recorded (5998 assertions). `tools\cli test --no-build --red LF_Test_EveronDialogueSquare`: RED PROOF OK (the hook skips the load, so every conversation check fails).
//! #4463 (W1.4 of the world chapter, #4170): under the Everon 1989 header (the pack leg of `tools\cli test`) the square
//! cast speaks. The two bench gossips (gossip_odile, gossip_colette) and the evening gendarme (gendarme_evening) each
//! resolve, through the pack's dialogue index, to a conversation of at least a dozen lines. Every line has a recording
//! on disk, a sound event, a length and a caption, the conversation is playable, each man has his own audio project, and
//! no caption holds a French word (US English everywhere: the W1.5 rule; place and person names stay).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronDialogueSquare : LF_Test
{
	static const int MIN_LINES = 12;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/dialogue-square";
	}

	//------------------------------------------------------------------------------------------------
	//! Spoken French that has no place in a US English caption; the full sign-word list is validate-us-english.
	protected static void FrenchWords(array<string> words)
	{
		words.InsertAll({
			"bonjour", "bonsoir", "salut", "merci", "monsieur", "madame", "mademoiselle", "mme", "mlle", "oui", "voila",
			"ami", "amis", "chere", "cher", "gendarmerie", "mairie", "boulangerie", "pharmacie", "banque", "poste", "ouvert",
			"ferme", "au revoir", "mon dieu", "n'est-ce pas", "enfin", "alors", "bien sur", "quelle", "peut-etre"
		});
	}

	//------------------------------------------------------------------------------------------------
	//! The lower-cased caption with its punctuation as spaces, padded, so a whole word is " word ".
	protected static string Plain(string text)
	{
		string plain = text;
		plain.ToLower();
		array<string> marks = { ".", ",", "!", "?", ";", ":", "\"", "(", ")", "-" };
		foreach (string mark : marks)
			plain.Replace(mark, " ");

		return " " + plain + " ";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCast(LF_TestContext ctx, string id, array<string> french, array<string> projects)
	{
		string path = LF_DialogueRegistry.Resolve(id);
		ctx.True(!path.IsEmpty(), id + ": resolves through the pack's dialogue index");

		// The act: load the conversation the way an NPC does. Red proof: no load.
		LF_DialogueConfig config;
		if (!RedProof() && !path.IsEmpty())
			config = LF_DialogueConfig.Layered(path);

		ctx.NotNull(config, id + ": has a playable dialogue");
		if (!config)
			return;

		ctx.EqualStr(id, config.m_sId, id + ": the conversation is its own");
		ctx.True(!config.m_sSoundProject.IsEmpty(), id + ": names an audio project");
		ctx.True(projects.Find(config.m_sSoundProject) == -1, id + ": has its own audio project, no other NPC's voice");
		projects.Insert(config.m_sSoundProject);
		ctx.True(HasText(config.m_sSpeakerKey), id + ": the speaker has a name");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), id + ": the conversation is playable");

		if (!config.m_aLines)
		{
			ctx.True(false, id + ": the dialogue has no lines");
			return;
		}

		ctx.True(config.m_aLines.Count() >= MIN_LINES, string.Format("%1: has at least %2 lines (%3)", id, MIN_LINES, config.m_aLines.Count()));

		// The clip is a wav the audio project names by {GUID}path (validate-guid-paths checks each one); loading it
		// by a bare path logged "Wrong GUID/name" for every line, so the project is what loads here. One load per
		// conversation: every line of it names the same project, and the check still runs once per line.
		Resource project;
		if (!config.m_sSoundProject.IsEmpty())
			project = Resource.Load(config.m_sSoundProject);
		bool projectLoads = project && project.IsValid();

		// Each whole-word needle, padded, built once instead of once per line and word.
		array<string> needles = {};
		foreach (string frenchWord : french)
			needles.Insert(" " + frenchWord + " ");

		foreach (LF_DialogueLine line : config.m_aLines)
		{
			if (!line)
				continue;

			string where = id + "/" + line.m_sId;
			ctx.True(!line.m_sEvent.IsEmpty(), where + ": has a sound event");
			ctx.True(line.m_fSeconds > 0, where + ": has a length");
			ctx.True(projectLoads, where + ": its audio project loads");

			ctx.True(HasText(line.m_sTextKey), where + ": has its caption");
			string caption = Plain(WidgetManager.Translate(line.m_sTextKey));
			foreach (int w, string word : french)
				ctx.False(caption.Contains(needles[w]), where + ": no French word (" + word + ")");
		}

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node || !node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				ctx.True(HasText(option.m_sTextKey), id + ": reply " + option.m_sTextKey + " has its text");
				string reply = Plain(WidgetManager.Translate(option.m_sTextKey));
				foreach (int w, string word : french)
					ctx.False(reply.Contains(needles[w]), id + ": reply " + option.m_sTextKey + " has no French word (" + word + ")");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_MissionHeader.GetCurrent(), "the pack leg runs under an LF mission header");

		array<string> french = {};
		FrenchWords(french);

		// Every audio project the pack's other voiced NPCs use: the square cast shares none of them.
		array<string> projects = {};
		array<string> others = { "gendarme", "resident_mireille", "resident_teller", "resident_anselme", "old_man", "cafe_owner", "night_shift" };
		foreach (string other : others)
		{
			string otherPath = LF_DialogueRegistry.Resolve(other);
			if (otherPath.IsEmpty())
				continue;

			LF_DialogueConfig otherConfig = LF_DialogueConfig.Layered(otherPath);
			if (otherConfig && !otherConfig.m_sSoundProject.IsEmpty())
				projects.Insert(otherConfig.m_sSoundProject);
		}

		array<string> cast = { "gossip_odile", "gossip_colette", "gendarme_evening" };
		foreach (string id : cast)
			CheckCast(ctx, id, french, projects);
	}
}
