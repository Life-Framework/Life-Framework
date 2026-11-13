//------------------------------------------------------------------------------------------------
//! One poster on the wanted board's face, decoded on the client.
class LF_WantedPosterEntry : Managed
{
	string m_sKind;
	//! A person: empty until the server marked the report identified. A stolen car: the plate.
	string m_sName;
	//! The crime catalog id (#685a), a localization key such as #LF-Offence_Robbery.
	string m_sOffence;
	int m_iLevel;
	//! Clothing display-name keys joined by '|', as LF_WantedReportRecord.m_sClothing.
	string m_sClothing;
	//! Vehicle display-name key, empty on foot.
	string m_sVehicle;
	//! The suspect's look at the crime (LF_WantedLook); empty draws an empty photo frame.
	string m_sAppearance;
}

//------------------------------------------------------------------------------------------------
//! The wanted board's public poster list (docs/architecture/visual-boards-spike.md, issue 1). The
//! server builds it from the crime reports (LF_WantedReports) and the wanted levels, one poster per
//! suspect, newest first, and hands it to every wanted board (LF_WantedPosterBoardComponent), which
//! sends it to clients: RplSave/RplLoad for a late joiner or a board streaming in, one reliable
//! broadcast when it changes. A change is an event on the bus (a crime, a wanted level, an
//! identification) or a load; changes inside COALESCE_MS make one broadcast. Nothing polls.
//!
//! An entry is a short string of ids: "P;name;offence id;level;clothing keys;vehicle key;look",
//! the look being prefab GUIDs (LF_WantedLook), never paths. It holds
//! what the public Read card shows (no place, the name only once identified); police-only lines
//! stay on the Read card (LF_WantedBoard.ServerRead).
class LF_WantedPosters
{
	static const string FEATURE = "WantedPosters";
	static const string KIND_PERSON = "P";
	static const string KIND_VEHICLE = "V";
	static const string FIELD = ";";
	//! Wait after the first change so a burst (a crime, then its wanted level) makes one broadcast.
	static const int COALESCE_MS = 1500;	// lf-fixed: replication batching, not gameplay
	//! The fields of a person entry.
	static const int PERSON_FIELDS = 6;	// lf-fixed: the entry format
	//! The fields of a stolen car entry: "V;plate;name key;prefab GUID".
	static const int VEHICLE_FIELDS = 4;	// lf-fixed: the entry format

	protected static ref array<string> s_aEntries;
	protected static string s_sSignature;
	protected static bool s_bScheduled;
	protected static int s_iBroadcasts;
	protected static ref LF_WantedPostersSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Flush);

		s_aEntries = null;
		s_sSignature = string.Empty;
		s_bScheduled = false;
		s_iBroadcasts = 0;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for the events that change a poster. Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnWantedChanged) > 0)
			return true;

		s_Subscriber = new LF_WantedPostersSubscriber();
		bool ok = LF_EventBus.Subscribe(LF_OnCrimeCommitted, s_Subscriber);
		ok = LF_EventBus.Subscribe(LF_OnWantedChanged, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnSuspectIdentified, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnVehicleReportedStolen, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnVehicleRecovered, s_Subscriber) && ok;
		ok = LF_EventBus.Subscribe(LF_OnVehicleChopped, s_Subscriber) && ok;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a board came into play. It takes the current list at once (its RplSave carries it to
	//! clients), or the list is built if it never was.
	static void BoardJoined(notnull LF_WantedPosterBoardComponent board)
	{
		if (s_aEntries)
			board.ServerSet(s_aEntries, LF_WantedPostersConfig.DrawRadius(), false);
		else
			MarkDirty("board");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: something a poster shows changed. The list is rebuilt once, COALESCE_MS later.
	static void MarkDirty(string why)
	{
		if (!Replication.IsServer() || s_bScheduled)
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			Flush();
			return;
		}

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Flush, COALESCE_MS, false);
		LF_Debug.Log(FEATURE, "rebuild queued: " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuild the list; when it differs from what was sent, every board sends it.
	//! \return True when the list changed.
	static bool Flush()
	{
		s_bScheduled = false;
		array<string> entries = {};
		BuildEntries(LF_WantedPostersConfig.Posters(), entries, LF_WantedPostersConfig.StolenPosters());
		float radius = LF_WantedPostersConfig.DrawRadius();
		string signature = Signature(entries, radius);
		if (s_aEntries && signature == s_sSignature)
			return false;

		s_sSignature = signature;
		s_aEntries = entries;
		s_iBroadcasts++;
		int boards = LF_WantedPosterBoardComponent.SendToAll(entries, radius);

		LF_Debug.Info(FEATURE, string.Format("list changed: %1 poster(s), %2 chars, sent by %3 board(s)", entries.Count(), signature.Length(), boards));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A rebuild is waiting for its coalescing delay. Tests read it.
	static bool IsQueued()
	{
		return s_bScheduled;
	}

	//------------------------------------------------------------------------------------------------
	//! What the boards last sent (null before the first build). Tests read it.
	static array<string> Current()
	{
		return s_aEntries;
	}

	//------------------------------------------------------------------------------------------------
	//! Broadcasts since the server started. Tests read it.
	static int BroadcastCount()
	{
		return s_iBroadcasts;
	}

	//------------------------------------------------------------------------------------------------
	static string Signature(notnull array<string> entries, float radius)
	{
		string signature = radius.ToString();
		foreach (string entry : entries)
			signature += "\n" + entry;

		return signature;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the public posters, one per wanted suspect (their newest report), newest first, then
	//! up to `maxStolen` reported stolen cars, newest first, in the last places. People take the
	//! places the cars leave free.
	//! \return Number of posters.
	static int BuildEntries(int maxPosters, notnull array<string> outEntries, int maxStolen = 0)
	{
		outEntries.Clear();
		array<LF_StolenReport> stolen = {};
		LF_StolenVehicles.CollectNewestFirst(stolen);
		int cars = Math.Min(Math.Min(stolen.Count(), maxStolen), maxPosters);
		int people = maxPosters - cars;
		array<LF_WantedReportRecord> reports = {};
		LF_WantedReports.CollectNewestFirst(reports);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		array<string> seen = {};
		foreach (LF_WantedReportRecord report : reports)
		{
			if (outEntries.Count() >= people)
				break;

			if (seen.Contains(report.m_sPlayerUid))
				continue;

			seen.Insert(report.m_sPlayerUid);
			LF_PlayerCharacter suspect;
			LF_CharacterStanding standing;
			if (accounts)
			{
				suspect = accounts.FindCharacterByKey(report.m_sPlayerUid);
				standing = accounts.FindStanding(report.m_sPlayerUid);
			}

			string name;
			if (report.m_bIdentified && suspect)
				name = suspect.GetFullName();

			int level = 1;
			if (standing)
				level = standing.m_iWantedLevel;

			outEntries.Insert(EncodePerson(name, report.m_sOffence, level, report.m_sClothing, report.m_sVehicle, report.m_sAppearance));
		}

		for (int i = 0; i < cars; i++)
			outEntries.Insert(EncodeVehicle(stolen[i].m_sPlate, stolen[i].m_sNameKey, stolen[i].m_sPrefabGuid));

		return outEntries.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! A field never carries the separators: a typed name could otherwise shift the fields.
	static string Clean(string text)
	{
		string clean = text;
		clean.Replace(FIELD, ",");
		clean.Replace("\n", " ");
		return clean;
	}

	//------------------------------------------------------------------------------------------------
	static string EncodePerson(string name, string offence, int level, string clothing, string vehicle, string appearance = "")
	{
		return KIND_PERSON + FIELD + Clean(name) + FIELD + Clean(offence) + FIELD + level.ToString() + FIELD + Clean(clothing) + FIELD + Clean(vehicle) + FIELD + Clean(appearance);
	}

	//------------------------------------------------------------------------------------------------
	//! A reported stolen car: its plate, its name key and its prefab GUID, for the picture.
	static string EncodeVehicle(string plate, string nameKey, string prefabGuid)
	{
		return KIND_VEHICLE + FIELD + Clean(plate) + FIELD + Clean(nameKey) + FIELD + Clean(prefabGuid);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: an entry back into its fields; null (logged) when malformed.
	static LF_WantedPosterEntry Decode(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD, fields, false);
		if (fields.Count() >= VEHICLE_FIELDS && fields[0] == KIND_VEHICLE)
		{
			LF_WantedPosterEntry car = new LF_WantedPosterEntry();
			car.m_sKind = KIND_VEHICLE;
			car.m_sName = fields[1];
			car.m_sVehicle = fields[2];
			car.m_sAppearance = fields[3];
			return car;
		}

		if (fields.Count() < PERSON_FIELDS || fields[0] != KIND_PERSON)
		{
			LF_Debug.Error(FEATURE, "poster dropped, malformed entry: " + entry);
			return null;
		}

		LF_WantedPosterEntry poster = new LF_WantedPosterEntry();
		poster.m_sKind = fields[0];
		poster.m_sName = fields[1];
		poster.m_sOffence = fields[2];
		poster.m_iLevel = Math.ClampInt(fields[3].ToInt(), 1, 5);
		poster.m_sClothing = fields[4];
		poster.m_sVehicle = fields[5];
		if (fields.Count() > PERSON_FIELDS)
			poster.m_sAppearance = fields[PERSON_FIELDS];

		return poster;
	}

	//------------------------------------------------------------------------------------------------
	//! The poster's description line: "Wearing a, b; driving c". A part nobody saw is left off, and
	//! with nothing seen the line is empty and the poster hides it.
	static string Description(string clothing, string vehicle)
	{
		string wearing;
		array<string> pieces = {};
		clothing.Split("|", pieces, true);
		foreach (string piece : pieces)
		{
			if (!wearing.IsEmpty())
				wearing += ", ";

			wearing += LF_WantedBoard.ResolveKey(piece);
		}

		string text;
		if (!wearing.IsEmpty())
			text = WidgetManager.Translate("#LF-Wanted_Wearing", wearing);

		if (vehicle.IsEmpty())
			return text;

		if (text.IsEmpty())
			return WidgetManager.Translate("#LF-Wanted_PosterDriving", LF_WantedBoard.ResolveKey(vehicle));

		return text + "; " + WidgetManager.Translate("#LF-Wanted_Driving", LF_WantedBoard.ResolveKey(vehicle));
	}
}

//------------------------------------------------------------------------------------------------
//! Rebuilds the posters on the events that change one.
class LF_WantedPostersSubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "WantedPosters";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_WantedPosters.MarkDirty(busEvent.Type().ToString());
		return true;
	}
}
