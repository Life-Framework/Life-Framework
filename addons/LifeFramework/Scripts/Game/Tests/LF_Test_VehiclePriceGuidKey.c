// red-proof: 2026-11-27 `tools\cli test --no-build --only LF_Test_VehiclePriceGuidKey` on the unfixed code (`text.Trim();`
// threw its result away) printed `FAIL vehicles/price-guid-key: - blanks around the key are ignored; - a padded key comes
// back clean (expected '54C3CC22DEBD57BE', got 'unset')`; green with `text = text.Trim();`. With `RedProof()` on no key is
// parsed, so every parse reads "not a GUID" with an empty GUID and the accepted shapes fail too.

//! vehicles/price-guid-key: how an owner's vehicle price record key is read as a prefab GUID (LF_VehiclePriceModel.
//! ParseGuidKey). Sixteen hex digits with or without braces, any case, surrounding blanks ignored, come back upper case;
//! anything else (too short or long, a non-hex letter, an empty or brace-only key, a path) is not a GUID, so an owner
//! typo is refused instead of pricing the wrong car.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehiclePriceGuidKey : LF_Test
{
	override string GetName()
	{
		return "vehicles/price-guid-key";
	}

	//------------------------------------------------------------------------------------------------
	protected bool Parse(string key, out string guid)
	{
		guid = "unset";
		return LF_VehiclePriceModel.ParseGuidKey(key, guid);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string bare = "";
		string braced = "";
		string lower = "";
		string padded = "";
		string mixed = "";
		string shortGuid = "";
		string longGuid = "";
		string badLetter = "";
		string emptyKey = "";
		string bracesOnly = "";
		string path = "";
		string halfBrace = "";
		bool okBare = false;
		bool okBraced = false;
		bool okLower = false;
		bool okPadded = false;
		bool okMixed = false;
		bool okShort = true;
		bool okLong = true;
		bool okBadLetter = true;
		bool okEmpty = true;
		bool okBracesOnly = true;
		bool okPath = true;
		bool okHalfBrace = true;
		if (!RedProof())
		{
			okBare = Parse("54C3CC22DEBD57BE", bare);
			okBraced = Parse("{54C3CC22DEBD57BE}", braced);
			okLower = Parse("54c3cc22debd57be", lower);
			okPadded = Parse("  {54C3CC22DEBD57BE}  ", padded);
			okMixed = Parse("{54c3Cc22dEbD57bE}", mixed);
			okShort = Parse("54C3CC22DEBD57B", shortGuid);
			okLong = Parse("54C3CC22DEBD57BE0", longGuid);
			okBadLetter = Parse("54C3CC22DEBD57BG", badLetter);
			okEmpty = Parse("", emptyKey);
			okBracesOnly = Parse("{}", bracesOnly);
			okPath = Parse("Prefabs/Vehicles/Wheeled/S105/S105_beige.et", path);
			okHalfBrace = Parse("{54C3CC22DEBD57BE", halfBrace);
		}

		ctx.True(okBare, "16 hex digits are a GUID");
		ctx.EqualStr("54C3CC22DEBD57BE", bare, "a bare GUID comes back as it is");
		ctx.True(okBraced, "braces around the digits are fine");
		ctx.EqualStr("54C3CC22DEBD57BE", braced, "the braces are dropped");
		ctx.True(okLower, "lower case digits are fine");
		ctx.EqualStr("54C3CC22DEBD57BE", lower, "lower case comes back upper case");
		ctx.True(okPadded, "blanks around the key are ignored");
		ctx.EqualStr("54C3CC22DEBD57BE", padded, "a padded key comes back clean");
		ctx.True(okMixed, "mixed case is fine");
		ctx.EqualStr("54C3CC22DEBD57BE", mixed, "mixed case comes back upper case");
		ctx.False(okShort, "15 digits are not a GUID");
		ctx.False(okLong, "17 digits are not a GUID");
		ctx.False(okBadLetter, "a non-hex letter is not a GUID");
		ctx.False(okEmpty, "an empty key is not a GUID");
		ctx.False(okBracesOnly, "empty braces are not a GUID");
		ctx.False(okPath, "a prefab path is not a GUID key");
		ctx.False(okHalfBrace, "an unclosed brace leaves 17 characters: not a GUID");
		ctx.EqualStr("unset", shortGuid, "a refused key leaves the GUID output untouched");
	}
}
