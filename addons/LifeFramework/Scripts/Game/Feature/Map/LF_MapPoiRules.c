//------------------------------------------------------------------------------------------------
//! How a POI row wants to be shown (docs/architecture/everon-map.md section 1, "Visibility").
enum LF_EMapPoiVisibility
{
	//! Shows whenever the place is live.
	PUBLIC,
	//! Shows once the player has heard of the place or visited it.
	DISCOVER,
	//! Never public. Only a personal pencil mark after the player met it.
	HIDDEN,
	//! Shows while the player holds the row's role (police, medic).
	ROLE
}

//------------------------------------------------------------------------------------------------
//! What one player knows of one place (section 4, "Per player: what you know").
enum LF_EMapPoiKnown
{
	NONE,
	HEARD,
	VISITED
}

//------------------------------------------------------------------------------------------------
//! What the map draws for one place and one player.
enum LF_EMapPoiState
{
	//! Not drawn.
	HIDDEN,
	//! Heard of: a faded icon with a pencil "?".
	FADED,
	//! The full ink icon.
	FULL,
	//! A personal pencil mark (a HIDDEN place the player met).
	PENCIL
}

//------------------------------------------------------------------------------------------------
//! The POI visibility rule as a pure function (card M2). Its node twin is
//! tools/map/rules/visibility.mjs; both run Configs/Map/Rules/visibility.cases.json
//! (LF_Test_MapPois), so they cannot drift.
class LF_MapPoiRules
{
	//------------------------------------------------------------------------------------------------
	//! The state one place shows in for one player.
	//! \param[in] visibility The POI row's visibility.
	//! \param[in] role The row's role, for ROLE rows.
	//! \param[in] live Whether the place is registered in LF_Places now.
	//! \param[in] known What the player knows of the place.
	//! \param[in] roles The roles the player holds now; may be null.
	static LF_EMapPoiState Resolve(LF_EMapPoiVisibility visibility, string role, bool live, LF_EMapPoiKnown known, array<string> roles)
	{
		if (!live)
			return LF_EMapPoiState.HIDDEN;

		if (visibility == LF_EMapPoiVisibility.PUBLIC)
			return LF_EMapPoiState.FULL;

		if (visibility == LF_EMapPoiVisibility.DISCOVER)
		{
			if (known == LF_EMapPoiKnown.VISITED)
				return LF_EMapPoiState.FULL;

			if (known == LF_EMapPoiKnown.HEARD)
				return LF_EMapPoiState.FADED;

			return LF_EMapPoiState.HIDDEN;
		}

		if (visibility == LF_EMapPoiVisibility.ROLE)
		{
			if (!role.IsEmpty() && roles && roles.Find(role) != -1)
				return LF_EMapPoiState.FULL;

			return LF_EMapPoiState.HIDDEN;
		}

		if (visibility == LF_EMapPoiVisibility.HIDDEN)
		{
			if (known == LF_EMapPoiKnown.HEARD || known == LF_EMapPoiKnown.VISITED)
				return LF_EMapPoiState.PENCIL;
		}

		return LF_EMapPoiState.HIDDEN;
	}

	//------------------------------------------------------------------------------------------------
	//! Case-table spelling of a visibility. An unknown word is NONE-safe: ROLE with no role never shows.
	static LF_EMapPoiVisibility ParseVisibility(string word)
	{
		if (word == "PUBLIC")
			return LF_EMapPoiVisibility.PUBLIC;

		if (word == "DISCOVER")
			return LF_EMapPoiVisibility.DISCOVER;

		if (word == "ROLE")
			return LF_EMapPoiVisibility.ROLE;

		return LF_EMapPoiVisibility.HIDDEN;
	}

	//------------------------------------------------------------------------------------------------
	//! Case-table spelling of what a player knows: none, heard, visited.
	static LF_EMapPoiKnown ParseKnown(string word)
	{
		if (word == "heard")
			return LF_EMapPoiKnown.HEARD;

		if (word == "visited")
			return LF_EMapPoiKnown.VISITED;

		return LF_EMapPoiKnown.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Case-table spelling of a state: hidden, faded, full, pencil (the words the emulator draws with).
	static string StateName(LF_EMapPoiState state)
	{
		if (state == LF_EMapPoiState.FADED)
			return "faded";

		if (state == LF_EMapPoiState.FULL)
			return "full";

		if (state == LF_EMapPoiState.PENCIL)
			return "pencil";

		return "hidden";
	}
}
