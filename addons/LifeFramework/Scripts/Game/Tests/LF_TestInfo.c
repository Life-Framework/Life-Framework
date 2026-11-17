//------------------------------------------------------------------------------------------------
//! A test class carries its own registration as a class attribute (#2610, #2757):
//!
//!   // tier: WORLD
//!   [LF_TestInfo(LF_TestTier.WORLD)]
//!   (the line above the test's class declaration)
//!
//! LF_TestManager finds every LF_Test descendant by reflection (typename.GetInheritedTypes), keeps the
//! ones that have their own LF_TestInfo (attributes are not inherited, so a helper base class is
//! skipped), spawns them and sets tier and pack flag from the attribute. No static initializer, no
//! shared file: adding a test adds one line to that test's file.
//!
//! A pack test (a pack's Scripts/Game/Tests) says `pack: true`: it runs only when named, in the pack
//! leg of `tools\cli test`. The core never names a pack class.
//!
//! The `// tier:` comment stays until the text readers move to the attribute; gen-test-registry.ps1
//! fails when the two disagree.
//------------------------------------------------------------------------------------------------
class LF_TestInfo
{
	LF_TestTier m_eTier;
	bool m_bPack;

	//------------------------------------------------------------------------------------------------
	void LF_TestInfo(LF_TestTier tier, bool pack = false)
	{
		m_eTier = tier;
		m_bPack = pack;
	}
}
