[BaseContainerProps()]
class EL_LicensePlateGeneratorGeneric: EL_LicensePlateGeneratorBase
{
	//! RandomInt's upper bound is exclusive. 26 letters and a number end of 10000
	//! are what make Z and 9999 reachable.
	static const string ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	static const int LETTER_COUNT = 26;
	static const int NUMBER_MIN = 100;
	static const int NUMBER_MAX = 9999;

	//------------------------------------------------------------------------------------------------
	static int LetterRollCount()
	{
		return LETTER_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	static int NumberRollEnd()
	{
		return NUMBER_MAX + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! \param index Zero-based alphabet index. 25 is Z.
	//! \return The letter, or empty when the index is outside the alphabet.
	static string LetterAt(int index)
	{
		if (index < 0 || index >= LETTER_COUNT)
			return "";

		return ALPHABET.Get(index);
	}

	//------------------------------------------------------------------------------------------------
	//! \return "AA BB NNNN", or empty when a letter index or the number is outside the plate range.
	static string ComposePlate(int a, int b, int c, int d, int number)
	{
		string l0 = LetterAt(a);
		string l1 = LetterAt(b);
		string l2 = LetterAt(c);
		string l3 = LetterAt(d);
		if (l0.IsEmpty() || l1.IsEmpty() || l2.IsEmpty() || l3.IsEmpty())
			return "";

		if (number < NUMBER_MIN || number > NUMBER_MAX)
			return "";

		string p1 = l0 + l1;
		string p2 = l2 + l3;
		return string.Format("%1 %2 %3", p1, p2, number.ToString());
	}

	//------------------------------------------------------------------------------------------------
	override string GenerateLicensePlate()
	{
		string plate = ComposePlate(
			Math.RandomInt(0, LetterRollCount()),
			Math.RandomInt(0, LetterRollCount()),
			Math.RandomInt(0, LetterRollCount()),
			Math.RandomInt(0, LetterRollCount()),
			Math.RandomInt(NUMBER_MIN, NumberRollEnd())
		);
		EL_Debug.Log("LicensePlate", string.Format("generated plate %1", plate));
		return plate;
	}
}
