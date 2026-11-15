// red-proof: Run before LF_HostSay.c existed: `tools\cli test --no-build --only LF_Test_HostSay` failed (scripts do not compile); the batched `--red` run then failed every check (ProcessFile skipped: nothing sent, no file consumed).

// tier: LOGIC
//! `tools\cli host say` (#1453): the server reads $profile:LifeFramework/host_say.txt, toasts every
//! player once, and deletes the file. An empty file sends nothing and warns; text past 200
//! characters is cut. The files here live under a test folder, never the real say file.
class LF_Test_HostSay : LF_Test
{
	protected static const string DIR = "$profile:LifeFramework/test";

	override string GetName()
	{
		return "ops/host-say";
	}

	override void Run(LF_TestContext ctx)
	{
		FileIO.MakeDirectory("$profile:LifeFramework");
		FileIO.MakeDirectory(DIR);

		// One line: one toast, the file is gone.
		string one = DIR + "/host_say_one.txt";
		WriteFile(one, "Hotfix restart in 10 minutes");
		int before = LF_HostSay.GetSentCount();
		string sent = "";
		if (!RedProof())
			sent = LF_HostSay.ProcessFile(one);
		ctx.EqualStr("Hotfix restart in 10 minutes", sent, "the one-line file's text was sent");
		ctx.Equal(before + 1, LF_HostSay.GetSentCount(), "one line gives exactly one NotifyAll");
		ctx.False(FileIO.FileExists(one), "the say file is deleted once read");

		// A file the poll has already consumed sends nothing more.
		string again = LF_HostSay.ProcessFile(one);
		ctx.EqualStr("", again, "a missing file sends nothing");
		ctx.Equal(before + 1, LF_HostSay.GetSentCount(), "a missing file is not a second toast");

		// Empty file: nothing sent, a warn, and the file is removed so it does not warn every poll.
		string empty = DIR + "/host_say_empty.txt";
		WriteFile(empty, "");
		before = LF_HostSay.GetSentCount();
		string emptySent = "x";
		if (!RedProof())
			emptySent = LF_HostSay.ProcessFile(empty);
		ctx.EqualStr("", emptySent, "an empty file sends no text");
		ctx.Equal(before, LF_HostSay.GetSentCount(), "an empty file sends no toast");
		ctx.True(LF_HostSay.GetLastLog().Contains("empty"), "an empty file logs a warn that says so");
		ctx.False(FileIO.FileExists(empty), "the empty say file is deleted");

		// Over 200 characters: cut.
		string longPath = DIR + "/host_say_long.txt";
		string longText = "";
		for (int i = 0; i < 300; i++)
			longText += "x";
		WriteFile(longPath, longText);
		before = LF_HostSay.GetSentCount();
		string longSent = longText;
		if (!RedProof())
			longSent = LF_HostSay.ProcessFile(longPath);
		ctx.Equal(LF_HostSay.MAX_LENGTH, longSent.Length(), "text over 200 characters is cut to 200");
		ctx.Equal(before + 1, LF_HostSay.GetSentCount(), "the cut text is still one toast");
		ctx.False(FileIO.FileExists(longPath), "the long say file is deleted");

		FileIO.DeleteFile(one);
		FileIO.DeleteFile(empty);
		FileIO.DeleteFile(longPath);
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteFile(string path, string text)
	{
		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
			return;

		if (!text.IsEmpty())
			handle.WriteLine(text);
		handle.Close();
	}
}
