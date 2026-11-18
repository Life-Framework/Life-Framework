// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_LongStringLimits`: RED PROOF OK, with the build step skipped (a 100-char string) the splice and Format assertions fail (`a splice through a long Substring tail comes back cut (expected 8203, got 100)`). Green with --only.
//! Pins where an Enforce string is cut (measured 2026-11-17, Reforger 26266485; the table and the
//! production sites are in docs/architecture/enforce-string-limits.md). `+`, `+=`, `Replace`, `Split`,
//! `IndexOf`, a JSON context and a file round trip keep a 140000-char string whole. `Substring` caps its
//! RESULT at 8191 chars and `string.Format` caps its result at 8191 (8190 when the long argument is not
//! first). An engine update that moves any of these fails here, and the doc table is then stale.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LongStringLimits : LF_Test
{
	//! The longest string `Substring` returns: 8192 bytes less the terminator.
	protected static const int CUT = 8191;
	protected static const int LONG_LENGTH = 12000;
	protected static const string SCRATCH_FILE = "$profile:lf-test-long-string.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "string/long-string-limits";
	}

	//------------------------------------------------------------------------------------------------
	//! A string of exactly this length, built with `+` only: `Substring` would cut it at CUT.
	protected static string Make(int length)
	{
		string chunk = "0123456789abcdef";
		string text = "";
		while (text.Length() + 16 <= length)
		{
			text = text + chunk;
		}

		while (text.Length() < length)
		{
			text = text + "x";
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The red proof builds a short string, so every "past the limit" assertion below fails.
		int length = 100;
		if (!RedProof())
			length = LONG_LENGTH;

		string longText = Make(length);
		ctx.Equal(length, longText.Length(), "the long string is built whole with +");

		// What keeps a long string whole.
		string joined = longText + "ABC";
		ctx.Equal(length + 3, joined.Length(), "+ keeps a string past 8191");
		string appended = "";
		appended += longText;
		appended += "Z";
		ctx.Equal(length + 1, appended.Length(), "+= keeps a string past 8191");
		string replaced = longText;
		int count = replaced.Replace("0", "00");
		ctx.Equal(length + count, replaced.Length(), "Replace keeps a string past 8191");
		ctx.Equal(length, (longText + "TAIL").IndexOf("TAIL"), "IndexOf finds a match past 8191");
		array<string> parts = {};
		(longText + ";" + longText).Split(";", parts, true);
		ctx.Equal(2, parts.Count(), "Split cuts a long string at its separators");
		ctx.Equal(length, parts[0].Length(), "Split keeps a long piece whole");

		// A JSON context: the save path.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		saveContext.WriteValue("k", longText);
		string json = saveContext.SaveToString();
		ctx.Equal(length + 8, json.Length(), "a JSON context keeps a long string value (SaveToString)");
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(json), "a long JSON string parses");
		string back;
		loadContext.ReadValue("k", back);
		ctx.Equal(length, back.Length(), "a JSON context reads a long string back whole");

		// A file: the other way out.
		FileHandle writer = FileIO.OpenFile(SCRATCH_FILE, FileMode.WRITE);
		if (writer)
		{
			writer.Write(longText);
			writer.Close();
		}

		string fromFile;
		FileHandle reader = FileIO.OpenFile(SCRATCH_FILE, FileMode.READ);
		if (reader)
		{
			reader.ReadLine(fromFile);
			reader.Close();
		}

		FileIO.DeleteFile(SCRATCH_FILE);
		ctx.Equal(length, fromFile.Length(), "a file keeps a long line whole");

		// What cuts it: Substring caps its result.
		string head = longText.Substring(0, length);
		ctx.Equal(CUT, head.Length(), "Substring returns at most 8191 chars (a copy of 12000 comes back 8191)");
		string tail = longText.Substring(10, length - 10);
		ctx.Equal(CUT, tail.Length(), "a Substring tail past 8191 is cut");
		// A length past the end of the source is a VM exception, not a clamp, so it is bounded here.
		string window = longText.Substring(0, Math.Min(4096, length));
		ctx.Equal(4096, window.Length(), "a Substring window under the cap is exact");

		// The splice that failed PR #2520: head + new text + a long tail, about 8203 chars not 12000.
		string spliced = longText.Substring(0, 10) + "49" + longText.Substring(12, length - 12);
		ctx.Equal(10 + 2 + CUT, spliced.Length(), "a splice through a long Substring tail comes back cut");

		// The workaround: pieces under the cap, joined with + (or written one by one to a file).
		string rebuilt = "";
		for (int from = 0; from < length; from += 2048)
		{
			rebuilt = rebuilt + longText.Substring(from, Math.Min(2048, length - from));
		}

		ctx.Equal(length, rebuilt.Length(), "2 KB Substring pieces joined with + rebuild the whole string");
		ctx.True(rebuilt == longText, "the rebuilt string equals the original");

		// string.Format caps its result: the long argument first loses 1 char past 8191, second loses 2.
		string formatFirst = string.Format("%1|%2", longText, "x");
		ctx.Equal(CUT, formatFirst.Length(), "string.Format returns at most 8191 chars (long argument first)");
		string formatSecond = string.Format("%1|%2", "x", longText);
		ctx.Equal(CUT - 1, formatSecond.Length(), "string.Format returns at most 8190 chars (long argument second)");
	}
}
