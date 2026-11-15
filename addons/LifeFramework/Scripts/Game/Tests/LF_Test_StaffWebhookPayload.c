// red-proof: RED-OK moderation/webhook-payload (fails with the hook on, 16 assertions): payload and no-URL post skipped
// tier: LOGIC
//! Staff webhook payload and fail-safe (#1283): hostile text becomes valid escaped JSON, an
//! over-long line is cut, a URL splits into host and path, and with no usable URL a send is a
//! logged no-op that reports false and never throws. No network is touched.
class LF_Test_StaffWebhookPayload : LF_Test
{
	override string GetName()
	{
		return "moderation/webhook-payload";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_StaffWebhook.SetCaptureForTest(null);

		// Plain text: one JSON object with the line in "content".
		ctx.EqualStr("{\"content\":\"hello\"}", LF_StaffWebhook.BuildPayload("hello"), "plain text becomes a content object");

		// Hostile text: quote, backslash, newline, carriage return, tab.
		string hostile = "say \"hi\"\\ now\nline2\r\n\ttab";
		string payload = string.Empty;
		if (!RedProof())
			payload = LF_StaffWebhook.BuildPayload(hostile);

		ctx.EqualStr("{\"content\":\"say \\\"hi\\\"\\\\ now\\nline2\\r\\n\\ttab\"}", payload, "quotes, backslashes and control characters are escaped");
		ctx.False(payload.Contains("\n"), "the payload holds no raw newline");

		// A long line is cut so the post is not refused for size.
		string longText = string.Empty;
		for (int i = 0; i < LF_StaffWebhook.MAX_TEXT + 50; i++)
			longText += "a";
		string longPayload = LF_StaffWebhook.BuildPayload(longText);
		ctx.Equal(LF_StaffWebhook.MAX_TEXT + 14, longPayload.Length(), "a long line is cut to MAX_TEXT characters inside the wrapper");

		// URL split: base is scheme and host, path starts with a slash.
		string baseUrl;
		string path;
		ctx.True(LF_StaffWebhook.SplitUrl("https://example.test/api/hooks/1/abc", baseUrl, path), "an https URL splits");
		ctx.EqualStr("https://example.test", baseUrl, "the base is scheme and host");
		ctx.EqualStr("/api/hooks/1/abc", path, "the path keeps its leading slash");
		ctx.True(LF_StaffWebhook.SplitUrl("http://example.test", baseUrl, path), "a URL with no path splits");
		ctx.EqualStr("/", path, "a URL with no path posts to the root");
		ctx.False(LF_StaffWebhook.SplitUrl("not a url", baseUrl, path), "text with no scheme is refused");
		ctx.False(LF_StaffWebhook.SplitUrl("ftp://example.test/x", baseUrl, path), "a scheme other than http(s) is refused");
		ctx.False(LF_StaffWebhook.SplitUrl("https:///path", baseUrl, path), "a URL with no host is refused");

		// No URL: a logged no-op that reports false.
		LF_StaffWebhookConfig.Use(LF_StaffWebhookConfig.CreateDefault());
		bool posted = true;
		if (!RedProof())
			posted = LF_StaffWebhook.Post("no url set");
		ctx.False(posted, "with no URL nothing is posted");

		// A bad URL: same, and it never throws.
		LF_StaffWebhookConfig bad = LF_StaffWebhookConfig.CreateDefault();
		bad.m_sWebhookUrl = "garbage-not-a-url";
		LF_StaffWebhookConfig.Use(bad);
		ctx.False(LF_StaffWebhook.Post("bad url"), "a malformed URL posts nothing");

		LF_StaffWebhookConfig.Reload();
	}
}
