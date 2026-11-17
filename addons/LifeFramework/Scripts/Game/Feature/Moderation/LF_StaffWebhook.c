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
	static const int MAX_TEXT = 1900; // lf-fixed: a chat webhook refuses more than 2000 characters
	static const int TIMEOUT_SECONDS = 10; // lf-fixed: a protocol bound, not tuning

	//! Test seam: when set, Send appends here instead of posting. Not owned (the test holds it).
	protected static array<string> s_aCapture;

	//! Receives the result of each POST. Stateless, so one instance serves every request.
	protected static ref LF_StaffWebhookResult s_Result;

	//! Requests in flight. The engine does not own a callback, so a local one could be freed before
	//! the answer and success or error would never fire (#2414). A finished request leaves the list.
	protected static ref array<ref RestCallback> s_aPending;
	static const int MAX_PENDING = 32; // lf-fixed: a bound on a list that only a dead network can fill

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
		Hold(callback);
		context.POST(callback, path, BuildPayload(text));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps a request's callback alive until its answer arrives; the oldest goes when the list is full.
	protected static void Hold(notnull RestCallback callback)
	{
		if (!s_aPending)
			s_aPending = new array<ref RestCallback>();

		if (s_aPending.Count() >= MAX_PENDING)
			s_aPending.RemoveOrdered(0);

		s_aPending.Insert(callback);
	}

	//------------------------------------------------------------------------------------------------
	//! Lets go of a callback whose answer arrived.
	static void Release(RestCallback callback)
	{
		if (!s_aPending || !callback)
			return;

		int idx = s_aPending.Find(callback);
		if (idx >= 0)
			s_aPending.Remove(idx);
	}

	//------------------------------------------------------------------------------------------------
	//! How many requests wait for an answer.
	static int PendingCount()
	{
		if (!s_aPending)
			return 0;

		return s_aPending.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: holds a callback as Post does, without a request.
	static void HoldForTest(notnull RestCallback callback)
	{
		Hold(callback);
	}

	//------------------------------------------------------------------------------------------------
	//! The JSON body for one line: {"content":"<text>"} with the text cut to MAX_TEXT bytes and escaped.
	static string BuildPayload(string text)
	{
		return "{\"content\":\"" + EscapeJson(CutUtf8(text, MAX_TEXT)) + "\"}";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the text cut to at most maxBytes bytes, never inside a multi-byte UTF-8 character (a
	//! cut in the middle leaves an invalid sequence the receiver may refuse).
	static string CutUtf8(string text, int maxBytes)
	{
		if (text.Length() <= maxBytes)
			return text;

		int end = maxBytes;
		// A continuation byte is 10xxxxxx: step back to the lead byte of the character the cut would split.
		while (end > 0 && IsContinuationByte(text.ToAscii(end)))
			end--;

		return text.Substring(0, end);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsContinuationByte(int value)
	{
		// ToAscii may give a byte above 127 as a negative number.
		if (value < 0)
			value += 256;

		return value >= 128 && value < 192;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: JSON string escapes. Backslash and quote, the short forms for newline, return and tab,
	//! and a six-character unicode escape for every other control character below 0x20 (a raw one
	//! makes the body invalid JSON).
	static string EscapeJson(string text)
	{
		string hex = "0123456789abcdef";
		string result = string.Empty;
		int length = text.Length();
		for (int i = 0; i < length; i++)
		{
			string character = text.Substring(i, 1);
			int code = text.ToAscii(i);
			if (character == "\\")
				result += "\\\\";
			else if (character == "\"")
				result += "\\\"";
			else if (character == "\n")
				result += "\\n";
			else if (character == "\r")
				result += "\\r";
			else if (character == "\t")
				result += "\\t";
			else if (code >= 0 && code < 32)
				result += "\\u00" + hex.Substring(code / 16, 1) + hex.Substring(code % 16, 1);
			else
				result += character;
		}

		return result;
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
		LF_StaffWebhook.Release(cb);
		LF_Debug.Info(LF_StaffWebhook.FEATURE, "StaffWebhook: posted");
	}

	//------------------------------------------------------------------------------------------------
	void OnError(RestCallback cb)
	{
		LF_StaffWebhook.Release(cb);
		int code = 0;
		if (cb)
			code = cb.GetHttpCode();

		LF_Debug.Error(LF_StaffWebhook.FEATURE, string.Format("StaffWebhook: the POST failed (HTTP %1); the line was not delivered", code));
	}
}
