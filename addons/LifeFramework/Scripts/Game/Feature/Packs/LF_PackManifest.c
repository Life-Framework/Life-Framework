//------------------------------------------------------------------------------------------------
//! LF_PackManifest - what a content pack says about itself (#963): its id and the core versions
//! it was built for. The mission header names one (LF_MissionHeader.m_LF_PackManifest); the pack
//! config source asks it before it hands out any overlay. Server side, decided at boot.
//! The range is [m_sCoreMin, m_sCoreBelow): min inclusive, below exclusive, both "x.y.z". An empty
//! m_sCoreBelow means no upper bound; an empty min means no lower bound. A bad number refuses the
//! pack, because a pack that cannot say which core it needs cannot be trusted with one.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_PackManifest
{
	static const string FEATURE = "Packs";

	//! The running core. `tools\cli release` stamps the .gproj; this stays in step by hand until
	//! it reads from there.
	static const string CORE_VERSION = "0.1.0";

	[Attribute("", desc: "The pack's addon id, for log lines.")]
	string m_sPackId;

	[Attribute("", desc: "Lowest core version the pack supports, x.y.z, inclusive. Empty: no lower bound.")]
	string m_sCoreMin;

	[Attribute("", desc: "First core version the pack does not support, x.y.z, exclusive. Empty: no upper bound.")]
	string m_sCoreBelow;

	//! Owner-config sections this pack adds (#553), namespaced by the pack id; see LF_PackConfigSection.
	[Attribute("", UIWidgets.Object, desc: "Owner-config sections this pack adds, named from the pack namespace (example, exampleChains).")]
	ref array<ref LF_PackConfigSection> m_aConfigSections;

	//------------------------------------------------------------------------------------------------
	//! Pure. Parses "x.y.z" into three ints. \return false for anything else.
	static bool ParseVersion(string text, out int major, out int minor, out int patch)
	{
		major = 0;
		minor = 0;
		patch = 0;
		array<string> parts = {};
		text.Split(".", parts, false);
		if (parts.Count() != 3)
			return false;
		for (int i = 0; i < 3; i++)
		{
			string part = parts[i];
			if (part.IsEmpty())
				return false;
			for (int c = 0; c < part.Length(); c++)
			{
				string ch = part.Substring(c, 1);
				if (ch < "0" || ch > "9")
					return false;
			}
		}
		major = parts[0].ToInt();
		minor = parts[1].ToInt();
		patch = parts[2].ToInt();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. -1, 0 or 1 comparing two valid versions; 0 when either does not parse.
	static int Compare(string a, string b)
	{
		int a1, a2, a3, b1, b2, b3;
		if (!ParseVersion(a, a1, a2, a3) || !ParseVersion(b, b1, b2, b3))
			return 0;
		if (a1 != b1)
		{
			if (a1 < b1)
				return -1;
			return 1;
		}
		if (a2 != b2)
		{
			if (a2 < b2)
				return -1;
			return 1;
		}
		if (a3 != b3)
		{
			if (a3 < b3)
				return -1;
			return 1;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. Whether `core` sits in [min, below). Empty bounds are open; a bound or `core` that does
	//! not parse is out of range.
	static bool InRange(string core, string min, string below)
	{
		int a, b, c;
		if (!ParseVersion(core, a, b, c))
			return false;
		if (!min.IsEmpty())
		{
			if (!ParseVersion(min, a, b, c))
				return false;
			if (Compare(core, min) < 0)
				return false;
		}
		if (!below.IsEmpty())
		{
			if (!ParseVersion(below, a, b, c))
				return false;
			if (Compare(core, below) >= 0)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this pack supports `core`.
	bool Supports(string core)
	{
		return InRange(core, m_sCoreMin, m_sCoreBelow);
	}

	//------------------------------------------------------------------------------------------------
	//! The one line the core logs when it refuses the pack: both versions, and what happens.
	string RefusalLine(string core)
	{
		string wanted = ">= " + m_sCoreMin;
		if (m_sCoreMin.IsEmpty())
			wanted = "any";
		if (!m_sCoreBelow.IsEmpty())
			wanted = wanted + ", < " + m_sCoreBelow;
		return string.Format("pack '%1' refused: it needs core %2, this core is %3; its config overlays are not loaded", m_sPackId, wanted, core);
	}
}
