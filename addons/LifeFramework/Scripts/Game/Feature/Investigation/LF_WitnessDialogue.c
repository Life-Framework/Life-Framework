//------------------------------------------------------------------------------------------------
//! What a questioned witness says (#615). Every town NPC gets a stable name from where it stands,
//! and an NPC that saw nothing answers with a noncommittal line (neither confirming nor denying)
//! instead of telling the officer what the officer saw. Pure functions; the actions call them.
class LF_WitnessDialogue
{
	static const int UNSURE_VARIANTS = 4;

	static const int NAME_COUNT = 12;

	//------------------------------------------------------------------------------------------------
	//! Name for a seed: the same seed always gives the same name, different seeds spread over the lists.
	static string NameFromSeed(int seed)
	{
		int s = Math.AbsInt(seed);
		array<string> firstNames = {"Anna", "Marcel", "Odile", "Henri", "Claire", "Luc", "Margot", "Paul", "Sylvie", "Rene", "Jeanne", "Michel"};
		array<string> surnames = {"Dupont", "Lefevre", "Moreau", "Garnier", "Roux", "Fontaine", "Bonnet", "Girard", "Chevalier", "Perrin", "Morel", "Vidal"};
		int first = s % NAME_COUNT;
		int last = (s / NAME_COUNT) % NAME_COUNT;
		return firstNames[first] + " " + surnames[last];
	}

	//------------------------------------------------------------------------------------------------
	//! Name of a town NPC. Seeded from its spot in the world, so it survives restarts and is the
	//! same for every officer who asks.
	static string NameOf(IEntity npc)
	{
		if (!npc)
			return NameFromSeed(0);

		vector p = npc.GetOrigin();
		int seed = Math.Round(p[0] * 10) * 31 + Math.Round(p[2] * 10) * 17 + Math.Round(p[1] * 10);
		return NameFromSeed(seed);
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key of the noncommittal line for a roll (any int; wraps over the variants).
	static string UnsureKey(int roll)
	{
		switch (Math.AbsInt(roll) % UNSURE_VARIANTS)
		{
			case 0:
				return "#LF-Witness_Unsure1";
			case 1:
				return "#LF-Witness_Unsure2";
			case 2:
				return "#LF-Witness_Unsure3";
		}

		return "#LF-Witness_Unsure4";
	}

	//------------------------------------------------------------------------------------------------
	//! Toast key for an officer's question. No scene: the witness saw nothing and says so without
	//! confirming or denying. A scene but nothing recalled: they saw it, remember nothing useful.
	static string AnswerKey(bool sawScene, int said, int roll)
	{
		if (!sawScene)
			return UnsureKey(roll);

		if (said <= 0)
			return "#LF-Witness_ForgotSome";

		return "#LF-Witness_GaveStatement";
	}
}
