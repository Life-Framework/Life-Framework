//------------------------------------------------------------------------------------------------
//! Medic rank ladder (#2708). A copy of the LF_PoliceRanks pattern, kept separate on purpose: the
//! police ladder is not generalised (blast radius); a shared base can come later (#652).
//!
//! Each rung sets the medic's duty pay per second and the kit they may draw. A rung's kit is every
//! prefab of its own row and of the rungs below it, so a higher rank draws everything a lower one
//! does. On-duty medic seconds (the duty clock, LF_PoliceRanks.SettleDutyAt) promote one rung when
//! the rung's m_iDutySecondsForNext is reached; the top rung never promotes.
//!
//! The ladder is LF_MedicConfig.m_aRanks; an empty list falls back to the built-in three rungs below.
//! Rank and medic duty seconds live on the character standing (LF_PlayerAccount) and are saved by the
//! "medicranks" save section (LF_MedicRanksSavePayload) under each character's record key. A save without that section loads
//! every medic at rank 0.
//------------------------------------------------------------------------------------------------
class LF_MedicRanks
{
	static const string FEATURE = "MedicRanks";
	static const int TRAINEE = 0;
	static const int PARAMEDIC = 1;
	static const int DOCTOR = 2;
	//! Built-in on-duty seconds per promotion (one and two hours of total medic duty).
	static const int DUTY_SECONDS_PARAMEDIC = 3600;	// lf-fixed: built-in ladder only; LF_Medic.conf m_aRanks sets the real value
	static const int DUTY_SECONDS_DOCTOR = 7200;	// lf-fixed: built-in ladder only; LF_Medic.conf m_aRanks sets the real value
	//! Localization key of the kit refusal; %1 is the title key of the rank the item needs.
	static const string KIT_REFUSED_KEY = "LF-MedicRank_KitRefused";

	static const ResourceName BANDAGE = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName TOURNIQUET = "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et";
	static const ResourceName SALINE = "{00E36F41CA310E2A}Prefabs/Items/Medicine/SalineBag_01/SalineBag_US_01.et";
	static const ResourceName MORPHINE = "{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et";

	protected static int s_iKitRefusals;

	//------------------------------------------------------------------------------------------------
	//! Game-mode start/end reset (LF_StaticReset).
	static void ResetStatics()
	{
		s_iKitRefusals = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The configured ladder, or the built-in three rungs when the config gives none.
	static void Ranks(notnull array<ref LF_MedicRank> outRanks)
	{
		outRanks.Clear();
		LF_MedicConfig config = LF_MedicConfig.Get();
		if (config && config.m_aRanks)
		{
			foreach (LF_MedicRank rank : config.m_aRanks)
			{
				if (!rank || rank.m_sId.IsEmpty())
				{
					LF_Debug.Warn(FEATURE, "ladder row with no id skipped");
					continue;
				}

				outRanks.Insert(rank);
			}

			if (!outRanks.IsEmpty())
				return;
		}

		BuiltIn(outRanks);
	}

	//------------------------------------------------------------------------------------------------
	//! Trainee (bandage, tourniquet; the jobs config rate), paramedic (saline), doctor (morphine).
	static void BuiltIn(notnull array<ref LF_MedicRank> outRanks)
	{
		LF_MedicRank trainee = LF_MedicRank.Create("trainee", "LF-MedicRank_Trainee", -1, DUTY_SECONDS_PARAMEDIC);
		trainee.m_aKit.Insert(BANDAGE);
		trainee.m_aKit.Insert(TOURNIQUET);
		outRanks.Insert(trainee);

		LF_MedicRank paramedic = LF_MedicRank.Create("paramedic", "LF-MedicRank_Paramedic", 0.5, DUTY_SECONDS_DOCTOR);
		paramedic.m_aKit.Insert(SALINE);
		outRanks.Insert(paramedic);

		LF_MedicRank doctor = LF_MedicRank.Create("doctor", "LF-MedicRank_Doctor", 0.6, 0);
		doctor.m_aKit.Insert(MORPHINE);
		outRanks.Insert(doctor);
	}

	//------------------------------------------------------------------------------------------------
	static int RankCount()
	{
		array<ref LF_MedicRank> ranks = {};
		Ranks(ranks);
		return ranks.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int TopRank()
	{
		return RankCount() - 1;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRankUsable(int rank)
	{
		return rank >= 0 && rank < RankCount();
	}

	//------------------------------------------------------------------------------------------------
	//! A saved rank put back on the ladder: below 0 is 0, past the top is the top.
	static int UsableRank(int rank)
	{
		return LF_Utils.ClampInt(rank, 0, LF_Utils.MaxInt(0, TopRank()));
	}

	//------------------------------------------------------------------------------------------------
	static string TitleKey(int rank)
	{
		LF_MedicRank row = Row(rank);
		if (row && !row.m_sTitleKey.IsEmpty())
			return row.m_sTitleKey;

		return "LF-MedicRank_Trainee";
	}

	//------------------------------------------------------------------------------------------------
	//! Cash per on-duty second for the rank; negative means the rank defers to the jobs config medic rate.
	static float PayPerSecond(int rank)
	{
		LF_MedicRank row = Row(rank);
		if (!row)
			return -1;

		return row.m_fPayPerSecond;
	}

	//------------------------------------------------------------------------------------------------
	//! On-duty seconds that promote this rank to the next. 0 for the top rank.
	static int DutySecondsForNext(int rank)
	{
		if (rank >= TopRank())
			return 0;

		LF_MedicRank row = Row(rank);
		if (!row)
			return 0;

		return LF_Utils.MaxInt(row.m_iDutySecondsForNext, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure threshold. requiredSeconds of 0 never promotes (the top rank).
	static bool PromotionDue(int onDutySeconds, int requiredSeconds)
	{
		if (requiredSeconds <= 0)
			return false;

		return onDutySeconds >= requiredSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The lowest rank whose kit (its row or a row below it) holds the prefab. -1 when no rung lists it.
	static int RankNeededFor(ResourceName prefab)
	{
		array<ref LF_MedicRank> ranks = {};
		Ranks(ranks);
		foreach (int i, LF_MedicRank row : ranks)
		{
			if (row && row.m_aKit && row.m_aKit.Contains(prefab))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Everything the rank may draw: its own row's kit and every row below it.
	static void CollectKit(int rank, notnull array<ResourceName> kit)
	{
		kit.Clear();
		array<ref LF_MedicRank> ranks = {};
		Ranks(ranks);
		foreach (int i, LF_MedicRank row : ranks)
		{
			if (i > rank || !row || !row.m_aKit)
				continue;

			foreach (ResourceName prefab : row.m_aKit)
			{
				if (!prefab.IsEmpty() && !kit.Contains(prefab))
					kit.Insert(prefab);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! May a medic of this rank draw the prefab? A refusal names the rank it needs in refusal
	//! (a localization key with that rank's title key as %1) and is logged.
	static bool CanDrawItem(int rank, ResourceName prefab, out string refusal)
	{
		refusal = string.Empty;
		int needed = RankNeededFor(prefab);
		if (needed < 0)
		{
			refusal = KIT_REFUSED_KEY;
			s_iKitRefusals++;
			LF_Debug.Warn(FEATURE, string.Format("kit draw refused: %1 is not on any medic kit", prefab));
			return false;
		}

		if (rank >= needed)
			return true;

		refusal = KIT_REFUSED_KEY;
		s_iKitRefusals++;
		LF_Debug.Info(FEATURE, string.Format("kit draw refused: rank %1 (%2) may not draw %3, it needs rank %4 (%5)", rank, TitleKey(rank), prefab, needed, TitleKey(needed)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! CanDrawItem for an account's active character.
	static bool CanDraw(LF_PlayerAccount account, ResourceName prefab, out string refusal)
	{
		if (!LF_MedicDuty.IsOnDuty(account))
		{
			refusal = KIT_REFUSED_KEY;
			s_iKitRefusals++;
			LF_Debug.Info(FEATURE, "kit draw refused: not an on-duty medic");
			return false;
		}

		return CanDrawItem(account.GetMedicRank(), prefab, refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! Duty pay rate of a medic account: its rank row's rate, or -1 to defer to the jobs config rate.
	static float PayPerSecondFor(LF_PlayerAccount account)
	{
		if (!account || account.GetFaction() != LF_Faction.MEDIC)
			return -1;

		return PayPerSecond(account.GetMedicRank());
	}

	//------------------------------------------------------------------------------------------------
	//! Adds medic on-duty seconds; promotes one rung when the threshold is met. Called by the duty
	//! clock for a medic account. \return true when this call promoted the medic.
	static bool AccrueDuty(LF_PlayerAccount account, int seconds)
	{
		if (seconds <= 0 || !account)
			return false;

		if (account.GetFaction() != LF_Faction.MEDIC)
		{
			LF_Debug.Warn(FEATURE, string.Format("duty accrual refused: %1 is not a medic", account.GetPersistentId()));
			return false;
		}

		account.AddMedicDutySeconds(seconds);
		int rank = account.GetMedicRank();
		if (!PromotionDue(account.GetMedicDutySeconds(), DutySecondsForNext(rank)))
			return false;

		int nextRank = rank + 1;
		if (!IsRankUsable(nextRank))
			return false;

		// The shift so far is paid at the old rank (#2874).
		float oldRate = LF_DutyPay.RateBeforeRankChange(account);
		account.SetMedicRank(nextRank);
		LF_DutyPay.AfterRankChange(account, oldRate);
		LF_Debug.Info(FEATURE, string.Format("promoted %1 to medic rank %2 (%3) by time on duty", account.GetPersistentId(), nextRank, TitleKey(nextRank)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam ("medicranks" save section): one row per character with a medic rank or medic
	//! duty seconds, keyed by the character's record key. Rank 0 with no seconds is not written.
	static array<ref LF_MedicRankRow> ExportAll()
	{
		array<ref LF_MedicRankRow> rows = new array<ref LF_MedicRankRow>();
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return rows;

		foreach (LF_PlayerAccountRecord accountRecord : LF_PlayerAccountManager.ExportAll())
		{
			if (!accountRecord)
				continue;

			LF_PlayerAccount account = accounts.GetFromCache(accountRecord.m_sPersistentId);
			if (!account)
				continue;

			array<string> keys = {};
			if (account.HasCharacters())
			{
				foreach (LF_PlayerCharacter character : account.GetCharacters())
				{
					if (character)
						keys.Insert(character.GetRecordKey());
				}
			}
			else
			{
				keys.Insert(account.GetPersistentId());
			}

			foreach (string key : keys)
			{
				LF_CharacterStanding standing = account.GetStandingFor(key);
				if (!standing || (standing.m_iMedicRank == 0 && standing.m_iMedicDutySeconds == 0))
					continue;

				rows.Insert(LF_MedicRankRow.Create(key, standing.m_iMedicRank, standing.m_iMedicDutySeconds));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts each saved rank and duty total back on the character its key names.
	//! Idempotent; a rank off the ladder clamps onto it; a key with no character is logged and skipped.
	//! \return Rows applied.
	static int ApplyAll(notnull array<ref LF_MedicRankRow> rows)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			LF_Debug.Error(FEATURE, "medic ranks not applied: no account manager");
			return 0;
		}

		int applied = 0;
		foreach (LF_MedicRankRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "skipped a medic rank row with no character key");
				continue;
			}

			LF_CharacterStanding standing = accounts.FindStanding(row.m_sKey);
			if (!standing)
			{
				LF_Debug.Warn(FEATURE, string.Format("medic rank %1 for %2 not applied: no character", row.m_iRank, row.m_sKey));
				continue;
			}

			int usable = UsableRank(row.m_iRank);
			if (usable != row.m_iRank)
				LF_Debug.Warn(FEATURE, string.Format("saved medic rank %1 for %2 is off the ladder, applied as %3", row.m_iRank, row.m_sKey, usable));

			standing.m_iMedicRank = usable;
			standing.m_iMedicDutySeconds = LF_Utils.MaxInt(0, row.m_iDutySeconds);
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MedicRank Row(int rank)
	{
		array<ref LF_MedicRank> ranks = {};
		Ranks(ranks);
		if (ranks.IsEmpty())
			return null;

		return ranks.Get(LF_Utils.ClampInt(rank, 0, ranks.Count() - 1));
	}
}

//------------------------------------------------------------------------------------------------
//! One rung of the medic ladder (LF_MedicConfig.m_aRanks). A saved rank is the rung's index, so a
//! rung leaves only from the end of the ladder. A record of the shared override model (LF_ConfigOverlay),
//! keyed by `m_sId`, so a pack overlay can add, replace, patch or remove a rung (#3047).
[BaseContainerProps()]
class LF_MedicRank : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable name of the rank, for logs and packs")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Localization key of the title, without the leading #")]
	string m_sTitleKey;

	[Attribute("-1", UIWidgets.Auto, "Cash per on-duty second at clock-out. Negative uses the jobs config medic rate", params: "-1 1000")]
	float m_fPayPerSecond;

	[Attribute("0", UIWidgets.Auto, "Total medic on-duty seconds that promote to the next rank. 0 means no time promotion", params: "0 10000000")]
	int m_iDutySecondsForNext;

	[Attribute("", UIWidgets.ResourceNamePicker, "Kit prefabs this rank adds; a rank draws its own and every lower rank's", "et")]
	ref array<ResourceName> m_aKit;

	//------------------------------------------------------------------------------------------------
	//! A rung with an empty kit; the caller inserts its prefabs.
	static LF_MedicRank Create(string id, string titleKey, float payPerSecond, int dutySecondsForNext)
	{
		LF_MedicRank rank = new LF_MedicRank();
		rank.m_sId = id;
		rank.m_sTitleKey = titleKey;
		rank.m_fPayPerSecond = payPerSecond;
		rank.m_iDutySecondsForNext = dutySecondsForNext;
		rank.m_aKit = {};
		return rank;
	}
}

//------------------------------------------------------------------------------------------------
//! One saved medic rank ("medicranks" save section, #2708): the character's record key, the rank index and
//! the medic on-duty seconds. Field order is the binary save order; append only.
class LF_MedicRankRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sKey;
	int m_iRank;
	int m_iDutySeconds;

	//------------------------------------------------------------------------------------------------
	static LF_MedicRankRow Create(string key, int rank, int dutySeconds)
	{
		LF_MedicRankRow row = new LF_MedicRankRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_iRank = rank;
		row.m_iDutySeconds = dutySeconds;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MedicRanks's statics (LF_StaticReset finds it by reflection).
class LF_MedicRanks_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MedicRanks";
	}

	override void Apply()
	{
		LF_MedicRanks.ResetStatics();
	}
}
