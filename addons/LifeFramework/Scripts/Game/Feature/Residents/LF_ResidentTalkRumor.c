// LF_ResidentTalkRumor.c - "Heard anything?" in resident talk (#4461, slice of #4170, plan W5.2).
// A player asks a giver (a shopkeeper, the barkeeper, the informant) and hears one rumor a day from
// that giver; the rumor's place goes on their own map and the clip plays (LF_RumorVoice.Deliver).
// The giver kind comes from the dialogue reply's param ("shop", "bar", "informant"), and the rumor
// pool decides what the giver may tell: a crime row (N32) goes only from bar staff and the informant,
// and only to a player the giver counts as friendly or better (LF_RumorPool.Eligible). A second ask
// the same game day, or a giver with nothing new, gets the "nothing new" line. The day each giver last
// told a character is not saved: the heard-of place is the persisted fact (LF_MapKnown), so a restart
// only lets the player ask once more, and a place they know is never offered again.

//------------------------------------------------------------------------------------------------
enum LF_ERumorTalk
{
	//! A rumor was told and its place is now heard of.
	TOLD,
	//! This giver already told this character a rumor today.
	ASKED_TODAY,
	//! The giver has no rumor this character does not already know (or may not be told).
	NOTHING_NEW
}

//------------------------------------------------------------------------------------------------
class LF_ResidentTalkRumor
{
	static const string FEATURE = "Residents";
	static const string KEY_TITLE = "#LF-RumorTalk_Title";
	static const string KEY_TOLD = "#LF-RumorTalk_Told";
	static const string KEY_NOTHING_NEW = "#LF-RumorTalk_NothingNew";
	static const string KEY_ASKED_TODAY = "#LF-RumorTalk_AskedToday";

	//! "<characterId>|<giver key>" -> the game day that giver last told that character a rumor.
	protected static ref map<string, int> s_mToldDay = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	protected static string Key(string characterId, string giverKey)
	{
		return characterId + "|" + giverKey;
	}

	//------------------------------------------------------------------------------------------------
	//! The game day the world clock is on (a running count, so two days never share a number).
	static int Today()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return 0;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		if (!clock)
			return 0;

		return LF_IslandCalendarRules.DayNumber(clock.GetYear(), clock.GetMonth(), clock.GetDay());
	}

	//------------------------------------------------------------------------------------------------
	//! True when `giverKey` already told `characterId` a rumor on `day`.
	static bool AskedToday(string characterId, string giverKey, int day)
	{
		int told;
		return s_mToldDay.Find(Key(characterId, giverKey), told) && told == day;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, the rule: `characterId` asks the giver `giverKey` (the resident id, or the giver kind for
	//! one with no resident row) of kind `giver`, who counts them as `tier`, on game day `day`. One
	//! rumor a day: a second ask the same day is refused with nothing told. Else `LF_RumorPool.Offers`
	//! narrows to what this giver may tell this character, `Pick` chooses by the day and a count, and
	//! `Tell` puts the place on their own map.
	//! \return The rumor told (null when `outcome` is not TOLD).
	static LF_RumorDef Ask(string characterId, string giverKey, string giver, string tier, int day, notnull LF_RumorState state, out LF_ERumorTalk outcome)
	{
		if (characterId.IsEmpty() || giverKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "heard anything: no character or no giver");
			outcome = LF_ERumorTalk.NOTHING_NEW;
			return null;
		}

		if (AskedToday(characterId, giverKey, day))
		{
			LF_Debug.Info(FEATURE, string.Format("heard anything: %1 asked '%2' again on day %3: nothing new", characterId, giverKey, day));
			outcome = LF_ERumorTalk.ASKED_TODAY;
			return null;
		}

		array<ref LF_RumorDef> offers = LF_RumorPool.Offers(characterId, giver, tier, state);
		LF_RumorDef def = LF_RumorPool.Pick(offers, day + s_mToldDay.Count());
		if (!def || !LF_RumorPool.Tell(characterId, def))
		{
			LF_Debug.Info(FEATURE, string.Format("heard anything: '%1' (%2, %3) has nothing new for %4", giverKey, giver, tier, characterId));
			outcome = LF_ERumorTalk.NOTHING_NEW;
			return null;
		}

		s_mToldDay.Set(Key(characterId, giverKey), day);
		LF_Debug.Info(FEATURE, string.Format("heard anything: '%1' told %2 rumor '%3' about %4", giverKey, characterId, def.m_sId, def.m_sPlaceId));
		outcome = LF_ERumorTalk.TOLD;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! The live conditions the world holds now. The core knows none beyond what the clock says, so a
	//! row's `night` or `shop_open` condition needs the caller to fill it; the talk offers day rows.
	static LF_RumorState WorldState()
	{
		return new LF_RumorState();
	}

	//------------------------------------------------------------------------------------------------
	//! The caption key a non-TOLD outcome says.
	static string NothingKey(LF_ERumorTalk outcome)
	{
		if (outcome == LF_ERumorTalk.ASKED_TODAY)
			return KEY_ASKED_TODAY;

		return KEY_NOTHING_NEW;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mToldDay = new map<string, int>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ResidentTalkRumor's statics (LF_StaticReset finds it by reflection).
class LF_ResidentTalkRumor_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentTalkRumor";
	}

	override void Apply()
	{
		LF_ResidentTalkRumor.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The reply "Heard anything?". The dialogue file names the hook "HeardAnything" with the giver kind
//! as the param ("shop", "bar", "informant"). As an action it asks the rule above for the speaking
//! character and, when a rumor is told, plays it through LF_RumorVoice.Deliver (map, clip, caption);
//! a refusal is a spoken "nothing new" caption. Server only, no state of its own.
// lf-spoken: #LF-RumorTalk_NothingNew #LF-RumorTalk_AskedToday
class LF_DialogueHook_HeardAnything : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	//! The key this giver is counted under: its resident id, else its kind.
	protected string GiverKey(IEntity npc, string giver)
	{
		string residentId = LF_ResidentTalk.MemoryIdOf(npc);
		if (!residentId.IsEmpty())
			return residentId;

		return giver;
	}

	//------------------------------------------------------------------------------------------------
	//! How the giver feels about the player; a giver with no resident row and no standing id (#4595)
	//! counts as neutral, so it tells no crime rumor.
	protected string TierFor(IEntity npc, string uid)
	{
		string residentId = LF_ResidentTalk.MemoryIdOf(npc);
		if (residentId.IsEmpty())
			return LF_ResidentMemory.TIER_NEUTRAL;

		return LF_ResidentMemory.Tier(residentId, uid);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		string uid = LF_Utils.GetPlayerUid(player);
		if (uid.IsEmpty() || param.IsEmpty())
		{
			LF_Debug.Warn(LF_ResidentTalkRumor.FEATURE, string.Format("heard anything refused: no character (uid '%1') or no giver kind (param '%2')", uid, param));
			return;
		}

		LF_ERumorTalk outcome;
		LF_RumorDef def = LF_ResidentTalkRumor.Ask(uid, GiverKey(npc, param), param, TierFor(npc, uid), LF_ResidentTalkRumor.Today(), LF_ResidentTalkRumor.WorldState(), outcome);
		if (!def)
		{
			spoken.Insert(LF_ResidentTalkRumor.NothingKey(outcome));
			return;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
			return;

		LF_RumorVoice.Deliver(playerId, npc, def, LF_ResidentTalkRumor.KEY_TITLE, LF_ResidentTalkRumor.KEY_TOLD);
	}
}
