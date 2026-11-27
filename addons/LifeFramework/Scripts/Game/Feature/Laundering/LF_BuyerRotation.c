//! Where the illicit buyer stands (#2856, M7 beat 2 of #1025). The buyer has several spots, each an
//! LF_BuyerSpotComponent registered in LF_Places under kind `chain_buyer`; exactly one is open at a
//! time and the open one changes every `m_iBuyerPeriodHours` of real time.
//!
//! The open spot is worked out, never saved: a pure function of unix time and the server's rotation
//! seed, so a restart lands on the same spot. Time is cut into periods (unixNow / period seconds);
//! `count` periods make a cycle, and each cycle visits every spot once in an order the seed shuffles
//! (a hashed shuffle: unguessable only because the seed is secret, not a cryptographic generator).
//! Two periods in a row inside a cycle never repeat a spot.
//!
//! The seed is a server secret, not a config value: a number in a repo file or in this script is
//! public, and anyone who saw the buyer once could then name every later spot (the same finding as
//! #2881). It is made at random on first use and kept in `$profile:LifeFramework/`, which no player or
//! repo can read. Server only; a client never knows a spot, so the component registers on the server.
class LF_BuyerRotation
{
	static const string FEATURE = "Laundering";
	static const string PLACE_KIND = "chain_buyer";
	static const string SEED_PATH = "$profile:LifeFramework/buyer-rotation-seed.txt";
	//! A seed shorter than this is treated as a damaged file and replaced.
	static const int MIN_SEED_LENGTH = 32;

	protected static string s_sSeed;
	protected static string s_sSeedForTests;
	//! The spot handed out for one period, so a spot added mid-period joins at the next one, not now.
	protected static int s_iCachePeriod = -1;
	protected static string s_sCacheSpot;
	protected static bool s_bWarnedNoSpot;

	//------------------------------------------------------------------------------------------------
	//! Pure. Which of count spots (sorted by id) is open in this period: the period's cycle shuffles the
	//! spots by a seeded hash, and the period's place in the cycle picks one. -1 when count is below 1.
	static int PickIndex(string seed, int period, int count)
	{
		if (count < 1)
			return -1;

		int cycle = period / count;
		int pos = period % count;

		// Seeded Fisher-Yates over the spot indexes. The step number leads the hashed text: a string hash
		// is linear in its last characters, so a trailing counter would only count up (the first draft
		// opened the spots in id order every cycle).
		array<int> order = {};
		for (int i = 0; i < count; i++)
		{
			order.Insert(i);
		}

		for (int k = 0; k < count - 1; k++)
		{
			int draw = string.Format("%1:%2:%3", k, seed, cycle).Hash() & 0x7FFFFFFF;
			int j = k + draw % (count - k);
			int held = order[k];
			order[k] = order[j];
			order[j] = held;
		}

		return order[pos];
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. The period a unix time falls in.
	static int PeriodOf(int unixNow, int periodSeconds)
	{
		return unixNow / LF_Utils.MaxInt(1, periodSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! The id of the spot open at unixNow, or "" when no spot is placed (logged once) or this is a client.
	static string OpenSpot(int unixNow)
	{
		if (!Replication.IsServer())
			return "";

		int period = PeriodOf(unixNow, LF_LaunderConfig.BuyerPeriodHours() * 3600);
		if (period == s_iCachePeriod && !s_sCacheSpot.IsEmpty() && LF_Places.Find(PLACE_KIND, s_sCacheSpot))
			return s_sCacheSpot;

		array<string> ids = {};
		LF_Places.FindIds(PLACE_KIND, ids);
		if (ids.IsEmpty())
		{
			if (!s_bWarnedNoSpot)
				LF_Debug.Warn(FEATURE, "no buyer spot is placed (component kind chain_buyer): the illicit buyer has nowhere to stand");

			s_bWarnedNoSpot = true;
			s_iCachePeriod = -1;
			s_sCacheSpot = "";
			return "";
		}

		s_bWarnedNoSpot = false;
		ids.Sort();
		int index = PickIndex(Seed(), period, ids.Count());
		if (index < 0)
			return "";

		string spot = ids[index];
		if (period != s_iCachePeriod || spot != s_sCacheSpot)
			LF_Debug.Info(FEATURE, string.Format("buyer spot for period %1: %2 (of %3)", period, spot, ids.Count()));

		s_iCachePeriod = period;
		s_sCacheSpot = spot;
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	//! The rotation seed: the test override, else the server's secret from the profile, made at random
	//! and written the first time. A profile that cannot be written keeps a seed for this run only
	//! (logged): the buyer then moves differently after a restart, but never by a guessable number.
	static string Seed()
	{
		if (!s_sSeedForTests.IsEmpty())
			return s_sSeedForTests;

		if (!s_sSeed.IsEmpty())
			return s_sSeed;

		s_sSeed = ReadSeed();
		if (s_sSeed.Length() >= MIN_SEED_LENGTH)
			return s_sSeed;

		s_sSeed = MakeSeed();
		WriteSeed(s_sSeed);
		return s_sSeed;
	}

	//------------------------------------------------------------------------------------------------
	protected static string MakeSeed()
	{
		string seed = "";
		for (int i = 0; i < 4; i++)
		{
			seed = seed + string.Format("%1-", Math.RandomInt(1, 2147483647));
		}

		return seed + System.GetTickCount().ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadSeed()
	{
		if (!FileIO.FileExists(SEED_PATH))
			return "";

		FileHandle handle = FileIO.OpenFile(SEED_PATH, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, "cannot open the buyer rotation seed: " + SEED_PATH);
			return "";
		}

		string line;
		handle.ReadLine(line);
		handle.Close();
		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteSeed(string seed)
	{
		FileIO.MakeDirectory("$profile:LifeFramework");
		FileHandle handle = FileIO.OpenFile(SEED_PATH, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, "cannot write the buyer rotation seed to " + SEED_PATH + ": the buyer will move differently after a restart");
			return;
		}

		handle.WriteLine(seed);
		handle.Close();
		LF_Debug.Info(FEATURE, "made a new buyer rotation seed in the profile");
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: a fixed seed so a run repeats; "" goes back to the profile's secret.
	static void SetSeedForTests(string seed)
	{
		s_sSeedForTests = seed;
		ForgetPeriod();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: drop the handed-out spot, as a new period would.
	static void ForgetPeriod()
	{
		s_iCachePeriod = -1;
		s_sCacheSpot = "";
		s_bWarnedNoSpot = false;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sSeed = "";
		s_sSeedForTests = "";
		ForgetPeriod();
	}
}

//------------------------------------------------------------------------------------------------
//! The ids behind a place kind, for a system that needs a stable order (LF_Places holds the table).
modded class LF_Places
{
	//------------------------------------------------------------------------------------------------
	//! The ids of every live place of a kind, appended to into. \return how many were added.
	static int FindIds(string kind, notnull array<string> into)
	{
		map<string, EntityID> ids = LF_Places.Kind(kind, false);
		if (!ids)
			return 0;

		int added = 0;
		foreach (string id, EntityID held : ids)
		{
			if (!LF_Places.Resolve(held))
				continue;

			into.Insert(id);
			added++;
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BuyerRotation's statics (LF_StaticReset finds it by reflection).
class LF_BuyerRotation_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuyerRotation";
	}

	override void Apply()
	{
		LF_BuyerRotation.ResetStatics();
	}
}
