//------------------------------------------------------------------------------------------------
//! The serial text of a weapon sold or issued at an outlet (#1765, Refs #1067): "LF-<outlet>-0001".
//! The outlet id sits inside the serial string, so a seized gun traces back to the counter and no
//! record changes shape. An old serial ("LF-0001") has no outlet prefix: it parses to no outlet and
//! still loads and resolves as before. Pure text, no state.
class LF_OutletSerial
{
	static const string PREFIX = "LF-";

	//------------------------------------------------------------------------------------------------
	//! \return The serial for a booking number: "LF-0007" with no outlet, "LF-gun_counter-0007" with one.
	//! An outlet id holding a '-' or a space is not valid in a serial and is left out (logged).
	static string Mint(string outlet, int number)
	{
		string digits = number.ToString(4, true);
		if (outlet.IsEmpty())
			return PREFIX + digits;

		if (outlet.IndexOf("-") >= 0 || outlet.IndexOf(" ") >= 0)
		{
			LF_Debug.Warn("Serials", string.Format("outlet '%1' cannot prefix a serial; issued with no outlet", outlet));
			return PREFIX + digits;
		}

		return PREFIX + outlet + "-" + digits;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The outlet id a serial names, or empty for an old serial with no prefix or an unknown shape.
	static string Outlet(string serial)
	{
		if (serial.IndexOf(PREFIX) != 0)
			return string.Empty;

		string rest = serial.Substring(PREFIX.Length(), serial.Length() - PREFIX.Length());
		int cut = rest.IndexOf("-");
		if (cut <= 0)
			return string.Empty;

		return rest.Substring(0, cut);
	}
}
