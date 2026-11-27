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

	//------------------------------------------------------------------------------------------------
	//! The fill weights for one row. An empty status gives its share to the label; an empty action keeps its weight
	//! (the button is hidden and frees its width on its own).
	static void Weights(bool hasStatus, out float weightLabel, out float weightStatus, out float weightAction)
	{
		weightLabel = WEIGHT_LABEL;
		weightStatus = WEIGHT_STATUS;
		weightAction = WEIGHT_ACTION;
		if (!hasStatus)
		{
			weightLabel = WEIGHT_LABEL + WEIGHT_STATUS;
			weightStatus = 0;
		}
	}
}
