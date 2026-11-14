// red-proof: drop `kept.Set(keepKey, keepText);` from LF_DialogueOverrides.Replace and
// reload/dialogue-file fails "a reload reads the new text (expected 'First', got
// '#LF-Dialogue_Clerk_Greet')" and nine more lines (observed 2026-11-14).

//------------------------------------------------------------------------------------------------
//! The dev reload (#970): the gate decision for every cell, the dev caption file read again after
//! it changed (new text, edited text, deleted file, a bad file that keeps what was there), the
//! summary of what changed, the stream section a client applies, and the reloaders found by name.
//! Uses a scratch file under $profile, never the server's own dev file.
// tier: LOGIC
class LF_Test_ContentReload : LF_Test
{
	static const string DIR = "$profile:LifeFramework/test-dev";
	static const string FILE = "$profile:LifeFramework/test-dev/dialogue.json";
	static const string KEY = "#LF-Dialogue_Clerk_Greet";

	override string GetName()
	{
		return "reload/dialogue-file";
	}

	protected static void WriteText(string text)
	{
		FileIO.MakeDirectory(DIR);
		FileHandle handle = FileIO.OpenFile(FILE, FileMode.WRITE);
		if (!handle)
			return;
		handle.WriteLine(text);
		handle.Close();
	}

	override void Run(LF_TestContext ctx)
	{
		// The gate: a dev file is read only where dev tools are open, and only for a permitted requester.
		ctx.False(LF_ContentReload.Permits(false, false, false), "an unpermitted requester is refused");
		ctx.True(LF_ContentReload.Permits(false, false, true), "config reload needs no dev tools");
		ctx.False(LF_ContentReload.Permits(true, false, true), "the dev file is not read on a live server, even for an administrator");
		ctx.True(LF_ContentReload.Permits(true, true, true), "the dev file is read where dev tools are open");
		ctx.False(LF_ContentReload.Permits(true, true, false), "dev tools open do not permit an unpermitted requester");

		// A reloader is found by name; a name nobody reloads is no reloader.
		LF_ContentReloader configReloader = LF_ContentReloader.Find("config");
		LF_ContentReloader dialogueReloader = LF_ContentReloader.Find("dialogue");
		ctx.True(configReloader != null, "config has a reloader");
		ctx.True(dialogueReloader != null, "dialogue has a reloader");
		if (dialogueReloader)
			ctx.True(dialogueReloader.NeedsDevTools(), "the dialogue reloader needs dev tools");
		if (configReloader)
			ctx.False(configReloader.NeedsDevTools(), "the config reloader does not");
		ctx.True(LF_ContentReloader.Find("no_such_thing") == null, "an unknown name has no reloader");

		LF_DialogueOverrides.Reset();
		FileIO.DeleteFile(FILE);
		ctx.EqualStr(KEY, LF_DialogueOverrides.Text(KEY), "with no override the key itself is shown (the widget translates it)");

		// First read: one override.
		WriteText("{\"captions\":{\"" + KEY + "\":\"First\"}}");
		string first = LF_DialogueOverrides.ReloadFromFile(FILE);
		ctx.EqualStr("First", LF_DialogueOverrides.Text(KEY), "a reload reads the new text");
		ctx.True(first.Contains("1 added"), "the summary counts the added key: " + first);

		// The file changes: the next reload shows the new text.
		WriteText("{\"captions\":{\"" + KEY + "\":\"Second\",\"#LF-Dialogue_Clerk_Idle\":\"Next, please.\"}}");
		string second = LF_DialogueOverrides.ReloadFromFile(FILE);
		ctx.EqualStr("Second", LF_DialogueOverrides.Text(KEY), "an edited line shows after a reload");
		ctx.EqualStr("Next, please.", LF_DialogueOverrides.Text("#LF-Dialogue_Clerk_Idle"), "a second line is read");
		ctx.True(second.Contains("1 added") && second.Contains("1 changed"), "the summary counts what changed: " + second);

		// A file that is not JSON leaves what was there; a value that is not text is skipped alone.
		WriteText("{\"captions\": oops");
		string bad = LF_DialogueOverrides.ReloadFromFile(FILE);
		ctx.EqualStr("Second", LF_DialogueOverrides.Text(KEY), "a broken file keeps the overrides as they were");
		ctx.True(bad.Contains("unchanged"), "the summary says it kept them: " + bad);
		WriteText("{\"captions\":{\"" + KEY + "\":\"Third\",\"#LF-Dialogue_Clerk_Idle\":12,\"#LF-Dialogue_Clerk_Board\":{\"a\":1}}}");
		LF_DialogueOverrides.ReloadFromFile(FILE);
		ctx.EqualStr("Third", LF_DialogueOverrides.Text(KEY), "a bad value costs only itself");
		ctx.EqualStr("#LF-Dialogue_Clerk_Idle", LF_DialogueOverrides.Text("#LF-Dialogue_Clerk_Idle"), "a number is not a caption");
		ctx.Equal(1, LF_DialogueOverrides.Count(), "one override is left");

		// The stream: a payload without overrides is what it was; one with them reaches a client; an empty one clears.
		map<string, string> none = new map<string, string>();
		LF_ShopPricesConfig shops = LF_Test_ConfigMergeFixture.MakeCore();
		string plain = LF_ConfigStream.BuildPayload(shops, false);
		ctx.EqualStr(plain, LF_ConfigStream.BuildPayload(shops, false, null, null, null, none), "no overrides: the payload is byte for byte what it was");
		string withCaptions = LF_ConfigStream.BuildPayload(shops, false, null, null, null, LF_DialogueOverrides.GetCaptions());
		ctx.True(withCaptions.Contains(LF_DialogueOverrides.SECTION), "overrides add the devDialogue section");
		LF_DialogueOverrides.Reset();
		ctx.EqualStr(KEY, LF_DialogueOverrides.Text(KEY), "reset drops the overrides");
		LF_DialogueOverrides.ApplyPayload(withCaptions);
		ctx.EqualStr("Third", LF_DialogueOverrides.Text(KEY), "a client applies the stream's text");
		LF_DialogueOverrides.ApplyPayload(plain);
		ctx.EqualStr(KEY, LF_DialogueOverrides.Text(KEY), "a stream with no section clears the overrides");

		// Deleting the file clears them.
		WriteText("{\"captions\":{\"" + KEY + "\":\"Fourth\"}}");
		LF_DialogueOverrides.ReloadFromFile(FILE);
		FileIO.DeleteFile(FILE);
		string gone = LF_DialogueOverrides.ReloadFromFile(FILE);
		ctx.EqualStr(KEY, LF_DialogueOverrides.Text(KEY), "a deleted file clears the overrides");
		ctx.True(gone.Contains("1 removed"), "the summary counts the removed key: " + gone);

		LF_DialogueOverrides.Reset();
	}
}
