// Fixture for string-splice-selftest.mjs: none of this is flagged.
class LF_SpliceGood
{
	const int MAX_TEXT = 64;

	//! A literal or CONSTANT length is a cap.
	static string Short(string name, string suffix)
	{
		return name.Substring(0, 3) + suffix + name.Substring(0, MAX_TEXT) + name.Substring(0, Math.Min(10, name.Length()));
	}

	//! A tail of a row name is not a splice and not a payload.
	static int RowIndex(string rowName, int prefix)
	{
		string digits = rowName.Substring(prefix, rowName.Length() - prefix);
		return digits.ToInt();
	}

	//! A nested + in another call, a comparison and a condition are not concatenation of the Substring.
	static bool Match(string key, int bar, string language)
	{
		if (key.Substring(0, bar) == language)
			return Foo(bar + 1);
		return Foo(key.Substring(0, bar).Length() + 1);
	}

	//! Marked bounded, with a reason.
	static string Mark(string name)
	{
		// lf-bounded: name is a verb word typed by staff, under 20 chars
		return name.Substring(0, 1) + name.Substring(1, name.Length() - 1);
	}

	//! Words in comments and strings are not code.
	static string Words(string text)
	{
		// json.Substring(0, i) + json.Substring(i, json.Length() - i)
		return "payload.Substring(0, n) + x";
	}

	//! A payload in a log line is only cut in the log.
	static void Log(string json)
	{
		LF_Debug.Info("Good", string.Format("got %1", json));
	}

	//! Counts and ids in Format are fine.
	static string Line(int count, string id)
	{
		return string.Format("%1 of %2", count, id);
	}
}
