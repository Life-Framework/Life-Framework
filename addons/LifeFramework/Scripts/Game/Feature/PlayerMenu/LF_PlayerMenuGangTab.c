// LF_PlayerMenuGangTab.c: the personal file's fourth tab, "Gang" (LF_PlayerMenuSheet): your crew at a
// glance, read only. The gang's name and flag, your rank, who of the gang is online, whether a radio
// you carry sits on the gang channel and the sites the gang holds. The tab shows only for a player in a
// gang (the replicated LF_OrgView on their own body, so the key is right before any answer arrives).
// The facts come from the server in an answer of their own (LF_PlayerMenuGangTabWire): it reads the
// asking player's own organisation and nobody else's, so another gang's flag, members or sites are
// never on this wire. Standing is never printed: no number, no word.
//
// Wire (version 1): records split by ';', fields by '|', names cleaned like LF_PlayerMenuData.
//   V|1
//   G|gangName|flag|rankName|radio           radio: 0 none carried, 1 carried but off, 2 on the gang channel
//   M|name|rankName                          one online member, you included
//   S|hideoutId|captured                     one held site, 0 base, 1 captured

//------------------------------------------------------------------------------------------------
enum LF_EPlayerMenuGangRadio
{
	//! No radio in the inventory.
	NONE,
	//! A radio is carried, not on the gang channel.
	OFF,
	//! A carried radio is on the gang channel.
	ON
}

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuGangData
{
	static const int VERSION = 1;
	static const string RECORD = ";";
	static const string FIELD = "|";

	string m_sName;
	//! 1 to LF_OrgFlags.COUNT, or LF_OrgFlags.NONE.
	int m_iFlag;
	//! The asking member's rank: a `#` key for a shipped rank, plain text for a custom one.
	string m_sRank;
	LF_EPlayerMenuGangRadio m_eRadio;
	ref array<string> m_aOnlineNames = {};
	ref array<string> m_aOnlineRanks = {};
	ref array<string> m_aSiteIds = {};
	ref array<bool> m_aSiteCaptured = {};

	//------------------------------------------------------------------------------------------------
	string Encode()
	{
		string wire = "V" + FIELD + VERSION.ToString();
		wire += RECORD + "G" + FIELD + LF_PlayerMenuData.Clean(m_sName) + FIELD + m_iFlag.ToString();
		wire += FIELD + LF_PlayerMenuData.Clean(m_sRank) + FIELD + m_eRadio.ToString();
		foreach (int i, string name : m_aOnlineNames)
		{
			wire += RECORD + "M" + FIELD + LF_PlayerMenuData.Clean(name) + FIELD + LF_PlayerMenuData.Clean(m_aOnlineRanks[i]);
		}

		foreach (int s, string site : m_aSiteIds)
		{
			string captured = "0";
			if (m_aSiteCaptured[s])
				captured = "1";

			wire += RECORD + "S" + FIELD + LF_PlayerMenuData.Clean(site) + FIELD + captured;
		}

		return wire;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The data, or null for a wire of another version or with no gang record (a player in no gang).
	static LF_PlayerMenuGangData Decode(string wire)
	{
		array<string> records = {};
		wire.Split(RECORD, records, false);
		if (records.Count() < 2 || records[0] != "V" + FIELD + VERSION.ToString())
			return null;

		LF_PlayerMenuGangData data = new LF_PlayerMenuGangData();
		bool gang = false;
		foreach (string record : records)
		{
			array<string> f = {};
			record.Split(FIELD, f, false);
			if (f.IsEmpty())
				continue;

			if (f[0] == "G" && f.Count() >= 5 && !f[1].IsEmpty())
			{
				data.m_sName = f[1];
				data.m_iFlag = f[2].ToInt();
				data.m_sRank = f[3];
				data.m_eRadio = f[4].ToInt();
				gang = true;
			}
			else if (f[0] == "M" && f.Count() >= 3)
			{
				data.m_aOnlineNames.Insert(f[1]);
				data.m_aOnlineRanks.Insert(f[2]);
			}
			else if (f[0] == "S" && f.Count() >= 3 && !f[1].IsEmpty())
			{
				data.m_aSiteIds.Insert(f[1]);
				data.m_aSiteCaptured.Insert(f[2] == "1");
			}
		}

		if (!gang)
			return null;

		return data;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlayerMenuGangTab
{
	static const string FEATURE = "PlayerMenu";
	//! The most online members the card lists; the rest are counted in the heading.
	static const int ONLINE_SHOWN = 10;
	static const string PART_COLUMNS = "PlayerMenuGangColumns";
	static const string CARD_CREW = "PlayerMenuGangCrew";
	static const string CARD_ONLINE = "PlayerMenuGangOnline";
	static const string CARD_SITES = "PlayerMenuGangSites";
	static const string PART_WAITING = "PlayerMenuGangWaiting";
	static const string PART_NAME = "PlayerMenuGangName";
	static const string PART_FLAG = "PlayerMenuGangFlag";
	static const string PART_RANK = "PlayerMenuGangRank";
	static const string PART_RADIO = "PlayerMenuGangRadio";
	static const string ROW_ONLINE = "PlayerMenuGangOnline_";
	static const string ROW_SITE = "PlayerMenuGangSite_";

	//------------------------------------------------------------------------------------------------
	//! \return True when the tab shows: the player's own view says they are in a gang. No gang, no tab.
	static bool HasTab(LF_OrgView view)
	{
		return view && !view.m_sOrgId.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: does the local player's body say they are in a gang?
	static bool HasTabLocal()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return false;

		return HasTab(LF_OrgReplication.ViewOf(controller.GetControlledEntity()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the crew facts of one member. \param characterId the asking player's active character,
	//! \param online the character ids online now, \param names display names by character id,
	//! \param radio what the member's radios say. \return Null when the character is in no gang.
	static LF_PlayerMenuGangData Gather(string characterId, notnull array<string> online, notnull map<string, string> names, LF_EPlayerMenuGangRadio radio)
	{
		if (characterId.IsEmpty())
			return null;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
			return null;

		LF_PlayerMenuGangData data = new LF_PlayerMenuGangData();
		data.m_sName = org.GetName();
		data.m_iFlag = LF_OrgFlags.FlagOf(org.GetId());
		data.m_sRank = org.GetRankName(characterId);
		data.m_eRadio = radio;

		array<string> ids = {};
		org.GetMemberIds(ids);
		foreach (string memberId : ids)
		{
			if (online.Find(memberId) < 0)
				continue;

			string name = "?";
			if (names.Contains(memberId))
				name = names.Get(memberId);

			data.m_aOnlineNames.Insert(name);
			data.m_aOnlineRanks.Insert(org.GetRankName(memberId));
		}

		LF_OrgHeldSites.Split(LF_OrgHeldSites.RowsFor(org.GetId()), data.m_aSiteIds, data.m_aSiteCaptured);
		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the facts for the player at playerId, read from the server's own state.
	static LF_PlayerMenuGangData ServerBuild(int playerId)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return null;

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		string characterId = LF_OrganizationPlayer.CharacterId(body);
		if (characterId.IsEmpty())
			return null;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (!org)
			return null;

		array<string> online = {};
		LF_OrgFlagRules.OnlineCharacterIds(online);
		map<string, string> names = new map<string, string>();
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		array<string> ids = {};
		org.GetMemberIds(ids);
		foreach (string memberId : ids)
		{
			if (!accounts || online.Find(memberId) < 0)
				continue;

			LF_PlayerCharacter character = accounts.FindCharacter(memberId);
			if (character)
				names.Set(memberId, (character.GetFirstName() + " " + character.GetLastName()).Trim());
		}

		return Gather(characterId, online, names, RadioOf(body, org.GetId()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the member's radios against the gang's band (never assigns a band).
	static LF_EPlayerMenuGangRadio RadioOf(IEntity body, string orgId)
	{
		array<BaseRadioComponent> radios = {};
		LF_ServiceRadio.FindCarried(body, radios);
		if (radios.IsEmpty())
			return LF_EPlayerMenuGangRadio.NONE;

		LF_GangRadioBand band = LF_GangRadio.Find(orgId);
		foreach (BaseRadioComponent radio : radios)
		{
			if (LF_GangRadioTune.IsOnBand(radio, band))
				return LF_EPlayerMenuGangRadio.ON;
		}

		return LF_EPlayerMenuGangRadio.OFF;
	}

	//------------------------------------------------------------------------------------------------
	//! The `#` key of the radio line's value.
	static string RadioKey(LF_EPlayerMenuGangRadio radio)
	{
		if (radio == LF_EPlayerMenuGangRadio.ON)
			return "#LF-PlayerMenuGang_RadioOn";

		if (radio == LF_EPlayerMenuGangRadio.OFF)
			return "#LF-PlayerMenuGang_RadioOff";

		return "#LF-PlayerMenuGang_RadioNone";
	}

	//------------------------------------------------------------------------------------------------
	//! The `#` key of the flag's name, or the "no flag" line.
	static string FlagKey(int flag)
	{
		if (!LF_OrgFlags.IsValid(flag))
			return "#LF-PlayerMenuGang_NoFlag";

		return LF_OrgFlagWall.NameKey(flag);
	}

	//------------------------------------------------------------------------------------------------
	//! Column shares; a zero after the first is left out.
	protected static array<float> Weights(float a, float b, float c)
	{
		array<float> weights = {a};
		if (b > 0)
			weights.Insert(b);

		if (c > 0)
			weights.Insert(c);

		return weights;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart CardIn(int index, string name)
	{
		return LF_UIPart.CreateColumns(name, Weights(1, 0, 0)).Cards().InColumn(PART_COLUMNS, index);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Card(string card, LF_UIPart part)
	{
		return part.InColumn(card, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the tab's parts between the file's header and footer. Null data prints the waiting line.
	static void Build(notnull array<ref LF_UIPart> parts, LF_PlayerMenuGangData data)
	{
		if (!data)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_WAITING, "#LF-PlayerMenuGang_Waiting"));
			return;
		}

		parts.Insert(LF_UIPart.CreateColumns(PART_COLUMNS, Weights(1, 1, 1)));

		parts.Insert(CardIn(0, CARD_CREW));
		parts.Insert(Card(CARD_CREW, LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuGangHeadCrew", "#LF-PlayerMenuGang_HeadCrew")));
		parts.Insert(Card(CARD_CREW, LF_UIPart.CreatePair(PART_NAME, "#LF-PlayerMenuGang_Name", data.m_sName)));
		parts.Insert(Card(CARD_CREW, LF_UIPart.CreatePair(PART_FLAG, "#LF-PlayerMenuGang_Flag", FlagKey(data.m_iFlag))));
		parts.Insert(Card(CARD_CREW, LF_UIPart.CreatePair(PART_RANK, "#LF-PlayerMenuGang_Rank", data.m_sRank).WithIcon("job")));
		parts.Insert(Card(CARD_CREW, LF_UIPart.CreatePair(PART_RADIO, "#LF-PlayerMenuGang_Radio", RadioKey(data.m_eRadio))));

		parts.Insert(CardIn(1, CARD_ONLINE));
		parts.Insert(Card(CARD_ONLINE, LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuGangHeadOnline", "#LF-PlayerMenuGang_HeadOnline")));
		if (data.m_aOnlineNames.IsEmpty())
			parts.Insert(Card(CARD_ONLINE, LF_UIPart.Create(LF_EUIPartKind.LINE, ROW_ONLINE + "none", "#LF-PlayerMenuGang_OnlineNone")));

		foreach (int i, string name : data.m_aOnlineNames)
		{
			if (i >= ONLINE_SHOWN)
				break;

			parts.Insert(Card(CARD_ONLINE, LF_UIPart.CreatePair(ROW_ONLINE + i.ToString(), name, data.m_aOnlineRanks[i])));
		}

		parts.Insert(CardIn(2, CARD_SITES));
		parts.Insert(Card(CARD_SITES, LF_UIPart.Create(LF_EUIPartKind.HEADING, "PlayerMenuGangHeadSites", "#LF-PlayerMenuGang_HeadSites")));
		if (data.m_aSiteIds.IsEmpty())
			parts.Insert(Card(CARD_SITES, LF_UIPart.Create(LF_EUIPartKind.LINE, ROW_SITE + "none", "#LF-PlayerMenuGang_SitesNone")));

		foreach (int s, string site : data.m_aSiteIds)
		{
			parts.Insert(Card(CARD_SITES, LF_UIPart.CreatePair(ROW_SITE + s.ToString(), LF_OrgHeldSites.Label(site, data.m_aSiteCaptured[s]), LF_OrgHeldSites.MarkKey(data.m_aSiteCaptured[s])).WithIcon("house")));
		}
	}
}
