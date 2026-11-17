// red-proof: RED-OK 2026-11-17, `tools/cli test --no-build --red LF_Test_StaffWebhookEscape,...` (hook skips the cut and escape helpers and the callback hold): `the cut stops before the character it would split (expected 1899, got 1905)`, `a request's callback is held while it waits (expected 1, got 0)` failed; restored, green.

// tier: LOGIC
//! moderation/webhook-escape (#2414): the staff webhook body stays valid JSON and valid UTF-8 for
//! admin-sourced text. Control characters other than newline, return and tab are escaped, a line cut
//! at MAX_TEXT never ends inside a multi-byte character, and a request's callback is held until its
//! answer arrives.
class LF_Test_StaffWebhookEscape : LF_Test
{
	override string GetName()
	{
		return "moderation/webhook-escape";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_StaffWebhook.SetCaptureForTest(null);

		// A raw control character (here 0x01 and 0x1f) is a six-character escape, never a raw byte.
		int low = 1;
		int high = 31;
		string text = "bell" + low.AsciiToString() + "end" + high.AsciiToString();
		string escaped = text;
		if (!RedProof())
			escaped = LF_StaffWebhook.EscapeJson(text);
		ctx.EqualStr("bell\\u0001end\\u001f", escaped, "control characters become unicode escapes");
		ctx.EqualStr("a\\nb\\tc\\r", LF_StaffWebhook.EscapeJson("a\nb\tc\r"), "newline, tab and return keep their short escapes");
		ctx.EqualStr("q\\\"b\\\\", LF_StaffWebhook.EscapeJson("q\"b\\"), "quote and backslash are escaped");

		// A cut that would land inside a two-byte character steps back to the character boundary.
		string longText = string.Empty;
		for (int i = 0; i < LF_StaffWebhook.MAX_TEXT - 1; i++)
			longText += "a";
		longText += "é" + "tail";
		string cut = longText;
		if (!RedProof())
			cut = LF_StaffWebhook.CutUtf8(longText, LF_StaffWebhook.MAX_TEXT);
		ctx.Equal(LF_StaffWebhook.MAX_TEXT - 1, cut.Length(), "the cut stops before the character it would split");
		ctx.True(cut.IndexOf("é") < 0, "the split character is dropped whole, not left as half a character");
		ctx.EqualStr("abc", LF_StaffWebhook.CutUtf8("abc", 10), "text under the limit is untouched");
		ctx.EqualStr("ab", LF_StaffWebhook.CutUtf8("abcd", 2), "a plain cut is exact");
		ctx.EqualStr("aé", LF_StaffWebhook.CutUtf8("aé", 3), "a character that fits whole is kept");

		// A callback in flight is held, and let go when its answer arrives.
		int held = LF_StaffWebhook.PendingCount();
		RestCallback callback = new RestCallback();
		if (!RedProof())
			LF_StaffWebhook.HoldForTest(callback);
		ctx.Equal(held + 1, LF_StaffWebhook.PendingCount(), "a request's callback is held while it waits");
		LF_StaffWebhook.Release(callback);
		ctx.Equal(held, LF_StaffWebhook.PendingCount(), "the answer lets it go");
	}
}
