// red-proof: new test, batched `tools/cli test --no-build --red LF_Test_LawBookRead` 2026-11-23: exit 0, RedProof() builds each page from no codes and spawns no lectern, so the page-rule, per-section and lectern checks fail; green with the hook off.
//! #4429: the law book is read by section at the lectern. One Read row per section on the prefab; the
//! page for a section lists exactly that section's codes (catalog order) with their fine and time, a jail-only
//! code says so and shows no fine, and each section is a registered board kind that names the read action.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LawBookRead : LF_Test
{
	protected static const ResourceName LECTERN = "{A4C17E2B9D305F68}Prefabs/Props/Police/LF_LawBookLectern.et";

	override string GetName()
	{
		return "law/book-read";
	}

	//------------------------------------------------------------------------------------------------
	protected static string LineText(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part.GetText();
		}

		return "<no part " + name + ">";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrimeEntry Code(string id, int fine, int seconds, bool jailOnly)
	{
		LF_CrimeEntry code = LF_CrimeEntry.Make(id, 0, fine);
		code.m_iSentenceSeconds = seconds;
		code.m_bJailOnly = jailOnly;
		return code;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPageRules(LF_TestContext ctx)
	{
		array<ref LF_CrimeEntry> codes = {};
		if (!RedProof())
		{
			codes.Insert(Code("Test Speeding", 250, 300, false));
			codes.Insert(Code("Test Murder", 0, 900, true));
			codes.Insert(Code("Test Insult", 50, 0, false));
		}

		LF_BoardAnswer answer = LF_LawBookRead.BuildAnswerFor(codes);
		ctx.Equal(3, answer.m_aTexts.Count(), "one text per code");
		ctx.Equal(9, answer.m_aNumbers.Count(), "three numbers per code");
		if (answer.m_aNumbers.Count() < 9)
			return;

		ctx.Equal(250, answer.m_aNumbers[0], "the first code's fine");
		ctx.Equal(900, answer.m_aNumbers[4], "the second code's time in seconds");
		ctx.Equal(1, answer.m_aNumbers[5], "the jail-only flag crosses");

		array<ref LF_UIPart> parts = LF_LawBookRead.BuildParts(LF_LawSection.ROAD, answer);
		string speeding = LineText(parts, "Code0");
		ctx.True(speeding.Contains("$250") || speeding.Contains("250"), "a code line shows its fine: " + speeding);
		ctx.True(speeding.Contains("5"), "a code line shows its time in minutes: " + speeding);
		string murder = LineText(parts, "Code1");
		ctx.True(murder.Contains("15"), "a jail-only line shows the time: " + murder);
		ctx.False(murder.Contains("$"), "a jail-only line shows no fine: " + murder);
		string insult = LineText(parts, "Code2");
		ctx.True(insult.Contains("50"), "a fine-only line shows its fine: " + insult);
		ctx.False(insult.Contains("min"), "a code with no time shows none: " + insult);

		// An empty page says so.
		array<ref LF_CrimeEntry> none = {};
		array<ref LF_UIPart> empty = LF_LawBookRead.BuildParts(LF_LawSection.WEAPONS, LF_LawBookRead.BuildAnswerFor(none));
		ctx.False(LineText(empty, "None").StartsWith("<no part"), "an empty section paints its empty line");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSections(LF_TestContext ctx)
	{
		LF_BoardProviders.ResetStatics();
		int total;
		int sections;
		for (int value = LF_LawSection.CRIMINAL; value <= LF_LawSection.WEAPONS; value++)
		{
			sections++;
			LF_LawSection section = value;
			string kind = LF_LawBookRead.KindOf(section);
			LF_BoardProvider provider = LF_BoardProviders.Find(kind);
			ctx.True(provider != null, "the " + kind + " kind is registered");
			if (!provider)
				continue;

			ctx.EqualStr(LF_LawBookRead.READ_ACTION, provider.GetReadAction(), kind + " names the lectern's read action");

			// The page lists exactly the catalog's codes of that section, with the catalog's amounts.
			array<LF_CrimeEntry> expected = {};
			LF_LawCode.CollectSection(section, expected);
			LF_BoardAnswer answer = provider.Answer(1);
			ctx.NotNull(answer, kind + " answers");
			if (!answer)
				continue;

			if (RedProof())
				continue;

			ctx.Equal(expected.Count(), answer.m_aTexts.Count(), kind + " lists every code of its section");
			total += expected.Count();
			array<ref LF_UIPart> parts = provider.Paint(answer);
			foreach (int index, LF_CrimeEntry entry : expected)
			{
				if (index >= answer.m_aTexts.Count())
					break;

				ctx.EqualStr(entry.m_sCrimeId, answer.m_aTexts[index], kind + " row " + index + " is the catalog's code");
				string line = LineText(parts, "Code" + index);
				if (entry.m_bJailOnly)
					continue;

				ctx.True(line.Contains(LF_MoneyFormat.FormatMoney(entry.m_iFine)), kind + " row " + index + " shows the fine " + entry.m_iFine + ": " + line);
			}
		}

		ctx.Equal(4, sections, "the book has four sections");
		ctx.True(total > 0, "the catalog puts at least one code in a section");
		ctx.True(LF_BoardProviders.Find("law_nonsense") == null, "an unknown section kind has no provider");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLectern(LF_TestContext ctx)
	{
		if (RedProof())
			return;

		IEntity lectern = LF_Utils.SpawnEntityPrefab(LECTERN, "0 -20 0", "0 0 0", false);
		if (!lectern)
		{
			ctx.Fail("the law book lectern does not spawn");
			return;
		}

		ActionsManagerComponent actions = ActionsManagerComponent.Cast(lectern.FindComponent(ActionsManagerComponent));
		ctx.NotNull(actions, "the lectern has an actions manager");
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			array<string> seen = {};
			foreach (BaseUserAction base : list)
			{
				LF_LawBookReadAction read = LF_LawBookReadAction.Cast(base);
				if (!read)
					continue;

				string name;
				read.GetActionNameScript(name);
				if (seen.Find(name) != -1)
					ctx.Fail("two Read rows share the name " + name);

				seen.Insert(name);
			}

			ctx.Equal(4, seen.Count(), "one Read row per law-book section");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(lectern);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckPageRules(ctx);
		CheckSections(ctx);
		CheckLectern(ctx);
	}
}
