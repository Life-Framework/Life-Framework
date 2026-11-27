// LF_UIRowFit.c - how a status row (label, status, action) shares its width (2026-11-27 UI sweep part 2).
// Pure sizing: no widgets. LF_UIScreen applies the weights.
//
// The status row gave its columns fixed shares (label 1.5, status 1, action 1.5) even when the status was empty, so a handbook
// title read "How to read this handbc" and a map result "Bank, Saint-Philipp" with a quarter of the row held back for nothing.
// A row with no status now hides that column and gives its share to the label; the action column keeps its place.

class LF_UIRowFit
{
	static const float WEIGHT_LABEL = 1.5;
	static const float WEIGHT_STATUS = 1;
	static const float WEIGHT_ACTION = 1.5;

	//! The characters one share of label holds at body size (24 characters filled the default 1.5 and still cut).
	static const float LABEL_CHARS_PER_SHARE = 14; // lf-fixed: a text-fit measure of the row layout, not a gameplay tunable

	//! The most a long label takes; the action column keeps at least 1.
	static const float MAX_LABEL_WEIGHT = 2; // lf-fixed: a text-fit measure of the row layout, not a gameplay tunable

	//------------------------------------------------------------------------------------------------
	//! The fill weights for one row. An empty status gives its share to the label; an empty action keeps its weight
	//! (the button is hidden and frees its width on its own).
	static void Weights(bool hasStatus, out float weightLabel, out float weightStatus, out float weightAction)
	{
		WeightsFor(hasStatus, 0, weightLabel, weightStatus, weightAction);
	}

	//------------------------------------------------------------------------------------------------
	//! As Weights, for a label of known length. A row that keeps its status takes the extra width a long label needs
	//! (#5147: "Carrying without a license" was cut at "licen" on the charge sheet) from the action column, one
	//! share of label per LABEL_CHARS_PER_SHARE characters, up to MAX_LABEL_WEIGHT; the status column keeps its share
	//! so a list stays aligned.
	//! \param labelLength Characters of the label as the player reads it (localized), 0 when not known.
	static void WeightsFor(bool hasStatus, int labelLength, out float weightLabel, out float weightStatus, out float weightAction)
	{
		weightLabel = WEIGHT_LABEL;
		weightStatus = WEIGHT_STATUS;
		weightAction = WEIGHT_ACTION;
		if (!hasStatus)
		{
			weightLabel = WEIGHT_LABEL + WEIGHT_STATUS;
			weightStatus = 0;
			return;
		}

		float needed = labelLength / LABEL_CHARS_PER_SHARE; // lf-float: a share of the row, not a large int
		weightLabel = Math.Clamp(needed, WEIGHT_LABEL, MAX_LABEL_WEIGHT);
		weightAction = WEIGHT_LABEL + WEIGHT_ACTION - weightLabel;
	}
}
