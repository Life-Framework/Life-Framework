// LF_StaffWebhook.c - the staff webhook: one call, Send(text) (#1346), and the real POST behind it (#1283).
// One call for the moderation relays (reports, admin actions). Server only: clients never call it.
// The URL comes from the owner's `staff.webhookUrl` (LF_StaffWebhookConfig), which the host writes
// from its own secrets file; it is a secret and is never logged. The POST is fire and forget and
// fail safe: an empty or malformed URL, a client call or a failed request logs one LF_Debug.Error
// and the game carries on. A test installs a capture and reads back the lines Send was given, so
// relays are proven without a network. The payload is generic JSON, {"content": "<text>"}; what
// listens on the other end (Discord, a relay) is the owner's choice.

//------------------------------------------------------------------------------------------------
class LF_StaffWebhook
{
	static const string FEATURE = "Moderation";

	//! Longest line posted; longer text is cut (a chat webhook refuses an oversized message).
	static const int MAX_TEXT = 1900;
	static const int TIMEOUT_SECONDS = 10;

	//! Test seam: when set, Send appends here instead of posting. Not owned (the test holds it).
	protected static array<string> s_aCapture;

	//! Receives the result of each POST. Stateless, so one instance serves every request.
	protected static ref LF_StaffWebhookResult s_Result;

	//------------------------------------------------------------------------------------------------
	//! Sends one line to the staff channel. Server only by contract: relays call it from server code.
	static void Send(string text)
	{
		if (s_aCapture)
		{
			s_aCapture.Insert(text);
			return;
		}

		Post(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Posts one line to the configured URL.
	//! \return True when a request was handed to the engine; false (after one LF_Debug.Error) when
	//! nothing was sent. A delivery failure arrives later and is logged by the result handler.
	static bool Post(string text)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "StaffWebhook.Post called off the server; nothing sent");
			return false;
		}

		LF_StaffWebhookConfig config = LF_StaffWebhookConfig.Get();
		if (!config || !config.IsSet())
		{
			LF_Debug.Error(FEATURE, "StaffWebhook: no webhook URL is set (staff.webhookUrl); nothing sent");
			return false;
		}

		string baseUrl;
		string path;
		if (!SplitUrl(config.m_sWebhookUrl, baseUrl, path))
		{
			// Never the URL itself: it is a secret.
			LF_Debug.Error(FEATURE, "StaffWebhook: the webhook URL is not an http(s) URL; nothing sent");
			return false;
		}

		RestApi api = GetGame().GetRestApi();
		if (!api)
		{
			LF_Debug.Error(FEATURE, "StaffWebhook: the REST API is not available; nothing sent");
			return false;
		}

		RestContext context = api.GetContext(baseUrl);
		if (!context)
		{
			LF_Debug.Error(FEATURE, "StaffWebhook: no REST context for the webhook host; nothing sent");
			return false;
		}

		if (!s_Result)
			s_Result = new LF_StaffWebhookResult();

		context.SetHeaders("Content-Type,application/json");
		context.SetTimeout(TIMEOUT_SECONDS);

		RestCallback callback = new RestCallback();
		callback.SetOnSuccess(s_Result.OnSuccess);
		callback.SetOnError(s_Result.OnError);
		context.POST(callback, path, BuildPayload(text));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The JSON body for one line: {"content":"<text>"} with the text escaped and cut to MAX_TEXT.
	static string BuildPayload(string text)
	{
		string body = text;
		if (body.Length() > MAX_TEXT)
			body = body.Substring(0, MAX_TEXT);

		// Backslash first, so the escapes added below are not escaped again.
		body.Replace("\\", "\\\\");
		body.Replace("\"", "\\\"");
		body.Replace("\n", "\\n");
		body.Replace("\r", "\\r");
		body.Replace("\t", "\\t");
		return "{\"content\":\"" + body + "\"}";
	}

	//------------------------------------------------------------------------------------------------
	//! Splits an http(s) URL into the base (scheme and host) and the path (leading slash kept; "/"
	//! when the URL has none).
	//! \return False when the text is not an http(s) URL with a host.
	static bool SplitUrl(string url, out string baseUrl, out string path)
	{
		baseUrl = string.Empty;
		path = string.Empty;

		int schemeEnd = url.IndexOf("://");
		if (schemeEnd < 0)
			return false;

		string scheme = url.Substring(0, schemeEnd);
		if (scheme != "https" && scheme != "http")
			return false;

		int hostStart = schemeEnd + 3;
		string rest = url.Substring(hostStart, url.Length() - hostStart);
		int slash = rest.IndexOf("/");
		string host = rest;
		path = "/";
		if (slash >= 0)
		{
			host = rest.Substring(0, slash);
			path = rest.Substring(slash, rest.Length() - slash);
		}

		if (host.IsEmpty())
			return false;

		baseUrl = scheme + "://" + host;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: record every Send into sink instead of sending. Pass null to clear.
	static void SetCaptureForTest(array<string> sink)
	{
		s_aCapture = sink;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasCapture()
	{
		return s_aCapture != null;
	}
}

//------------------------------------------------------------------------------------------------
//! Result handler for the staff webhook POST. Logs the outcome without the URL or the response body.
class LF_StaffWebhookResult
{
	//------------------------------------------------------------------------------------------------
	void OnSuccess(RestCallback cb)
	{
		LF_Debug.Info(LF_StaffWebhook.FEATURE, "StaffWebhook: posted");
	}

	//------------------------------------------------------------------------------------------------
	void OnError(RestCallback cb)
	{
		int code = 0;
		if (cb)
			code = cb.GetHttpCode();

		LF_Debug.Error(LF_StaffWebhook.FEATURE, string.Format("StaffWebhook: the POST failed (HTTP %1); the line was not delivered", code));
	}
}
