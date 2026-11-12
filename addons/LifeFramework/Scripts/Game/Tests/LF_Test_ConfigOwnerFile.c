// red-proof: drop the FileExists check in LF_ConfigService.WriteExample and config/owner-file
// fails "an existing example is left alone (expected '{"sentinel":1}', got '{')" (observed 2026-11-12).

//------------------------------------------------------------------------------------------------
//! The owner file on disk: a missing file boots on defaults and writes the example beside it, an
//! existing example is never overwritten, the example itself is a valid owner file, and a real
//! file is read. Uses a scratch folder under $profile, never the server's own config.json.
// tier: LOGIC
class LF_Test_ConfigOwnerFile : LF_Test
{
	static const string DIR = "$profile:LifeFramework/test-config";
	static const string OWNER = "$profile:LifeFramework/test-config/config.json";
	static const string EXAMPLE = "$profile:LifeFramework/test-config/config.example.json";

	override string GetName()
	{
		return "config/owner-file";
	}

	protected static void WriteText(string path, string text)
	{
		FileIO.MakeDirectory(DIR);
		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
			return;
		handle.WriteLine(text);
		handle.Close();
	}

	protected static string ReadFirstLine(string path)
	{
		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
			return string.Empty;
		string line;
		handle.ReadLine(line);
		handle.Close();
		return line;
	}

	override void Run(LF_TestContext ctx)
	{
		FileIO.DeleteFile(OWNER);
		FileIO.DeleteFile(EXAMPLE);
		LF_ConfigService.Reset();

		ctx.False(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE), "a missing file is no owner layer");
		ctx.False(FileIO.FileExists(OWNER), "a missing owner file is not created");
		ctx.True(FileIO.FileExists(EXAMPLE), "the example is written beside it");

		// The example is itself a valid owner file with a shops section.
		FileHandle example = FileIO.OpenFile(EXAMPLE, FileMode.READ);
		string text;
		string line;
		if (example)
		{
			while (example.ReadLine(line) >= 0)
			{
				text += line + "\n";
			}
			example.Close();
		}
		ctx.True(LF_ConfigService.LoadOwnerString(text, EXAMPLE), "the example parses");
		LF_ShopPricesConfig fromExample = LF_Test_ConfigMergeFixture.MakeCore();
		fromExample.ApplyLayers(string.Empty);
		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(fromExample, LF_Test_ConfigMergeFixture.FOOD);
		ctx.True(food && food.m_iBuyPrice == 10, "the example's shops section applies");

		// An existing example is never overwritten.
		WriteText(EXAMPLE, "{\"sentinel\":1}");
		LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE);
		ctx.EqualStr("{\"sentinel\":1}", ReadFirstLine(EXAMPLE), "an existing example is left alone");

		// A real owner file is read and applied; the example is not touched.
		WriteText(OWNER, "{\"schemaVersion\":1,\"shops\":{\"defaultSellPrice\":33}}");
		ctx.True(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE), "an owner file is an owner layer");
		LF_ShopPricesConfig fromFile = LF_Test_ConfigMergeFixture.MakeCore();
		fromFile.ApplyLayers(string.Empty);
		ctx.Equal(33, fromFile.m_iDefaultSellPrice, "the owner file's value applies");
		ctx.EqualStr("{\"sentinel\":1}", ReadFirstLine(EXAMPLE), "loading an owner file writes no example");

		// A malformed owner file boots on defaults.
		WriteText(OWNER, "{\"shops\":{");
		ctx.False(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE), "a malformed owner file is no owner layer");

		FileIO.DeleteFile(OWNER);
		FileIO.DeleteFile(EXAMPLE);
		LF_ConfigService.Reset();
	}
};
