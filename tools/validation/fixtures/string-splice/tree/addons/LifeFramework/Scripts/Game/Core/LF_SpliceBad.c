// Fixture for string-splice-selftest.mjs: every method here is a flagged shape.
class LF_SpliceBad
{
	//! splice: rebuilt with Substring and +.
	static string Stamp(string json, int open)
	{
		string head = json.Substring(0, open + 1) + "\"v\":1,";
		return head + json.Substring(open + 1, json.Length() - open - 1);
	}

	//! splice: += of a Substring tail.
	static string Append(string text, int i, int length)
	{
		string result;
		if (i < length)
			result += text.Substring(i, length - i);
		return result;
	}

	//! tail: the rest of a payload-named string.
	static string Rest(string payload, int at)
	{
		string rest = payload.Substring(at, payload.Length() - at);
		return rest;
	}

	//! format: a payload argument.
	static string Wrap(string json)
	{
		return string.Format("{\"data\":%1}", json);
	}
}
