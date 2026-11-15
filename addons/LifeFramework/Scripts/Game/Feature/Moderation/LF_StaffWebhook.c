// LF_StaffWebhook.c - the staff webhook's interface (#1346).
// One call, Send(text), for the moderation relays (reports, admin actions). Server only: clients
// never call it. This slice is the contract; the real HTTP POST (S4) goes behind the same call.
// Until then Send logs one line and sends nothing. A test installs a capture and reads back the
// lines Send was given, so relays are proven without a network.

//------------------------------------------------------------------------------------------------
class LF_StaffWebhook
{
	static const string FEATURE = "Moderation";

	//! Test seam: when set, Send appends here instead of logging. Not owned (the test holds it).
	protected static array<string> s_aCapture;

	//------------------------------------------------------------------------------------------------
	//! Sends one line to the staff channel. Server only by contract: relays call it from server code.
	static void Send(string text)
	{
		if (s_aCapture)
		{
			s_aCapture.Insert(text);
			return;
		}

		LF_Debug.Info(FEATURE, "StaffWebhook.Send (no transport yet, nothing sent): " + text);
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
