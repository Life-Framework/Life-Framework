// red-proof: RedProof() looks the signs up under a prefix that does not exist and skips the style family files, so no town entry, no junction and no `road` style resolve and every check below fails. Observed before the rows existed (#4478): `tools\cli test --no-build --only LF_Test_EveronRoadSigns` failed, 5 town entries and 5 junctions missing.
//! everon/road-signs (#4478, WO-roadsigns card 1 of 2: the rows): under the Everon 1989 header (the pack leg) every
//! live town has a town-entry sign and every main junction has a junction sign, both in the `road` style. Each row
//! names a town in US English capitals-and-lowercase with a compass letter and a distance in miles; the distance is
//! worked out here from the place positions (straight line, 1609.344 m to the mile, one decimal), never typed into
//! the test. Place names keep their own spelling. Placement is the second card.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronRoadSigns : LF_Test
{
	static const float METRES_PER_MILE = 1609.344;
	static const float MILE_TOLERANCE = 0.06;
	static const int JUNCTION_ARMS = 3;

	protected ref array<string> m_aTownNames = {};
	protected ref array<string> m_aTownSlugs = {};
	protected ref array<vector> m_aTownAt = {};
	protected ref array<string> m_aJunctionSlugs = {};
	protected ref array<vector> m_aJunctionAt = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/road-signs";
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTown(string name, string slug, float x, float z)
	{
		m_aTownNames.Insert(name);
		m_aTownSlugs.Insert(slug);
		m_aTownAt.Insert(Vector(x, 0, z));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddJunction(string slug, float x, float z)
	{
		m_aJunctionSlugs.Insert(slug);
		m_aJunctionAt.Insert(Vector(x, 0, z));
	}

	//------------------------------------------------------------------------------------------------
	//! The live towns and the main junctions, at the map's own positions (docs/everon/places dossiers).
	protected void Fill()
	{
		AddTown("Saint-Philippe", "saint-philippe", 4480, 10715);
		AddTown("Meaux", "meaux", 4520.45, 9467.976);
		AddTown("Montignac", "montignac", 4773.455, 7094.566);
		AddTown("Chotain", "chotain", 7086.131, 6012.347);
		AddTown("Levie", "levie", 7464.442, 4738.911);

		AddJunction("stillbarn", 5347.6, 9944.6);
		AddJunction("roadfarm", 5181.9, 7085.3);
		AddJunction("farmcompound", 6661.7, 5880.7);
		AddJunction("sawmill", 3122.1, 5227.5);
		AddJunction("northeast", 5017.4, 10857.4);
	}

	//------------------------------------------------------------------------------------------------
	protected static float Miles(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return Math.Sqrt(dx * dx + dz * dz) / METRES_PER_MILE;
	}

	//------------------------------------------------------------------------------------------------
	//! The eight-point compass letter from `a` to `b` (z is north, x is east).
	protected static string Compass(vector a, vector b)
	{
		float dx = b[0] - a[0];
		float dz = b[2] - a[2];
		float degrees = Math.Atan2(dx, dz) * Math.RAD2DEG;
		if (degrees < 0)
			degrees += 360;

		array<string> points = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
		int index = Math.Round(degrees / 45);
		if (index >= points.Count())
			index = 0;

		return points[index];
	}

	//------------------------------------------------------------------------------------------------
	//! A row's value is "<miles> mi"; returns the miles, or -1 when it is not that shape.
	protected static float ParseMiles(string value)
	{
		int space = value.IndexOf(" ");
		if (space < 1)
			return -1;

		if (value.Substring(space, value.Length() - space) != " mi")
			return -1;

		return value.Substring(0, space).ToFloat();
	}

	//------------------------------------------------------------------------------------------------
	//! US English: every word is a place name, a compass letter, "mi" or a short sign word. No digit-only
	//! check here; the numbers are checked against the positions.
	protected bool WordsAreUs(string text, array<string> allowed)
	{
		string cleaned = text;
		cleaned.Replace("(", " ");
		cleaned.Replace(")", " ");
		array<string> words = {};
		cleaned.Split(" ", words, true);
		foreach (string word : words)
		{
			bool numeric = word.ToFloat() > 0;
			if (numeric)
				continue;

			if (allowed.Find(word) == -1)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSign(LF_TestContext ctx, string where, LF_SignDef sign, vector from, string ownName, array<string> allowed)
	{
		ctx.NotNull(sign, where + ": the sign row exists");
		if (!sign)
			return;

		ctx.EqualStr("road", sign.m_sStyle, where + ": it is the road style");
		ctx.True(!sign.m_sTitle.IsEmpty(), where + ": it has a title");
		ctx.True(WordsAreUs(sign.m_sTitle, allowed), where + ": the title is US English (" + sign.m_sTitle + ")");
		ctx.True(sign.m_aRows && sign.m_aRows.Count() > 0, where + ": it has rows");
		if (!sign.m_aRows)
			return;

		float last = 0;
		foreach (LF_SignRow row : sign.m_aRows)
		{
			ctx.True(WordsAreUs(row.m_sLabel, allowed), where + ": the row is US English (" + row.m_sLabel + ")");
			float miles = ParseMiles(row.m_sValue);
			ctx.True(miles > 0, where + ": the value reads '<miles> mi' (" + row.m_sValue + ")");
			ctx.True(miles >= last, where + ": rows run nearest first");
			last = miles;

			// The row names a live town; the distance and compass letter are the computed ones.
			bool matched;
			for (int i = 0; i < m_aTownNames.Count(); i++)
			{
				string expected = m_aTownNames[i] + " (" + Compass(from, m_aTownAt[i]) + ")";
				if (row.m_sLabel != expected)
					continue;

				matched = true;
				ctx.True(m_aTownNames[i] != ownName, where + ": no row for the place the sign stands in");
				float want = Miles(from, m_aTownAt[i]);
				ctx.True(Math.AbsFloat(miles - want) <= MILE_TOLERANCE, string.Format("%1: %2 is %3 mi away, the row says %4", where, m_aTownNames[i], want, row.m_sValue));
			}

			ctx.True(matched, where + ": the row's town and compass letter match the positions (" + row.m_sLabel + ")");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Fill();

		// The style: its family file, its two layouts, capitals.
		LF_SignStyleConfig styles = LF_SignStyles.Load(LF_SignStyles.CONFIG_PATH);
		ctx.NotNull(styles, "the sign styles load");
		if (!styles)
			return;

		if (!RedProof())
			LF_SignStyles.ApplyFamilyFiles(styles);

		LF_SignStyleDef road = LF_SignStyles.FindStyleIn(styles, "road");
		ctx.NotNull(road, "the road style resolves from LF_SignStyles_Road.conf");
		if (road)
		{
			Resource face = Resource.Load(road.m_sLayout);
			Resource row = Resource.Load(road.m_sRowLayout);
			ctx.True(face && face.IsValid(), "the road face layout resolves");
			ctx.True(row && row.IsValid(), "the road row layout resolves");
			ctx.True(road.m_bUpperTitle, "road titles are capitals");
		}

		array<string> allowed = { "mi", "Junction", "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
		foreach (string name : m_aTownNames)
			allowed.Insert(name);

		string entryPrefix = "road_entry_";
		string junctionPrefix = "road_jct_";
		if (RedProof())
		{
			entryPrefix = "road_none_";
			junctionPrefix = "road_none_";
		}

		// One entry per live town, one row per other town.
		for (int t = 0; t < m_aTownNames.Count(); t++)
		{
			LF_SignDef entry = LF_SignCatalog.Find(entryPrefix + m_aTownSlugs[t]);
			CheckSign(ctx, "entry " + m_aTownNames[t], entry, m_aTownAt[t], m_aTownNames[t], allowed);
			if (entry && entry.m_aRows)
				ctx.Equal(m_aTownNames.Count() - 1, entry.m_aRows.Count(), "entry " + m_aTownNames[t] + ": one row per other town");

			if (entry)
				ctx.EqualStr(m_aTownNames[t], entry.m_sTitle, "entry " + m_aTownNames[t] + ": the title is the town's name");
		}

		// One junction per main crossroads, its nearest towns.
		for (int j = 0; j < m_aJunctionSlugs.Count(); j++)
		{
			LF_SignDef junction = LF_SignCatalog.Find(junctionPrefix + m_aJunctionSlugs[j]);
			CheckSign(ctx, "junction " + m_aJunctionSlugs[j], junction, m_aJunctionAt[j], string.Empty, allowed);
			if (junction && junction.m_aRows)
				ctx.Equal(JUNCTION_ARMS, junction.m_aRows.Count(), "junction " + m_aJunctionSlugs[j] + ": the three nearest towns");
		}

		ctx.True(m_aJunctionSlugs.Count() >= 5, "five or more main junctions are covered");
	}
}
