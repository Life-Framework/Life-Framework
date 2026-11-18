//------------------------------------------------------------------------------------------------
//! Where you stand, in words (#2705, docs/architecture/reputation.md section 5). The ID card sheet
//! prints one line ("Locals: Known. The law: Unknown."), and the clerk's "My file" reply says the
//! same line by line. A line is the faction's label and the faction's tier word: never a number, a
//! bar, or a faction whose config says m_bShown is false (the underworld).
//!
//! The words are the `#` keys the config holds (LF_ReputationFactionDef.m_sLabel and m_aTierLabels),
//! so a pack overlay re-words them without touching this class.
//------------------------------------------------------------------------------------------------
class LF_ReputationWords
{
	//! "%1: %2." with the faction label and the tier word.
	static const string KEY_LINE = "#LF-Reputation_File_Line";

	//------------------------------------------------------------------------------------------------
	//! Pure: the tier of `faction` out of the three the caller holds.
	protected static int TierOf(string faction, int town, int police, int underworld)
	{
		if (faction == LF_Reputation.FACTION_TOWN)
			return town;
		if (faction == LF_Reputation.FACTION_POLICE)
			return police;

		return underworld;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the faction label key and the tier word key of each faction a screen may show, in wire
	//! order (town, police, underworld). A faction missing from the config, one that is not shown, or
	//! one with no word for the tier is left out.
	static void Pairs(LF_ReputationConfig config, int town, int police, int underworld, notnull array<string> labelKeys, notnull array<string> wordKeys)
	{
		labelKeys.Clear();
		wordKeys.Clear();
		if (!config)
			return;

		array<string> factions = {};
		LF_ReputationSync.FactionOrder(factions);
		foreach (string faction : factions)
		{
			LF_ReputationFactionDef def = config.FindFaction(faction);
			if (!def || !def.m_bShown)
				continue;

			string word = LF_Reputation.TierWordKey(def, TierOf(faction, town, police, underworld));
			if (word.IsEmpty() || def.m_sLabel.IsEmpty())
				continue;

			labelKeys.Insert(def.m_sLabel);
			wordKeys.Insert(word);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return One faction's line as text ("Locals: Known."), with the keys translated.
	static string FactionLine(string labelKey, string wordKey)
	{
		return LF_Terms.Text(KEY_LINE, LF_Terms.Text(labelKey), LF_Terms.Text(wordKey));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The faction lines on one row ("Locals: Known. The law: Unknown."); empty when no
	//! faction is shown. Translates, so it runs where the language is loaded (a client).
	static string Line(LF_ReputationConfig config, int town, int police, int underworld)
	{
		array<string> labels = {};
		array<string> words = {};
		Pairs(config, town, police, underworld, labels, words);

		string line;
		for (int i = 0; i < labels.Count(); i++)
		{
			if (!line.IsEmpty())
				line += " ";

			line += FactionLine(labels[i], words[i]);
		}

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: this player's own standing as one row, from the tiers the server last sent. Empty until
	//! the first send arrived, or when the config does not resolve.
	static string ClientLine()
	{
		int town;
		int police;
		int underworld;
		if (!LF_ReputationSync.ClientTier(LF_Reputation.FACTION_TOWN, town))
			return string.Empty;

		LF_ReputationSync.ClientTier(LF_Reputation.FACTION_POLICE, police);
		LF_ReputationSync.ClientTier(LF_Reputation.FACTION_UNDERWORLD, underworld);
		return Line(LF_ReputationConfig.Get(), town, police, underworld);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what the clerk says about the character's file, one entry per faction. Each entry is
	//! the line as text; where the server has no language loaded (the key comes back untranslated) the
	//! label key and the word key go as two entries the client translates.
	static void Spoken(string playerUid, notnull array<string> spoken)
	{
		array<string> labels = {};
		array<string> words = {};
		Pairs(LF_ReputationConfig.Get(), LF_Reputation.Tier(playerUid, LF_Reputation.FACTION_TOWN), LF_Reputation.Tier(playerUid, LF_Reputation.FACTION_POLICE), LF_Reputation.Tier(playerUid, LF_Reputation.FACTION_UNDERWORLD), labels, words);
		for (int i = 0; i < labels.Count(); i++)
		{
			string text = FactionLine(labels[i], words[i]);
			if (text.StartsWith("#"))
			{
				spoken.Insert(labels[i]);
				spoken.Insert(words[i]);
				continue;
			}

			// The spoken list is split on ";".
			text.Replace(LF_DialogueFlow.SEPARATOR, ",");
			spoken.Insert(text);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Action: "What is in my file?" The clerk says where the player stands with the town and the
//! police, in words (#2705).
class LF_DialogueHook_StandingFile : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		string uid = LF_Utils.GetPlayerUid(player);
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(LF_Reputation.FEATURE, "my file refused: no account id for this player");
			return;
		}

		LF_ReputationWords.Spoken(uid, spoken);
		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("the clerk read the file of %1 (%2 entry/ies)", uid, spoken.Count()));
	}
}
