// LF_BusTimetable.c - the printed timetable on a bus stop's pole. The stop prefab carries an
// LF_SignComponent (sign "bus_timetable", style "timetable") whose row source is this one: the rows
// are worked out on the reader's machine from the placed stops and the bus config, the same numbers
// the actions show and the server charges. First row: this stop, marked "you are here"; then every
// other stop nearest first with its fare. Nothing is sent or replicated.

//------------------------------------------------------------------------------------------------
class LF_BusTimetableSource : LF_SignRowSource
{
	static const string KIND = "bus_timetable";

	//------------------------------------------------------------------------------------------------
	override string GetKind()
	{
		return KIND;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsLocal()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void LocalRows(notnull IEntity sign, notnull array<ref LF_SignRow> rows)
	{
		array<IEntity> stops = {};
		LF_Places.FindAll(LF_BusStop.PLACE_KIND, stops);
		BuildRows(sign, stops, LF_BusStopConfig.Get(), rows);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure over the given stops: "you are here" for the sign's own stop, then the offers nearest first.
	//! A sign with no LF_BusStopComponent prints only the offers.
	static void BuildRows(notnull IEntity sign, notnull array<IEntity> stops, LF_BusStopConfig config, notnull array<ref LF_SignRow> rows)
	{
		LF_BusStopComponent here = LF_BusStopComponent.Of(sign);
		if (here)
			rows.Insert(LF_SignRow.Create(here.GetLabel(), "#LF-BusStop_BoardHere"));

		array<ref LF_BusOffer> offers = {};
		LF_BusTicket.Offers(sign, stops, config, offers);
		foreach (LF_BusOffer offer : offers)
		{
			rows.Insert(LF_SignRow.Create(offer.m_sLabel, LF_Terms.Text("#LF-BusStop_BoardFare", offer.m_iFare.ToString())));
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_SignRowSourceSet
{
	override void Collect(notnull array<ref LF_SignRowSource> sources)
	{
		super.Collect(sources);
		sources.Insert(new LF_BusTimetableSource());
	}
}
