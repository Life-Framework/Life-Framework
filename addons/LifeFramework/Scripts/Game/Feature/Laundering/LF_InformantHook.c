//------------------------------------------------------------------------------------------------
//! The informant's tip-off (#2921, M7 beat 2 of #1025). A dialogue reply ("Heard where the man is
//! buying?") costs a config fee in cash and puts a private mark on the map of the spot where the
//! illicit buyer stands today; the mark is gone when the rotation moves. Server only.
//!
//! The server re-checks everything the client could lie about, in this order, and every refusal is
//! told in words with nothing taken:
//!  - a dead or downed body, or a rate-limited ask, does nothing;
//!  - police on duty, or an underworld standing below `m_iInformantMinTier`, get "Never heard of him."
//!    (the same words, so the refusal does not say which it was);
//!  - nobody is buying today (no spot placed): refused before any cash moves;
//!  - a player who already holds today's mark is told so and charged nothing;
//!  - too little cash: refused with nothing taken.
//! Only then is the fee taken, and a mark that cannot be put is paid back.
//!
//! The mark is a PLAYER row of LF_MapShares (the map's private-mark path, as LF_JobMapMarks), keyed
//! `informant:<uid>`, that expires when the buyer's period ends (the same unix-time period
//! LF_BuyerRotation cuts), so it never shows a spot the buyer has left. Nothing is saved: a restart
//! clears the rows, and a player may buy the tip again.
//!
//! Dialogue file wiring (the pack's informant NPC): reply action hook "Informant", no param; the same
//! hook as a condition shows the reply only to a player the informant would talk to.
//------------------------------------------------------------------------------------------------
enum LF_EInformantTip
{
	//! The fee was taken and the mark is on the map.
	PAID,
	//! The player already holds today's mark: told so, nothing taken.
	ALREADY,
	//! Police on duty, or standing below the config tier: "Never heard of him."
	NOT_TRUSTED,
	//! Not enough cash: nothing taken.
	NO_CASH,
	//! Nobody is buying today (no spot is placed).
	NO_SPOT,
	//! A dead, downed or rate-limited ask, or a failure; nothing taken.
	REFUSED
}

//------------------------------------------------------------------------------------------------
class LF_InformantTip
{
	static const string FEATURE = "Laundering";
	static const string FACTION = "UNDERWORLD";
	static const string KEY_PREFIX = "informant:";
	static const string REASON = "informant tip";
	static const string TEXT_MARK = "#LF-Informant_Mark";
	static const string KEY_NEVER_HEARD = "#LF-Informant_NeverHeard";
	static const string KEY_NO_CASH = "#LF-Informant_NoCash";
	static const string KEY_TOLD = "#LF-Informant_Told";
	static const string KEY_ALREADY = "#LF-Informant_Already";
	static const string KEY_NO_SPOT = "#LF-Informant_NoSpot";
	//! Indexes into the vanilla placed-marker config (Configs/Map/MapMarkerConfig.conf), as LF_JobMapMarks.
	static const int ICON = 0; // lf-fixed: vanilla config index, not a server tunable
	static const int COLOUR = 1; // lf-fixed: vanilla config index

	//! uid -> the buyer period the player paid for (so a second ask in it is not charged again).
	protected static ref map<string, int> s_mPaidPeriod;

	//------------------------------------------------------------------------------------------------
	static string MarkKey(string uid)
	{
		return KEY_PREFIX + uid;
	}

	//------------------------------------------------------------------------------------------------
	//! The fee in cash; 0 (free) when the config is missing or sets a negative number.
	static int Fee()
	{
		LF_LaunderConfig config = LF_LaunderConfig.Get();
		if (!config)
			return 0;

		return LF_Utils.MaxInt(0, config.m_iInformantFee);
	}

	//------------------------------------------------------------------------------------------------
	//! The lowest underworld tier the informant talks to; 1 when the config is missing.
	static int MinTier()
	{
		LF_LaunderConfig config = LF_LaunderConfig.Get();
		if (!config)
			return 1;

		return config.m_iInformantMinTier;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the informant talks to this character at all: not police on duty, underworld standing at
	//! or above the config tier. The points live on the server, so this is the authoritative answer.
	static bool Trusts(string uid)
	{
		if (uid.IsEmpty())
			return false;

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		if (account && account.GetFaction() == LF_Faction.POLICE && account.IsOnDuty())
			return false;

		return LF_Reputation.Tier(uid, FACTION) >= MinTier();
	}

	//------------------------------------------------------------------------------------------------
	//! World seconds the mark lasts: what is left of the buyer's period at unixNow (at least 1).
	static float SecondsLeft(int unixNow)
	{
		int periodSeconds = LF_Utils.MaxInt(1, LF_LaunderConfig.BuyerPeriodHours() * 3600);
		int left = periodSeconds - unixNow % periodSeconds;
		return LF_Utils.MaxInt(1, left);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one ask for the tip-off. \param[out] key The `#` key to tell the player, "" for none.
	//! \return What happened; PAID is the only outcome that took cash.
	static LF_EInformantTip Ask(IEntity player, string uid, int unixNow, float worldNow, out string key)
	{
		key = string.Empty;
		if (!Replication.IsServer())
			return LF_EInformantTip.REFUSED;

		if (!player || uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "informant tip refused: no player or no account id");
			return LF_EInformantTip.REFUSED;
		}

		if (!LF_Utils.IsAliveState(player) || LF_Downed.IsDowned(player))
		{
			LF_Debug.Warn(FEATURE, string.Format("informant tip refused for %1: dead or downed", uid));
			return LF_EInformantTip.REFUSED;
		}

		if (!Trusts(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("informant tip refused for %1: not trusted (police on duty or standing below tier %2)", uid, MinTier()));
			key = KEY_NEVER_HEARD;
			return LF_EInformantTip.NOT_TRUSTED;
		}

		if (!LF_RateLimit.AllowCharacter(player, LF_ERequestKind.PURCHASE, REASON))
			return LF_EInformantTip.REFUSED;

		int period = LF_BuyerRotation.PeriodOf(unixNow, LF_LaunderConfig.BuyerPeriodHours() * 3600);
		string markKey = MarkKey(uid);
		if (HoldsTipFor(uid, period) && LF_MapShares.Has(markKey))
		{
			LF_Debug.Info(FEATURE, string.Format("informant tip for %1: already paid for period %2", uid, period));
			key = KEY_ALREADY;
			return LF_EInformantTip.ALREADY;
		}

		// The spot is looked up before any cash moves: an informant with nothing to sell takes nothing.
		string spotId = LF_BuyerRotation.OpenSpot(unixNow);
		IEntity spot;
		if (!spotId.IsEmpty())
			spot = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, spotId);

		if (!spot)
		{
			LF_Debug.Info(FEATURE, string.Format("informant tip refused for %1: no buyer spot is open", uid));
			key = KEY_NO_SPOT;
			return LF_EInformantTip.NO_SPOT;
		}

		int fee = Fee();
		if (fee > 0)
		{
			if (LF_MoneyUtils.GetCash(player) < fee)
			{
				LF_Debug.Info(FEATURE, string.Format("informant tip refused for %1: needs %2 cash", uid, fee));
				key = KEY_NO_CASH;
				return LF_EInformantTip.NO_CASH;
			}

			int taken = LF_MoneyUtils.RemoveCash(player, fee, REASON);
			if (taken != fee)
			{
				// A short take (stacks changed under us): give back what came out, nothing is sold.
				if (taken > 0)
					LF_MoneyUtils.AddCash(player, taken, REASON, string.Empty, true);

				LF_Debug.Warn(FEATURE, string.Format("informant tip for %1: took %2 of %3, paid back", uid, taken, fee));
				key = KEY_NO_CASH;
				return LF_EInformantTip.NO_CASH;
			}
		}

		vector at = spot.GetOrigin();
		int x = Math.Round(at[0]);
		int z = Math.Round(at[2]);
		LF_MapShareRow row = LF_MapShareRow.Make(markKey, LF_EMapShareAudience.PLAYER, uid, x, z, ICON, COLOUR, TEXT_MARK, worldNow + SecondsLeft(unixNow));
		if (!LF_MapShares.Put(row))
		{
			// The mark could not be put: the player gets the fee back.
			if (fee > 0)
				LF_MoneyUtils.AddCash(player, fee, REASON, string.Empty, true);

			LF_Debug.Error(FEATURE, string.Format("informant tip for %1: the map refused the mark, fee paid back", uid));
			return LF_EInformantTip.REFUSED;
		}

		PaidTable().Set(uid, period);
		LF_Debug.Info(FEATURE, string.Format("informant tip for %1: spot %2 marked until period %3 ends, fee %4", uid, spotId, period, fee));
		key = KEY_TOLD;
		return LF_EInformantTip.PAID;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the player paid for this period's tip (the mark may have been cleared since).
	static bool HoldsTipFor(string uid, int period)
	{
		if (!s_mPaidPeriod)
			return false;

		int paid;
		if (!s_mPaidPeriod.Find(uid, paid))
			return false;

		return paid == period;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> PaidTable()
	{
		if (!s_mPaidPeriod)
			s_mPaidPeriod = new map<string, int>();

		return s_mPaidPeriod;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPaidPeriod = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The dialogue hook "Informant" (the class name is the registration, see LF_DialogueHook.Find). As a
//! condition the reply shows only to a character the informant would talk to; as an action it runs
//! LF_InformantTip.Ask and tells the player the outcome in words (a toast, and the NPC's answer).
class LF_DialogueHook_Informant : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		return LF_InformantTip.Trusts(LF_Utils.GetPlayerUid(player));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		string key;
		LF_EInformantTip result = LF_InformantTip.Ask(player, LF_Utils.GetPlayerUid(player), System.GetUnixTime(), LF_MapShares.Now(), key);
		if (key.IsEmpty())
			return;

		if (result == LF_EInformantTip.NO_CASH)
		{
			// Only the toast: it names the fee, and the NPC's caption takes no argument.
			LF_Utils.NotifyEntity(player, key, "", 6.0, LF_InformantTip.Fee().ToString());
			return;
		}

		spoken.Insert(key);
		LF_Utils.NotifyEntity(player, key, "", 6.0);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_InformantTip's statics (LF_StaticReset finds it by reflection).
class LF_InformantTip_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_InformantTip";
	}

	override void Apply()
	{
		LF_InformantTip.ResetStatics();
	}
}
