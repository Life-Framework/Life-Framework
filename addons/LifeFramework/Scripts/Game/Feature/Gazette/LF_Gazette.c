// LF_Gazette.c - the town paper, built from the event bus (#792). Server only.
// A subscriber per event class the config lists turns each event into a plain item; an edition
// ranks the waiting items, caps each kind, withholds the name of anyone not identified, and stores
// KEYS plus arguments (never formatted text), so a language or term change re-renders old editions.
// Reading goes through RpcAsk_LF_Gazette / RpcDo_LF_ShowGazette (LF_GazetteRead.c).
// NOT persisted yet: pending items and editions live for the session (see Feature.md).

//------------------------------------------------------------------------------------------------
//! One thing that happened, in plain values, waiting for the next edition.
class LF_GazetteItem : Managed
{
	ref LF_GazetteRule m_Rule;
	int m_iSequence;
	string m_sUid;
	string m_sDetail;
	int m_iAmount;
}

//------------------------------------------------------------------------------------------------
//! One printed headline: the story key and its arguments. %1 name (empty when withheld), %2 detail, %3 number.
class LF_GazetteRow : Managed
{
	static const string SEPARATOR = "|";

	string m_sKind;
	string m_sTextKey;
	ref array<string> m_aArgs = {};

	//------------------------------------------------------------------------------------------------
	string Arg(int index)
	{
		if (index < 0 || index >= m_aArgs.Count())
			return string.Empty;

		return m_aArgs[index];
	}

	//------------------------------------------------------------------------------------------------
	//! One string for the reply RPC: kind|textKey|arg1|arg2|arg3.
	string Pack()
	{
		return string.Format("%1|%2|%3|%4|%5", m_sKind, m_sTextKey, Arg(0), Arg(1), Arg(2));
	}

	//------------------------------------------------------------------------------------------------
	static LF_GazetteRow Unpack(string packed)
	{
		array<string> parts = {};
		packed.Split(SEPARATOR, parts, false);
		if (parts.Count() < 5)
			return null;

		LF_GazetteRow row = new LF_GazetteRow();
		row.m_sKind = parts[0];
		row.m_sTextKey = parts[1];
		row.m_aArgs.Insert(parts[2]);
		row.m_aArgs.Insert(parts[3]);
		row.m_aArgs.Insert(parts[4]);
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_GazetteEdition : Managed
{
	int m_iNumber;
	int m_iIssuedAt;
	ref array<ref LF_GazetteRow> m_aRows = {};
}

//------------------------------------------------------------------------------------------------
//! Reads the fields a story needs off a bus event. One branch per event class the core knows;
//! the config decides which of them count.
class LF_GazetteAdapter
{
	//------------------------------------------------------------------------------------------------
	//! \return False when this event is not a story (a jammed vault, a death with no killer).
	static bool Read(notnull LF_Event busEvent, notnull LF_GazetteItem item)
	{
		LF_OnCrimeCommitted crime = LF_OnCrimeCommitted.Cast(busEvent);
		if (crime)
		{
			item.m_sUid = crime.m_sPlayerUid;
			item.m_sDetail = crime.m_sOffence;
			return true;
		}

		LF_OnArrested arrested = LF_OnArrested.Cast(busEvent);
		if (arrested)
		{
			item.m_sUid = arrested.m_sPlayerUid;
			item.m_iAmount = arrested.m_iSentenceSeconds / 60;
			return true;
		}

		LF_OnVehicleChopped chopped = LF_OnVehicleChopped.Cast(busEvent);
		if (chopped)
		{
			item.m_sUid = chopped.m_sChopperUid;
			item.m_iAmount = chopped.m_iPayout;
			return true;
		}

		LF_OnHeistLoot heist = LF_OnHeistLoot.Cast(busEvent);
		if (heist)
		{
			item.m_sUid = heist.m_sPlayerUid;
			item.m_sDetail = heist.m_sKind;
			item.m_iAmount = heist.m_iAmount;
			return true;
		}

		LF_OnVaultBreach vault = LF_OnVaultBreach.Cast(busEvent);
		if (vault)
		{
			item.m_sUid = vault.m_sPlayerUid;
			return vault.m_sOutcome == "opened";
		}

		LF_OnPlayerKilled killed = LF_OnPlayerKilled.Cast(busEvent);
		if (killed)
		{
			item.m_sUid = killed.m_sKillerUid;
			return killed.m_bMurder && !killed.m_sKillerUid.IsEmpty();
		}

		LF_OnJobCompleted job = LF_OnJobCompleted.Cast(busEvent);
		if (job)
		{
			item.m_iAmount = job.m_iPay;
			return true;
		}

		LF_OnFenceSale sale = LF_OnFenceSale.Cast(busEvent);
		if (sale)
		{
			item.m_sUid = sale.m_sBuyerUid;
			item.m_iAmount = sale.m_iPrice;
			return true;
		}

		LF_OnPatientDelivered patient = LF_OnPatientDelivered.Cast(busEvent);
		if (patient)
		{
			item.m_sUid = patient.m_sMedicUid;
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_GazetteSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		return LF_Gazette.Collect(busEvent);
	}
}

//------------------------------------------------------------------------------------------------
//! Remembers who the police identified; their name may print from then on.
class LF_GazetteIdentifiedSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnSuspectIdentified identified = LF_OnSuspectIdentified.Cast(busEvent);
		if (!identified)
			return false;

		LF_Gazette.MarkIdentified(identified.m_sPlayerUid);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Gazette
{
	static const string FEATURE = "Gazette";

	protected static ref array<ref LF_GazetteItem> s_aItems;
	protected static ref array<ref LF_GazetteEdition> s_aEditions;
	protected static ref map<string, bool> s_mIdentified;
	protected static ref map<string, string> s_mTestNames;
	protected static ref array<ref LF_EventSubscriber> s_aSubscribers;
	protected static ref array<string> s_aSubscribedClasses;
	protected static int s_iSequence;
	protected static int s_iEditionNumber;
	protected static int s_iLastIssued;
	protected static bool s_bInstalled;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_GazetteItem> Items()
	{
		if (!s_aItems)
			s_aItems = {};

		return s_aItems;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_GazetteEdition> Editions()
	{
		if (!s_aEditions)
			s_aEditions = {};

		return s_aEditions;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		if (s_aSubscribers && s_aSubscribedClasses)
		{
			for (int i = 0; i < s_aSubscribers.Count() && i < s_aSubscribedClasses.Count(); i++)
			{
				LF_EventBus.Unsubscribe(s_aSubscribedClasses[i].ToType(), s_aSubscribers[i]);
			}
		}

		s_aSubscribers = null;
		s_aSubscribedClasses = null;
		s_aItems = null;
		s_aEditions = null;
		s_mIdentified = null;
		s_mTestNames = null;
		s_iSequence = 0;
		s_iEditionNumber = 0;
		s_iLastIssued = 0;
		s_bInstalled = false;
		LF_GazetteConfig.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe to every event class the config lists, plus the identification. Idempotent.
	static void Install()
	{
		if (!Replication.IsServer() || s_bInstalled)
			return;

		if (!LF_Systems.IsEnabled("Gazette"))
			return;

		LF_GazetteConfig config = LF_GazetteConfig.Get();
		if (!config || !config.m_aRules)
		{
			LF_Debug.Warn(FEATURE, "gazette not installed: no config");
			return;
		}

		s_aSubscribers = {};
		s_aSubscribedClasses = {};
		foreach (LF_GazetteRule rule : config.m_aRules)
		{
			if (!rule)
				continue;

			typename eventType = rule.m_sEvent.ToType();
			if (!eventType || !eventType.IsInherited(LF_Event))
			{
				LF_Debug.Error(FEATURE, string.Format("rule skipped: '%1' is not a bus event class", rule.m_sEvent));
				continue;
			}

			if (s_aSubscribedClasses.Find(rule.m_sEvent) >= 0)
				continue;

			LF_GazetteSubscriber subscriber = new LF_GazetteSubscriber();
			if (LF_EventBus.Subscribe(eventType, subscriber))
			{
				s_aSubscribers.Insert(subscriber);
				s_aSubscribedClasses.Insert(rule.m_sEvent);
			}
		}

		LF_GazetteIdentifiedSubscriber identified = new LF_GazetteIdentifiedSubscriber();
		if (LF_EventBus.Subscribe(LF_OnSuspectIdentified, identified))
		{
			s_aSubscribers.Insert(identified);
			s_aSubscribedClasses.Insert("LF_OnSuspectIdentified");
		}

		s_iLastIssued = System.GetUnixTime();
		s_bInstalled = true;
		GetGame().GetCallqueue().CallLater(Tick, 60000, true);
		LF_Debug.Info(FEATURE, string.Format("gazette installed: %1 event class(es), an edition every %2 min", s_aSubscribedClasses.Count(), config.m_iEditionIntervalMinutes));
	}

	//------------------------------------------------------------------------------------------------
	static void MarkIdentified(string uid)
	{
		if (uid.IsEmpty())
			return;

		if (!s_mIdentified)
			s_mIdentified = new map<string, bool>();

		s_mIdentified.Set(uid, true);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsIdentified(string uid)
	{
		if (uid.IsEmpty() || !s_mIdentified)
			return false;

		return s_mIdentified.Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: a name for a uid without an account manager.
	static void SetTestName(string uid, string name)
	{
		if (!s_mTestNames)
			s_mTestNames = new map<string, string>();

		s_mTestNames.Set(uid, name);
	}

	//------------------------------------------------------------------------------------------------
	//! The character's full name, empty when unknown.
	static string NameOf(string uid)
	{
		if (s_mTestNames && s_mTestNames.Contains(uid))
			return s_mTestNames.Get(uid);

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return string.Empty;

		LF_PlayerCharacter character = accounts.FindCharacterByKey(uid);
		if (!character)
			return string.Empty;

		return character.GetFullName();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a bus event becomes a waiting item when a rule counts it.
	//! \return False when the event is not a story or no rule counts it.
	static bool Collect(notnull LF_Event busEvent)
	{
		LF_GazetteConfig config = LF_GazetteConfig.Get();
		if (!config)
			return false;

		LF_GazetteRule rule = config.FindRule(busEvent.Type().ToString());
		if (!rule)
			return false;

		LF_GazetteItem item = new LF_GazetteItem();
		item.m_Rule = rule;
		if (!LF_GazetteAdapter.Read(busEvent, item))
			return false;

		s_iSequence++;
		item.m_iSequence = s_iSequence;
		array<ref LF_GazetteItem> items = Items();
		items.Insert(item);
		while (config.m_iMaxPending > 0 && items.Count() > config.m_iMaxPending)
		{
			items.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("%1 collected as '%2' (%3 waiting)", busEvent.Type().ToString(), rule.m_sKind, items.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int PendingCount()
	{
		return Items().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: one waiting item as a headline row, or null when the story is dropped. The name is
	//! withheld unless the rule names nobody, or the suspect is identified (or the rule is public).
	protected static LF_GazetteRow MakeRow(notnull LF_GazetteItem item)
	{
		LF_GazetteRule rule = item.m_Rule;
		string name;
		string textKey = rule.m_sTextKey;
		if (rule.m_bNamed)
		{
			bool mayName = !rule.m_bNeedsIdentified || IsIdentified(item.m_sUid);
			if (mayName)
				name = NameOf(item.m_sUid);

			if (name.IsEmpty())
			{
				if (rule.m_sAnonKey.IsEmpty())
					return null;

				textKey = rule.m_sAnonKey;
			}
		}

		LF_GazetteRow row = new LF_GazetteRow();
		row.m_sKind = rule.m_sKind;
		row.m_sTextKey = textKey;
		row.m_aArgs.Insert(name);
		row.m_aArgs.Insert(item.m_sDetail);
		row.m_aArgs.Insert(item.m_iAmount.ToString());
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the waiting items become an edition: highest priority first (newest first among equals),
	//! at most the per-kind cap of any kind, at most the max headlines. The items are consumed.
	//! \return The new edition, or null when nothing was worth printing (the paper does not run).
	static LF_GazetteEdition BuildEdition(int nowSeconds)
	{
		LF_GazetteConfig config = LF_GazetteConfig.Get();
		if (!config)
			return null;

		array<ref LF_GazetteItem> items = Items();
		array<ref LF_GazetteItem> sorted = {};
		foreach (LF_GazetteItem item : items)
		{
			int at = sorted.Count();
			for (int i = 0; i < sorted.Count(); i++)
			{
				LF_GazetteItem other = sorted[i];
				if (item.m_Rule.m_iPriority > other.m_Rule.m_iPriority || (item.m_Rule.m_iPriority == other.m_Rule.m_iPriority && item.m_iSequence > other.m_iSequence))
				{
					at = i;
					break;
				}
			}

			sorted.InsertAt(item, at);
		}

		LF_GazetteEdition edition = new LF_GazetteEdition();
		map<string, int> perKind = new map<string, int>();
		foreach (LF_GazetteItem entry : sorted)
		{
			if (config.m_iMaxHeadlines > 0 && edition.m_aRows.Count() >= config.m_iMaxHeadlines)
				break;

			int cap = entry.m_Rule.m_iKindCap;
			if (cap <= 0)
				cap = config.m_iPerKindCap;

			int used = perKind.Get(entry.m_Rule.m_sKind);
			if (cap > 0 && used >= cap)
				continue;

			LF_GazetteRow row = MakeRow(entry);
			if (!row)
				continue;

			perKind.Set(entry.m_Rule.m_sKind, used + 1);
			edition.m_aRows.Insert(row);
		}

		int consumed = items.Count();
		items.Clear();
		s_iLastIssued = nowSeconds;
		if (edition.m_aRows.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("no edition: nothing to print from %1 item(s)", consumed));
			return null;
		}

		s_iEditionNumber++;
		edition.m_iNumber = s_iEditionNumber;
		edition.m_iIssuedAt = nowSeconds;
		array<ref LF_GazetteEdition> editions = Editions();
		editions.Insert(edition);
		while (config.m_iEditionsKept > 0 && editions.Count() > config.m_iEditionsKept)
		{
			editions.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("edition %1 issued: %2 headline(s) from %3 item(s)", edition.m_iNumber, edition.m_aRows.Count(), consumed));
		return edition;
	}

	//------------------------------------------------------------------------------------------------
	//! Server timer: issue an edition when the interval has passed.
	static void Tick()
	{
		LF_GazetteConfig config = LF_GazetteConfig.Get();
		if (!config || !Replication.IsServer())
			return;

		int now = System.GetUnixTime();
		if (now - s_iLastIssued < config.m_iEditionIntervalMinutes * 60)
			return;

		BuildEdition(now);
	}

	//------------------------------------------------------------------------------------------------
	static int EditionCount()
	{
		return Editions().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The edition the stand shows, or null before the first one.
	static LF_GazetteEdition Latest()
	{
		array<ref LF_GazetteEdition> editions = Editions();
		if (editions.IsEmpty())
			return null;

		return editions[editions.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the story as a reader sees it. Arguments that are keys are translated, then the terms
	//! placeholders ({police}, {Town}) are expanded by the one text path.
	static string RenderRow(notnull LF_GazetteRow row)
	{
		string a1 = ShownArg(row.Arg(0));
		string a2 = ShownArg(row.Arg(1));
		string a3 = ShownArg(row.Arg(2));
		return LF_Terms.Text(row.m_sTextKey, a1, a2, a3);
	}

	//------------------------------------------------------------------------------------------------
	protected static string ShownArg(string arg)
	{
		if (arg.StartsWith("#"))
			return WidgetManager.Translate(arg);

		return arg;
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the read RPC: the latest edition for the reader at the stand.
	static void ServerRead(SCR_PlayerController controller)
	{
		if (!controller)
			return;

		IEntity reader = controller.GetControlledEntity();
		if (!reader || !LF_ActionSite.IsNear(reader.GetOrigin(), LF_GazetteSheet.READ_DISTANCE, LF_GazetteSheet.READ_ACTION))
		{
			LF_Debug.Warn(FEATURE, string.Format("gazette read refused for player %1: not at a newsstand", controller.GetPlayerId()));
			return;
		}

		LF_GazetteConfig config = LF_GazetteConfig.Get();
		string paperKey = "#LF-Gazette_Title";
		if (config)
			paperKey = config.m_sPaperNameKey;

		int number;
		array<string> packed = {};
		LF_GazetteEdition edition = Latest();
		if (edition)
		{
			number = edition.m_iNumber;
			foreach (LF_GazetteRow row : edition.m_aRows)
			{
				packed.Insert(row.Pack());
			}
		}

		LF_Debug.Info(FEATURE, string.Format("gazette read by player %1: edition %2, %3 headline(s)", controller.GetPlayerId(), number, packed.Count()));
		controller.LF_ShowGazette(paperKey, number, packed);
	}
}
