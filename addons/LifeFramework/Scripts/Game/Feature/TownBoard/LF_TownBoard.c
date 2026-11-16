// LF_TownBoard.c — the cards on the town board: what there is to do in town and where it is.
//
// Data only, read on every machine: Configs/TownBoard/LF_TownBoard.conf (the core ships no cards)
// plus the pack's overlays, which add the cards with their text in the pack's string table. A card
// names its place by an LF_Places kind and id (a sign id by default), so the board finds it by
// component on any map. Nothing here is saved or sent over the network.

//------------------------------------------------------------------------------------------------
//! One card on the board. A pack overlay adds, replaces, patches or removes a card by id.
[BaseContainerProps()]
class LF_TownBoardCard : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Card id (stable; the waypoint remembers it)")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Category printed in red at the top of the card (key or text): WORK, SHOPS, ...")]
	string m_sCategory;

	[Attribute("", UIWidgets.EditBox, "Card title. The prompt reads Read: <title>")]
	string m_sTitle;

	[Attribute("", UIWidgets.EditBox, "One short line under the title")]
	string m_sLine;

	[Attribute("", UIWidgets.EditBox, "The detail sheet's text. | or a newline between items makes a list; *word* is printed in the accent colour")]
	string m_sDetail;

	[Attribute("Sign", UIWidgets.EditBox, "LF_Places kind of the place this card points at. Sign: a sign prop by its sign id")]
	string m_sPlaceKind;

	[Attribute("", UIWidgets.EditBox, "LF_Places id of the place (for kind Sign, the sign id). Empty: the card has no place")]
	string m_sPlaceId;

	[Attribute("", UIWidgets.EditBox, "Name of the place on the card. Empty: the sign's title (kind Sign)")]
	string m_sPlaceName;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_TownBoardConfig
{
	[Attribute("#LF-TownBoard_Title", UIWidgets.EditBox, "Header printed on the board (key or text)")]
	string m_sTitle;

	[Attribute("", UIWidgets.Object, "Cards, in board order: left to right, then the next row. The board shows the first 8")]
	ref array<ref LF_TownBoardCard> m_aCards;

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class the mission header lists, in load order.
	//! \return Cards changed.
	int ApplyPackOverlays()
	{
		if (!m_aCards)
			m_aCards = new array<ref LF_TownBoardCard>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_TownBoard.CONFIG_PATH, LF_TownBoardConfig, "townBoard", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		int total;
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LF_TownBoard.FEATURE, string.Format("pack overlay %1 changed %2 card(s)", names[i], changed));
			total += changed;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one overlay container: its title when set, and its cards by the override verbs.
	//! \return Cards changed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		if (!m_aCards)
			m_aCards = new array<ref LF_TownBoardCard>();

		if (container.IsVariableSetDirectly("m_sTitle"))
			container.Get("m_sTitle", m_sTitle);

		return LF_ConfigOverlay.ApplyContainerList(new LF_TownBoardCardList(this), container, "m_aCards", source, claims);
	}
}

//------------------------------------------------------------------------------------------------
//! The cards on the override model, keyed by card id. A new or replaced card needs a title.
class LF_TownBoardCardList : LF_ConfigList
{
	protected LF_TownBoardConfig m_Config;

	void LF_TownBoardCardList(LF_TownBoardConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "townBoard.cards";
	}

	override int Count()
	{
		return m_Config.m_aCards.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aCards[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_TownBoardCard.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_TownBoardCard card = new LF_TownBoardCard();
		card.m_sId = id;
		card.m_sPlaceKind = LF_SignComponent.PLACE_KIND;
		return card;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_TownBoardCard card = LF_TownBoardCard.Cast(record);
		if (index < m_Config.m_aCards.Count())
			m_Config.m_aCards.Set(index, card);
		else
			m_Config.m_aCards.Insert(card);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aCards.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_TownBoardCard card = LF_TownBoardCard.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sCategory") && container.Get("m_sCategory", card.m_sCategory))
			changed++;
		if (container.IsVariableSetDirectly("m_sTitle") && container.Get("m_sTitle", card.m_sTitle))
			changed++;
		if (container.IsVariableSetDirectly("m_sLine") && container.Get("m_sLine", card.m_sLine))
			changed++;
		if (container.IsVariableSetDirectly("m_sDetail") && container.Get("m_sDetail", card.m_sDetail))
			changed++;
		if (container.IsVariableSetDirectly("m_sPlaceKind") && container.Get("m_sPlaceKind", card.m_sPlaceKind))
			changed++;
		if (container.IsVariableSetDirectly("m_sPlaceId") && container.Get("m_sPlaceId", card.m_sPlaceId))
			changed++;
		if (container.IsVariableSetDirectly("m_sPlaceName") && container.Get("m_sPlaceName", card.m_sPlaceName))
			changed++;

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_TownBoardCard.Cast(record).m_sTitle.IsEmpty())
			return "a town board card needs a title (m_sTitle)";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The board's cards and what a card says about its place. Every machine; no state but the config.
class LF_TownBoard
{
	static const string FEATURE = "TownBoard";
	static const ResourceName CONFIG_PATH = "{7C1E5A93D24B6F08}Configs/TownBoard/LF_TownBoard.conf";

	//! The board has room for two rows of four cards (LF_TownBoardFace).
	static const int MAX_CARDS = 8;	// lf-fixed: the face's slots and the prefab's card contexts
	//! A place this close to the board is "right here" rather than a distance and a direction.
	static const float NEAR_METERS = 15;	// lf-fixed: reads better than "10 m north" across a square

	static const string KEY_TITLE = "#LF-TownBoard_Title";
	static const string KEY_WHERE = "#LF-TownBoard_Where";
	static const string KEY_WHERE_NEAR = "#LF-TownBoard_WhereNear";
	static const string KEY_NO_PLACE = "#LF-TownBoard_NoPlace";
	//! Compass points from north clockwise, 45 degrees apart.
	static const ref array<string> DIRECTION_KEYS = {"#LF-TownBoard_Dir_N", "#LF-TownBoard_Dir_NE", "#LF-TownBoard_Dir_E", "#LF-TownBoard_Dir_SE", "#LF-TownBoard_Dir_S", "#LF-TownBoard_Dir_SW", "#LF-TownBoard_Dir_W", "#LF-TownBoard_Dir_NW"};

	protected static ref LF_TownBoardConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads a town board config. Logs and returns null when it does not resolve.
	static LF_TownBoardConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, "town board config does not resolve: " + path);
			return null;
		}

		LF_TownBoardConfig config = LF_TownBoardConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error(FEATURE, "town board config has an unexpected root class: " + path);

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The config in use: the core file with the pack overlays merged, loaded once.
	static LF_TownBoardConfig Config()
	{
		if (!s_bLoadAttempted)
		{
			s_bLoadAttempted = true;
			s_Config = Load(CONFIG_PATH);
			if (s_Config)
			{
				s_Config.ApplyPackOverlays();
				LF_Debug.Info(FEATURE, string.Format("%1 card(s) on the board", CardCount()));
			}
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this config instead of the file (null: load the file again on the next read).
	static void UseConfig(LF_TownBoardConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = config != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Cards shown: the config's, at most MAX_CARDS.
	static int CardCount()
	{
		LF_TownBoardConfig config = s_Config;
		if (!s_bLoadAttempted)
			config = Config();

		if (!config || !config.m_aCards)
			return 0;

		return Math.Min(config.m_aCards.Count(), MAX_CARDS);
	}

	//------------------------------------------------------------------------------------------------
	//! The card in board slot `slot`, or null for an empty slot.
	static LF_TownBoardCard CardAt(int slot)
	{
		if (slot < 0 || slot >= CardCount())
			return null;

		return Config().m_aCards[slot];
	}

	//------------------------------------------------------------------------------------------------
	//! The card with this id among the shown ones, or null.
	static LF_TownBoardCard FindCard(string id)
	{
		for (int i = 0; i < CardCount(); i++)
		{
			LF_TownBoardCard card = CardAt(i);
			if (card && card.m_sId == id)
				return card;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The board's printed header, upper case.
	static string Title()
	{
		string title;
		LF_TownBoardConfig config = Config();
		if (config)
			title = Text(config.m_sTitle);

		if (title.IsEmpty())
			title = Text(KEY_TITLE);

		title.ToUpper();
		return title;
	}

	//------------------------------------------------------------------------------------------------
	//! A #key translated, anything else as written; an unknown key (pack not loaded) is empty.
	static string Text(string value)
	{
		return LF_SignCatalog.Resolve(value);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the card's place stands. False (origin untouched) when the card has no place or the
	//! place is not on this map.
	static bool FindPlace(LF_TownBoardCard card, out vector origin)
	{
		if (!card || card.m_sPlaceId.IsEmpty())
			return false;

		string kind = card.m_sPlaceKind;
		if (kind.IsEmpty())
			kind = LF_SignComponent.PLACE_KIND;

		return LF_Places.FindOrigin(kind, card.m_sPlaceId, origin);
	}

	//------------------------------------------------------------------------------------------------
	//! The name of the card's place: its own, else the title of the sign it points at.
	static string PlaceName(LF_TownBoardCard card)
	{
		if (!card)
			return string.Empty;

		string name = Text(card.m_sPlaceName);
		if (!name.IsEmpty())
			return name;

		if (card.m_sPlaceKind.IsEmpty() || card.m_sPlaceKind == LF_SignComponent.PLACE_KIND)
		{
			LF_SignDef sign = LF_SignCatalog.Find(card.m_sPlaceId);
			if (sign)
				name = Text(sign.m_sTitle);
		}

		if (name.IsEmpty())
			name = Text(card.m_sTitle);

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Compass point from `from` to `to` (+Z north, +X east): 0 north, 1 north-east, ... 7 north-west.
	static int Direction(vector from, vector to)
	{
		float dx = to[0] - from[0];
		float dz = to[2] - from[2];
		float degrees = Math.Atan2(dx, dz) * Math.RAD2DEG;
		if (degrees < 0)
			degrees += 360;

		int point = Math.Round(degrees / 45);
		return point % 8;
	}

	//------------------------------------------------------------------------------------------------
	//! A distance as a sign would print it: to 10 m under 200 m, else to 50 m; never under 10.
	static int RoundDistance(float meters)
	{
		int step = 10;
		if (meters >= 200)
			step = 50;

		int rounded = Math.Round(meters / step) * step;
		return Math.Max(rounded, 10);
	}

	//------------------------------------------------------------------------------------------------
	//! "Bank, 120 m east" from `from`; "Bank, right here" close by; a line for no place.
	static string WhereText(LF_TownBoardCard card, vector from)
	{
		vector place;
		if (!FindPlace(card, place))
			return Text(KEY_NO_PLACE);

		string name = PlaceName(card);
		float flat = vector.DistanceXZ(from, place);
		if (flat < NEAR_METERS)
			return WidgetManager.Translate(KEY_WHERE_NEAR, name);

		string direction = Text(DIRECTION_KEYS[Direction(from, place)]);
		return WidgetManager.Translate(KEY_WHERE, name, RoundDistance(flat), direction);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_TownBoard");
		if (apply)
			LF_TownBoard.ResetStatics();
	}
}
