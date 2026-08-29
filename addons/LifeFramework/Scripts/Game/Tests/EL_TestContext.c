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
		return m_aFailures;
	}
}
