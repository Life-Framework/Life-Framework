//------------------------------------------------------------------------------------------------
//! A test aid for a placement that is refused for no visible reason (#2762): names what stands within
//! `RADIUS` metres of a spot, with its height, so the failure line says what was in the way.
class LF_TestDebrisProbe
{
	protected static const float RADIUS = 9;

	protected ref array<string> m_aLines = {};

	//------------------------------------------------------------------------------------------------
	//! One line per entity: prefab (or class), origin, bounds height.
	string Describe(vector at)
	{
		m_aLines.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(at, RADIUS, Visit);
		string all;
		foreach (string line : m_aLines)
			all = all + " | " + line;

		return all;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Visit(IEntity entity)
	{
		string name = entity.ClassName();
		EntityPrefabData data = entity.GetPrefabData();
		if (data)
			name = FilePath.StripPath(data.GetPrefabName());

		vector mins;
		vector maxs;
		entity.GetWorldBounds(mins, maxs);
		m_aLines.Insert(string.Format("%1@%2 h=%3", name, entity.GetOrigin(), maxs[1] - mins[1]));
		return true;
	}
}
