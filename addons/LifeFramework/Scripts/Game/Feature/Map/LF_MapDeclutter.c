// LF_MapDeclutter.c: how the place icons sit on the open map, as on a printed paper map (0.2.0 island
// playtest: "the map markers on the map look bad"). Icons were drawn at their 64 px image size whatever the
// zoom, every place wrote its name beside it, and the Saint-Philippe square was a pile of icons and names
// written over each other. Now no name is written on the map (the card under the cursor names a place) and:
// - an icon's size follows the zoom (small when the view is wide, larger close in);
// - icons whose boxes overlap are one mark: the most important place keeps its icon, the card lists the
//   rest, and a click zooms in until they part.
// All in reference units of the screen the map is drawn on (y down). Pure, so LF_Test_MapDeclutter covers
// them with no map and no widgets.

//------------------------------------------------------------------------------------------------
class LF_MapDeclutter
{
	//! A place icon at the solo zoom, and how much it grows per zoom level closer in, up to ICON_MAX.
	static const float ICON_MIN = 22;	// lf-fixed: reference units; reads as a map pictogram, not a button
	static const float ICON_STEP = 3;	// lf-fixed: map readability, not a server tunable
	static const float ICON_MAX = 30;	// lf-fixed: the badge stays a pictogram at the closest zoom, not a button
	//! A cluster stamp (several places counted as one) at the far zooms.
	static const float STAMP_SIZE = 28;	// lf-fixed
	//! Two icons are one mark when their boxes overlap by this share of their size on both axes.
	static const float MERGE_SHARE = 0.85;	// lf-fixed: a sliver of overlap still reads as two icons
	//! The waypoint tag's type (LF_MapWaypointUI): LF_MapPoiLabel.layout's font size, and the estimated mean
	//! advance per character of RobotoCondensed Bold, on the generous side so the tag is never clipped.
	static const float LABEL_FONT = 17;	// lf-fixed: must match LF_MapPoiLabel.layout
	static const float CHAR_ADVANCE = 0.52;	// lf-fixed: share of the font size
	static const float LABEL_PAD = 8;	// lf-fixed: the paper outline round the letters
	static const float LABEL_HEIGHT = 22;	// lf-fixed
	//! The pointer hits an icon this far outside its box (a small target on a gamepad cursor).
	static const float HIT_PAD = 4;	// lf-fixed

	//------------------------------------------------------------------------------------------------
	//! The drawn size of a place icon (or a stamp) at a zoom (LF_MapClusterRules zoom: 12800 m / 2^zoom wide).
	static float IconSize(float zoom, bool stamp)
	{
		if (stamp)
			return STAMP_SIZE;

		float size = ICON_MIN + (zoom - LF_MapClusterRules.SOLO_ZOOM) * ICON_STEP;
		if (size < ICON_MIN)
			return ICON_MIN;

		if (size > ICON_MAX)
			return ICON_MAX;

		return size;
	}

	//------------------------------------------------------------------------------------------------
	//! The width a tag takes on the map. An empty one takes none.
	static float LabelWidth(string text)
	{
		if (text.IsEmpty())
			return 0;

		return text.Length() * LABEL_FONT * CHAR_ADVANCE + LABEL_PAD;
	}

	//------------------------------------------------------------------------------------------------
	//! Which mark wins a crowded spot, lower first: a stamp, then places in the legend's category order, a
	//! place only heard of after every visited one, a pencil mark last.
	static int Priority(string category, LF_EMapPoiState state, bool stamp)
	{
		if (stamp)
			return 0;

		int rank = LF_MapLegend.s_aOrder.Find(category);
		if (rank < 0)
			rank = LF_MapLegend.s_aOrder.Count();

		int priority = 10 + rank;
		if (state == LF_EMapPoiState.FADED)
			priority += 100;
		else if (state == LF_EMapPoiState.PENCIL)
			priority += 200;

		return priority;
	}

	//------------------------------------------------------------------------------------------------
	//! The indexes 0..n-1 by priority, lower first; equal priorities keep their input order.
	static void Order(notnull array<int> priorities, notnull array<int> into)
	{
		into.Clear();
		foreach (int i, int priority : priorities)
		{
			int at = into.Count();
			while (at > 0 && priorities[into[at - 1]] > priority)
			{
				at--;
			}

			into.InsertAt(i, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One mark per crowded spot. Walks `order`; an item whose box overlaps a mark already kept joins it,
	//! else it is a mark of its own. leadOf[i] is the index of the mark item i is drawn as (i for a mark).
	static void Merge(notnull array<float> xs, notnull array<float> ys, notnull array<float> sizes, notnull array<int> order, notnull array<int> leadOf)
	{
		leadOf.Clear();
		for (int n = 0; n < xs.Count(); n++)
		{
			leadOf.Insert(n);
		}

		array<int> kept = {};
		foreach (int i : order)
		{
			int joined = -1;
			foreach (int k : kept)
			{
				float reach = (sizes[i] + sizes[k]) * 0.5 * MERGE_SHARE;
				if (Math.AbsFloat(xs[i] - xs[k]) < reach && Math.AbsFloat(ys[i] - ys[k]) < reach)
				{
					joined = k;
					break;
				}
			}

			if (joined == -1)
				kept.Insert(i);
			else
				leadOf[i] = joined;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The mark under the pointer at (px, py): its icon, padded by HIT_PAD. Marks are tried in index order,
	//! so pass them most important first. Hidden marks (shown false) are skipped.
	//! \return the index, or -1 when the pointer is over none.
	static int Hit(float px, float py, notnull array<float> xs, notnull array<float> ys, notnull array<float> sizes, notnull array<bool> shown)
	{
		foreach (int i, float x : xs)
		{
			if (!shown[i])
				continue;

			float half = sizes[i] * 0.5 + HIT_PAD;
			if (Math.AbsFloat(px - x) <= half && Math.AbsFloat(py - ys[i]) <= half)
				return i;
		}

		return -1;
	}
}
