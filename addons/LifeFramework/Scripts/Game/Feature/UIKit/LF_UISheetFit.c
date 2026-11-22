// LF_UISheetFit.c - how tall a kit sheet is for its content (#2462, #2473).
// Pure sizing: no widgets. LF_UIScreen measures its Content and applies the result.

class LF_UISheetFit
{
	//! A list never shrinks below this when a sheet at its cap squeezes it to scroll.
	static const float MIN_LIST_HEIGHT = 120;

	//------------------------------------------------------------------------------------------------
	//! The sheet height for its content. A sheet keeps its skin height while the content fits, grows to
	//! the content when it does not, and stops at the screen cap (the caller then scrolls the list).
	//! A cap below the skin height never shrinks the sheet under its skin.
	//! \param contentHeight Height the sheet needs to show everything, frame margins included.
	//! \param skinHeight The skin's own sheet height.
	//! \param screenCap The tallest the sheet may be on this screen; 0 or less means no cap.
	static float Height(float contentHeight, float skinHeight, float screenCap)
	{
		float height = skinHeight;
		if (contentHeight > height)
			height = contentHeight;

		if (screenCap > 0 && height > screenCap)
			height = screenCap;

		// A tight cap never takes the sheet below its skin.
		if (height < skinHeight)
			height = skinHeight;

		return height;
	}

	//------------------------------------------------------------------------------------------------
	//! The list height once the sheet is at its cap and `overflow` units of content still do not fit.
	static float ListHeight(float listHeight, float overflow)
	{
		if (overflow <= 0)
			return listHeight;

		float height = listHeight - overflow;
		if (height < MIN_LIST_HEIGHT)
			height = MIN_LIST_HEIGHT;

		return height;
	}

	//------------------------------------------------------------------------------------------------
	//! A wide sheet's width: `share` of the screen, never narrower than `floor`, never wider than the
	//! screen less a margin each side.
	static float SheetWidth(float screenWidth, float share, float floor)
	{
		float width = screenWidth * share;
		if (width < floor)
			width = floor;

		float most = screenWidth - 2 * LF_UIScreen.SHEET_EDGE_MARGIN;
		if (most > 0 && width > most)
			width = most;

		return width;
	}

	//------------------------------------------------------------------------------------------------
	//! Column `index`'s width when columns with these weights share `contentWidth`; 0 for a bad index.
	static float ColumnWidth(float contentWidth, notnull array<float> weights, int index)
	{
		if (index < 0 || index >= weights.Count())
			return 0;

		float total;
		foreach (float weight : weights)
		{
			total += weight;
		}

		if (total <= 0)
			return 0;

		return contentWidth * weights[index] / total;
	}
}
