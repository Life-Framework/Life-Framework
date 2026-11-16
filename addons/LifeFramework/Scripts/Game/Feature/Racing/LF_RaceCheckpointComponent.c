//------------------------------------------------------------------------------------------------
//! A placed race checkpoint (#1923). The pack puts it on a marker; it registers in LF_Places under
//! its id, and the server race runner asks whether a racer is inside its radius. A course names
//! checkpoints by id only, so core script holds no coordinates.
[ComponentEditorProps(category: "LifeFramework/Feature/Racing", description: "Race checkpoint: a course passes it by id.")]
class LF_RaceCheckpointComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_RaceCheckpointComponent : ScriptComponent
{
	static const string PLACE_KIND = "race_checkpoint";

	[Attribute("", UIWidgets.EditBox, desc: "Checkpoint id a course lists (harbour_sprint_1). Unique in the world")]
	protected string m_sCheckpointId;

	[Attribute("12", UIWidgets.EditBox, desc: "Metres from the marker that count as passing it")]
	protected float m_fRadius;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sCheckpointId.IsEmpty())
		{
			LF_Debug.Error("Racing", string.Format("race checkpoint on %1 has no id; no course can pass it", owner.GetName()));
			return;
		}

		LF_Places.Register(PLACE_KIND, m_sCheckpointId, owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetCheckpointId()
	{
		return m_sCheckpointId;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the path from one world position to the next came within the placed checkpoint's
	//! radius, so a car that crosses between two detection ticks still counts. A missing
	//! checkpoint is never passed.
	static bool Crossed(string checkpointId, vector from, vector to)
	{
		IEntity marker = LF_Places.Find(PLACE_KIND, checkpointId);
		if (!marker)
			return false;

		LF_RaceCheckpointComponent checkpoint = LF_RaceCheckpointComponent.Cast(marker.FindComponent(LF_RaceCheckpointComponent));
		vector centre;
		if (!checkpoint || !LF_Places.FindOrigin(PLACE_KIND, checkpointId, centre))
			return false;

		return PathWithin(centre, checkpoint.GetRadius(), from, to);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the straight path from one point to another comes within radius of centre.
	static bool PathWithin(vector centre, float radius, vector from, vector to)
	{
		vector path = to - from;
		float lengthSq = path.LengthSq();
		vector closest = to;
		if (lengthSq > 0.0001)
		{
			float t = vector.Dot(centre - from, path) / lengthSq;
			if (t < 0)
				t = 0;
			else if (t > 1)
				t = 1;

			closest = from + path * t;
		}

		return vector.DistanceSq(closest, centre) <= radius * radius;
	}
}
