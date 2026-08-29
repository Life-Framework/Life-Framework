class EL_TestContext
{
	protected ref array<string> m_aFailures = {};
	protected int m_iAssertions;

	//------------------------------------------------------------------------------------------------
	void True(bool value, string message)
	{
		m_iAssertions++;
		if (!value)
			Fail(message);
	}

	//------------------------------------------------------------------------------------------------
	void False(bool value, string message)
	{
		True(!value, message);
	}

	//------------------------------------------------------------------------------------------------
	void Equal(int expected, int actual, string message)
	{
		True(expected == actual, string.Format("%1 (expected %2, got %3)", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	void EqualStr(string expected, string actual, string message)
	{
		True(expected == actual, string.Format("%1 (expected '%2', got '%3')", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	void EqualFloat(float expected, float actual, float epsilon, string message)
	{
		m_iAssertions++;
		if (Math.AbsFloat(expected - actual) > epsilon)
			Fail(string.Format("%1 (expected %2, got %3)", message, expected, actual));
	}

	//------------------------------------------------------------------------------------------------
	void NotNull(Managed obj, string message)
	{
		m_iAssertions++;
		if (obj == null)
			Fail(message);
	}

	//------------------------------------------------------------------------------------------------
	void InRange(float value, float lo, float hi, string message)
	{
		m_iAssertions++;
		if (value < lo || value > hi)
			Fail(string.Format("%1 (%2 not in [%3, %4])", message, value, lo, hi));
	}

	//------------------------------------------------------------------------------------------------
	void Pass(string message)
	{
		m_iAssertions++;
	}

	//------------------------------------------------------------------------------------------------
	void Fail(string message)
	{
		m_iAssertions++;
		m_aFailures.Insert(message);
	}

	//------------------------------------------------------------------------------------------------
	int AssertionCount()
	{
		return m_iAssertions;
	}

	//------------------------------------------------------------------------------------------------
	int FailureCount()
	{
		return m_aFailures.Count();
	}

	//------------------------------------------------------------------------------------------------
	array<string> GetFailures()
	{
		array<string> copy = {};
		foreach (string failure : m_aFailures)
			copy.Insert(failure);

		return copy;
	}
}
