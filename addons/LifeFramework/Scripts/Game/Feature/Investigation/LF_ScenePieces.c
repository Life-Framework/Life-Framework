//------------------------------------------------------------------------------------------------
//! The physical evidence a crime leaves besides casings (#148), and where every piece lies. Server
//! only. A robber on foot leaves a footprint card where they stood (LF_EvidenceConfig.LeavesFootprint);
//! a witness who tells an officer what they saw leaves a written statement on the ground beside them.
//! Every piece is one LF_Evidence row, so the evidence cap and the expiry hold for it, and every one
//! lies on the surface under it: GroundSpot asks the placement probe (LF_PlacementAudit.SupportAt)
//! for the floor, so nothing hangs in the air or sinks into a kerb. The pieces are not inventory
//! items (Prefabs/Investigation/LF_EvidencePiece_Base.et): no one picks them up but an on-duty
//! officer, with "Bag as evidence" (LF_BagEvidenceAction).
//!
//! A piece carries a note for its card on the case board (LF_CaseBoard): the footwear key of a
//! footprint, "clothing~vehicle~plate" of a statement, an item's name key. Keys, never words: the
//! reading client translates them.
class LF_ScenePieces
{
	static const string FEATURE = "Evidence";
	//! The probe starts this far above the point, so the floor a body stands on is found from inside it.
	static const float PROBE_LIFT = 0.5;	// lf-fixed: placement probe start, not tuning
	//! Mesh base below the pivot, so each piece rests on the surface (tools\cli place bounds).
	static const float CASING_BASE = 0.005;	// lf-fixed: Case_556x45.xob lying on its side
	static const float CARD_BASE = 0.006;	// lf-fixed: IdCard.xob turned flat (CARD_PITCH)
	static const float PAPER_BASE = 0.001;	// lf-fixed: Papers_Personal.xob, base at the pivot
	//! The pitch that lays IdCard.xob (modelled upright, 0.109 m tall) flat on its back.
	static const float CARD_PITCH = -90;	// lf-fixed: the mesh orientation
	//! A dropped item's card lies beside it, not under it.
	static const vector MARKER_OFFSET = "0.25 0 0.15";
	//! Casings land on a ring around the shooter: the first at SCATTER_MIN, later ones a step farther.
	static const float SCATTER_MIN = 0.45;	// lf-fixed: an ejected case lands about half a metre away
	static const float SCATTER_STEP = 0.12;	// lf-fixed: keeps a burst's cases apart
	//! The footprint lies this far behind where the suspect stood when the report was filed.
	static const float FOOTPRINT_BACK = 0.6;	// lf-fixed: one stride
	//! The statement lies this far in front of the witness.
	static const float STATEMENT_AHEAD = 0.5;	// lf-fixed: at the witness's feet
	static const string NOTE_SEPARATOR = "~";

	//------------------------------------------------------------------------------------------------
	//! Where a piece of `base` (its mesh bottom below the pivot) rests under `at`: on the highest
	//! surface the placement probe finds between PROBE_LIFT above it and 5 m below, never on a body.
	static vector GroundSpot(vector at, float base)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return at;

		string support;
		float y = LF_PlacementAudit.SupportAt(null, at[0], at[2], at[1] + PROBE_LIFT, support);
		return Vector(at[0], y + base, at[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! The `index`-th casing's spot around the shooter: the golden angle spreads them round the ring.
	static vector Scatter(vector origin, int index)
	{
		float angle = index * 137.5 * Math.DEG2RAD;
		int ring = index % 3;
		float radius = SCATTER_MIN + ring * SCATTER_STEP;
		return origin + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
	}

	//------------------------------------------------------------------------------------------------
	static float ScatterYaw(int index)
	{
		int yaw = (index * 73) % 360;
		return yaw;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a crime report was filed (LF_WantedReports.Report). A robber on foot leaves one
	//! footprint card behind where they stood.
	//! \return The card, or null (not that offence, in a vehicle, already one for this report).
	static IEntity OnReport(LF_WantedReportRecord report, IEntity suspect, int nowMs)
	{
		if (!Replication.IsServer() || !report || !suspect)
			return null;

		if (!LF_EvidenceConfig.LeavesFootprint(report.m_sOffence))
			return null;

		if (!report.m_sVehicle.IsEmpty())
		{
			LF_Debug.Log(FEATURE, string.Format("report #%1: no footprint, the suspect sat in a vehicle", report.m_iSequence));
			return null;
		}

		if (LF_Evidence.CountFor(report.m_iSequence, LF_Evidence.KIND_FOOTPRINT) > 0)
			return null;

		vector forward = suspect.GetTransformAxis(2);
		forward[1] = 0;
		forward.Normalize();
		vector at = suspect.GetOrigin() - forward * FOOTPRINT_BACK;
		float yaw = suspect.GetYawPitchRoll()[0];
		return LF_Evidence.LeavePiece(LF_Evidence.FOOTPRINT_PREFAB, GroundSpot(at, CARD_BASE), Vector(yaw, CARD_PITCH, 0), report.m_iSequence, report.m_sOffence, report.m_sPlace, LF_Evidence.KIND_FOOTPRINT, NoteFootwear(suspect), nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a witness told an officer what they saw (LF_WitnessAskAction, LF_WitnessRole). The
	//! statement lies at the witness's feet for the officer to bag. One paper per statement: asking
	//! again adds nothing new, and LF_Witness.Ask then says nothing.
	//! \return The paper, or null.
	static IEntity LeaveStatement(LF_WitnessScene scene, LF_WitnessStatement statement, IEntity witness, int nowMs)
	{
		if (!Replication.IsServer() || !scene || !statement || !witness)
			return null;

		vector forward = witness.GetTransformAxis(2);
		forward[1] = 0;
		forward.Normalize();
		vector at = witness.GetOrigin() + forward * STATEMENT_AHEAD;
		float yaw = witness.GetYawPitchRoll()[0];
		string note = NoteStatement(statement.m_sClothing, statement.m_sVehicle, statement.m_sPlateFragment);
		return LF_Evidence.LeavePiece(LF_Evidence.STATEMENT_PREFAB, GroundSpot(at, PAPER_BASE), Vector(yaw, 0, 0), scene.m_iReport, scene.m_sOffence, scene.m_sPlace, LF_Evidence.KIND_STATEMENT, note, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! The display-name key of the suspect's footwear; empty barefoot or unknown.
	static string NoteFootwear(IEntity suspect)
	{
		if (!suspect)
			return string.Empty;

		array<Managed> storages = {};
		suspect.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			for (int slot = 0; slot < storage.GetSlotsCount(); slot++)
			{
				IEntity item = storage.Get(slot);
				if (!item)
					continue;

				BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
				if (!cloth || !LoadoutBootsArea.Cast(cloth.GetAreaType()))
					continue;

				UIInfo info = LF_UIInfoUtils.GetInfo(item);
				if (info)
					return info.GetName();
			}
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! "clothing~vehicle~plate": the fields of a witness statement, keys as LF_WitnessStatement holds them.
	static string NoteStatement(string clothing, string vehicle, string plate)
	{
		return clothing + NOTE_SEPARATOR + vehicle + NOTE_SEPARATOR + plate;
	}

	//------------------------------------------------------------------------------------------------
	//! The display-name key of a dropped item; empty when it has none.
	static string NoteItem(IEntity item)
	{
		UIInfo info = LF_UIInfoUtils.GetInfo(item);
		if (!info)
			return string.Empty;

		return info.GetName();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a card's note in words, by kind. Empty when the piece says nothing more than its kind.
	static string NoteText(string kind, string note)
	{
		if (note.IsEmpty())
			return string.Empty;

		if (kind == LF_Evidence.KIND_FOOTPRINT)
			return WidgetManager.Translate("#LF-CaseBoard_FootprintNote", LF_WantedBoard.ResolveKey(note));

		if (kind == LF_Evidence.KIND_STATEMENT)
		{
			array<string> fields = {};
			note.Split(NOTE_SEPARATOR, fields, false);
			while (fields.Count() < 3)
				fields.Insert(string.Empty);

			string text = LF_WantedPosters.Description(fields[0], fields[1]);
			if (!fields[2].IsEmpty())
			{
				if (!text.IsEmpty())
					text += "; ";

				text += WidgetManager.Translate("#LF-Witness_BoardPlate", fields[2]);
			}

			return text;
		}

		return LF_WantedBoard.ResolveKey(note);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends. The pieces live in LF_Evidence.
	static void ResetStatics()
	{
	}
}
