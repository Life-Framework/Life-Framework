//! Hand-built subjects for LOGIC-tier tests.
//!
//! `new` applies no [Attribute] defaults, so every field a test's assertions
//! depend on must be set explicitly. These probes expose protected state
//! directly instead of going through world-bound setters that call
//! Replication.BumpMe().

class EL_TestLevelProbe : EL_PlayerLevelComponent
{
	void SetLevelForTest(int level)
	{
		m_iPlayerLevel = level;
	}

	void SetSkillPointsForTest(int sp)
	{
		m_iSkillPoints = sp;
	}

	void SetExperienceForTest(float xp)
	{
		m_fPlayerExperience = xp;
	}

	void SetTotalSkillPointsEarnedForTest(int total)
	{
		m_iTotalSkillPointsEarned = total;
	}

	void RunCheckLevelUp()
	{
		CheckLevelUp();
	}

	//! CheckLevelUp's tail calls OnLevelUp(), which fires Replication.BumpMe().
	//! A detached test component has no replication scope, so this override keeps
	//! the level-up cascade pure: the math (xp drain, level, SP) is all that is
	//! under test.
	override protected void OnLevelUp()
	{
	}
}