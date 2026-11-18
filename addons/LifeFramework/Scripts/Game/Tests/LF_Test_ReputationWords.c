// red-proof: observed 2026-11-18 with `tools/cli test --no-build --red LF_Test_ReputationWords`: RED-OK, all 1 listed test(s) fail with the hook on, first `the town and the police are listed, the underworld is not (expected 2, got 3)`. Green with `--only`: passed=1 failed=0.
//! Standing in words (#2705): for each tier pair `LF_ReputationWords.Pairs` gives the faction's label
//! key and the tier word key from its own table, skips a faction whose m_bShown is false, and the line
//! the player reads has no digit. The shipped config shows the town and the police, never the underworld.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationWords : LF_Test
{
	override string GetName()
	{
		return "reputation/words";
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDigit(string text)
	{
		string digits = "0123456789";
		for (int i = 0; i < text.Length(); i++)
		{
			if (digits.Contains(text.Get(i)))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Three factions with their own tier word keys; the underworld not shown.
	protected LF_ReputationConfig MakeConfig()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		array<string> ids = {"TOWN", "POLICE", "UNDERWORLD"};
		foreach (string id : ids)
		{
			LF_ReputationFactionDef faction = LF_ReputationFactionDef.Make(id, thresholds, 0);
			faction.m_sLabel = "#Test_Label_" + id;
			faction.m_aTierLabels = {};
			for (int tier = -2; tier <= 3; tier++)
			{
				faction.m_aTierLabels.Insert("#Test_" + id + "_Tier" + tier.ToString());
			}

			faction.m_bShown = (id != "UNDERWORLD");
			config.m_aFactions.Insert(faction);
		}

		config.m_aAwards = {};
		config.m_aGates = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ReputationConfig config = MakeConfig();
		array<string> labels = {};
		array<string> words = {};

		// RedProof: ignore the shown flag, so the underworld's line appears and the pair count is wrong (`--red`).
		if (RedProof())
			config.FindFaction("UNDERWORLD").m_bShown = true;

		LF_ReputationWords.Pairs(config, 1, -1, 3, labels, words);
		ctx.Equal(2, labels.Count(), "the town and the police are listed, the underworld is not");
		ctx.Equal(2, words.Count(), "one tier word per listed faction");
		if (labels.Count() == 2 && words.Count() == 2)
		{
			ctx.EqualStr("#Test_Label_TOWN", labels[0], "the town comes first, under its own label");
			ctx.EqualStr("#Test_TOWN_Tier1", words[0], "the town word is the town table's word for tier 1");
			ctx.EqualStr("#Test_Label_POLICE", labels[1], "the police come second, under their own label");
			ctx.EqualStr("#Test_POLICE_Tier-1", words[1], "the police word is the police table's word for tier -1");
		}

		// Each faction reads its own table: the same tier gives different words.
		LF_ReputationWords.Pairs(config, 0, 0, 0, labels, words);
		if (words.Count() == 2)
			ctx.True(words[0] != words[1], "the factions use their own tier word keys");

		// A tier the table has no word for leaves that faction out instead of printing an empty line.
		LF_ReputationWords.Pairs(config, 9, 0, 0, labels, words);
		ctx.Equal(1, labels.Count(), "a tier with no word leaves its faction out");

		// No config, no lines.
		LF_ReputationWords.Pairs(null, 0, 0, 0, labels, words);
		ctx.Equal(0, labels.Count(), "no config gives no lines");

		// The shipped config: the town and the police only, and the keys it holds carry words, not numbers.
		LF_ReputationConfig.Reload();
		LF_ReputationWords.Pairs(LF_ReputationConfig.Get(), 2, 0, 3, labels, words);
		ctx.Equal(2, labels.Count(), "the shipped config shows the town and the police");
		ctx.True(labels.Find("#LF-Reputation_Faction_Underworld") == -1, "the shipped underworld label is never listed");
		ctx.False(HasDigit(LF_ReputationWords.KEY_LINE), "the line key has no digit");
		foreach (string label : labels)
		{
			ctx.False(HasDigit(label), "no digit in the label key " + label);
		}

		// The line as text: translated keys never leave a digit behind. (An untranslated key keeps its
		// tier number, so the check applies only where the language is loaded.)
		string line = LF_ReputationWords.Line(LF_ReputationConfig.Get(), 2, 0, 3);
		ctx.True(!line.IsEmpty(), "the shipped config gives a line");
		if (!line.StartsWith("#") && !line.Contains("#LF-"))
			ctx.False(HasDigit(line), "the line has no digit: " + line);

		// The clerk's "My file": earned standing is said in words, one entry per shown faction, never the underworld.
		string uid = "test-reputation-words";
		LF_Reputation.Forget(uid);
		LF_Reputation.Add(uid, "TOWN", 150, "test");
		LF_Reputation.Add(uid, "UNDERWORLD", 150, "test");
		array<string> spoken = {};
		if (!RedProof())
			LF_ReputationWords.Spoken(uid, spoken);

		ctx.True(spoken.Count() == 2 || spoken.Count() == 4, "the clerk says one entry per shown faction (two when translated, label and word each when not)");
		foreach (string entry : spoken)
		{
			ctx.False(entry.IsEmpty(), "no empty entry");
			ctx.False(entry.Contains(LF_DialogueFlow.SEPARATOR), "an entry never holds the separator of the spoken list");
			ctx.False(entry.Contains("Underworld"), "the underworld is never said");
			ctx.True(entry != "#LF-Reputation_Faction_Underworld", "the underworld label is never said");
		}

		LF_Reputation.Forget(uid);
		LF_ReputationConfig.Reload();
	}
}
