// red-proof: with `RedProof()` on the rule is not asked, so every answer keeps its wrong default and the "same person",
// "unknown key" and "another person" checks fail.

//! interaction/known-names-same-key: when a learned name still names the player (LF_KnownNames.SameKey). A name a reader
//! learned stays valid while the character behind the player id is the same one; when the player switches to another
//! character the old name must not follow them. If either key is unknown (empty) the name is kept, so a late-loading
//! account never wipes what the reader learned. Keys compare exactly (case counts).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_KnownNamesSameKey : LF_Test
{
	override string GetName()
	{
		return "interaction/known-names-same-key";
	}

	override void Run(LF_TestContext ctx)
	{
		bool same = false;
		bool changed = true;
		bool learnedUnknown = false;
		bool currentUnknown = false;
		bool bothUnknown = false;
		bool otherCase = true;
		bool symmetric = false;
		if (!RedProof())
		{
			same = LF_KnownNames.SameKey("char-1", "char-1");
			changed = LF_KnownNames.SameKey("char-1", "char-2");
			learnedUnknown = LF_KnownNames.SameKey("", "char-2");
			currentUnknown = LF_KnownNames.SameKey("char-1", "");
			bothUnknown = LF_KnownNames.SameKey("", "");
			otherCase = LF_KnownNames.SameKey("char-1", "CHAR-1");
			symmetric = !LF_KnownNames.SameKey("char-2", "char-1");
		}

		ctx.True(same, "the same character keeps its learned name");
		ctx.False(changed, "a switch to another character drops the old name");
		ctx.True(learnedUnknown, "a name learned before the key was known is kept");
		ctx.True(currentUnknown, "an unknown current key never wipes a learned name");
		ctx.True(bothUnknown, "two unknown keys keep the name");
		ctx.False(otherCase, "keys match exactly: a different case is another character");
		ctx.True(symmetric, "the check does not depend on which key is which");
	}
}
