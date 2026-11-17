//------------------------------------------------------------------------------------------------
//! The client side of a shared row's area (everon-map.md sections 2 and 3). A row that carries a radius
//! and a style (LF_MapShareRow.m_iAreaRadius, m_iAreaStyle) is drawn as a circle in LF_MapAreas beside its
//! mark: LF_MapShareComponent calls Show when the row arrives and Hide when it is removed, replaced,
//! or cleared for a resend. The area is named after the row's key, so a replaced row replaces its area
//! and one key never holds two. Only the areas this class registered are ever removed.
//! Runs where the client runs: a dedicated server has no client, so nothing registers there.
class LF_MapShareAreas
{
	protected static ref set<string> s_aShown;

	//------------------------------------------------------------------------------------------------
	//! Client: draw the area of a share row, replacing the one the key already had. Radius 0 draws nothing
	//! (and drops a previous area). \return true when an area is now registered for the key.
	static bool Show(string key, int x, int z, int radius, int style, string label)
	{
		Hide(key);
		if (radius == 0)
			return false;

		if (!LF_MapShareRules.AreaValid(radius, style))
		{
			LF_Debug.Warn(LF_MapShares.FEATURE, string.Format("share %1 area not drawn: radius %2 style %3 is not drawable", key, radius, style));
			return false;
		}

		if (!LF_MapAreas.RegisterCircle(key, style, label, Vector(x, 0, z), radius))
			return false;

		Shown().Insert(key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: take a share's area off the map. A key that never had one is a no-op.
	static void Hide(string key)
	{
		if (!s_aShown || !s_aShown.Contains(key))
			return;

		s_aShown.RemoveItem(key);
		LF_MapAreas.Unregister(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: take every share area this client drew off the map.
	static void HideAll()
	{
		if (!s_aShown)
			return;

		array<string> keys = {};
		foreach (string key : s_aShown)
		{
			keys.Insert(key);
		}

		foreach (string key : keys)
		{
			Hide(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsShown(string key)
	{
		return s_aShown && s_aShown.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	protected static set<string> Shown()
	{
		if (!s_aShown)
			s_aShown = new set<string>();

		return s_aShown;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset) and tests. LF_MapAreas clears its own table.
	static void ResetStatics()
	{
		s_aShown = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapShareAreas' statics (registered at script load; Core/LF_StaticReset.c).
class LF_MapShareAreas_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapShareAreas_StaticResetHook());

	override string Owner()
	{
		return "LF_MapShareAreas";
	}

	override void Apply()
	{
		LF_MapShareAreas.ResetStatics();
	}
}
