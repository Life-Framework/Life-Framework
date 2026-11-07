class LF_FixtureCoalesce
{
	string Name(string name)
	{
		// name ?? "none" in a comment is fine, and so is "??" in a string.
		if (!name)
			return "??";
		return name;
	}
}
