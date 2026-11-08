//------------------------------------------------------------------------------------------------
//! What the registry clerk accepts on form R-89. Pure checks, shared by the client form (for its
//! error line) and the server (EL_CharacterCreationRules.RefuseCharacter, which decides). Each
//! Refuse* returns "" to accept or the localization key written on the form.
//!
//! Names are plain ASCII letters with single inner spaces, hyphens, or apostrophes
//! ("Anne-Marie", "O'Neill", "Van Der Berg"). A name starts and ends with a letter. Names are not
//! unique: two players may both be "John Smith", the card's holder id tells them apart.
//------------------------------------------------------------------------------------------------
class LF_RegistrationRules
{
	static const int NAME_MAX_LENGTH = 32;
	static const int MIN_AGE = 18;
	static const int MAX_AGE = 80;

	//------------------------------------------------------------------------------------------------
	//! \param name Already trimmed.
	static string RefuseName(string name)
	{
		if (name.IsEmpty())
			return "#EL-CC_Error_Fields";

		int length = name.Length();
		if (length > NAME_MAX_LENGTH)
			return "#EL-CC_Error_NameLength";

		if (!IsLetter(name.Substring(0, 1)) || !IsLetter(name.Substring(length - 1, 1)))
			return "#LF-Reg_Error_NameChars";

		bool previousWasMark = false;
		for (int i = 0; i < length; i++)
		{
			string ch = name.Substring(i, 1);
			if (IsLetter(ch))
			{
				previousWasMark = false;
				continue;
			}

			if (!IsMark(ch) || previousWasMark)
				return "#LF-Reg_Error_NameChars";

			previousWasMark = true;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The age written in the box, or -1 when it is not one to three digits.
	static int ParseAge(string text)
	{
		string trimmed = text.Trim();
		int length = trimmed.Length();
		if (length < 1 || length > 3)
			return -1;

		for (int i = 0; i < length; i++)
		{
			string ch = trimmed.Substring(i, 1);
			if (ch < "0" || ch > "9")
				return -1;
		}

		return trimmed.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	static string RefuseAge(int age)
	{
		if (age < MIN_AGE || age > MAX_AGE)
			return "#EL-CC_Error_Age";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The whole form as typed. Same order the server checks in.
	static string RefuseForm(string firstName, string lastName, string ageText)
	{
		string first = firstName.Trim();
		string last = lastName.Trim();
		if (first.IsEmpty() || last.IsEmpty() || ageText.Trim().IsEmpty())
			return "#EL-CC_Error_Fields";

		string refusal = RefuseName(first);
		if (!refusal.IsEmpty())
			return refusal;

		refusal = RefuseName(last);
		if (!refusal.IsEmpty())
			return refusal;

		return RefuseAge(ParseAge(ageText));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLetter(string ch)
	{
		return (ch >= "A" && ch <= "Z") || (ch >= "a" && ch <= "z");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsMark(string ch)
	{
		return ch == " " || ch == "-" || ch == "'";
	}
}
