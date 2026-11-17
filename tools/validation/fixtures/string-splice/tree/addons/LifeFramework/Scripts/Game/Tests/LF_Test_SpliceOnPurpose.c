// Fixture: a test builds long strings with Substring on purpose; the scan skips Tests/.
class LF_Test_SpliceOnPurpose
{
	static string Cut(string json, int i)
	{
		return json.Substring(0, i) + json.Substring(i, json.Length() - i);
	}
}
