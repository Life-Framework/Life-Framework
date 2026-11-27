// red-proof: with `RedProof()` on the parser is not asked, so every key parses to the wrong default (refused, nothing set)
// and the prefab, find-text, type, mode and refusal checks fail.

//! shop/rule-key: the key a counter's price rule is saved and addressed by (LF_ShopPriceResolver.RuleKey and its reverse
//! ParseRuleKey). A prefab rule is `prefab:<GUID>` and comes back as a `{GUID}` resource name, a type rule is
//! `type:<flags>[,mode:<mode>]` with the mode defaulting, any other text is a find-in-path rule, and an empty key, an empty
//! prefab id, a non-numeric number, an unknown part or a third part is refused rather than guessed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShopRuleParse : LF_Test
{
	override string GetName()
	{
		return "shop/rule-parse";
	}

	override void Run(LF_TestContext ctx)
	{
		bool prefabOk = false;
		string prefab = "-";
		string prefabFind = "-";
		bool shortOk = false;
		string shortPrefab = "-";
		bool emptyPrefabOk = true;
		bool findOk = false;
		string findPrefab = "-";
		string findText = "-";
		bool typeOk = false;
		int typeFlags = -1;
		int typeMode = -1;
		bool typeOnlyOk = false;
		int typeOnlyFlags = -1;
		int typeOnlyMode = -1;
		bool emptyOk = true;
		bool letterOk = true;
		bool negativeOk = true;
		bool unknownPartOk = true;
		bool thirdPartOk = true;
		bool modeFirstOk = false;
		string modeFirstPrefab = "-";
		string modeFirstFind = "-";
		string keyPrefab = "-";
		string keyFind = "-";
		string keyType = "-";
		if (!RedProof())
		{
			string p;
			string f;
			int t;
			int m;
			prefabOk = LF_ShopPriceResolver.ParseRuleKey("prefab:C9D661E5B0714711", prefab, prefabFind, t, m);
			shortOk = LF_ShopPriceResolver.ParseRuleKey("prefab:C9D6", shortPrefab, f, t, m);
			emptyPrefabOk = LF_ShopPriceResolver.ParseRuleKey("prefab:", p, f, t, m);
			findOk = LF_ShopPriceResolver.ParseRuleKey("Prefabs/Items/Food/", findPrefab, findText, t, m);
			typeOk = LF_ShopPriceResolver.ParseRuleKey("type:64,mode:2", p, f, typeFlags, typeMode);
			typeOnlyOk = LF_ShopPriceResolver.ParseRuleKey("type:64", p, f, typeOnlyFlags, typeOnlyMode);
			emptyOk = LF_ShopPriceResolver.ParseRuleKey("", p, f, t, m);
			letterOk = LF_ShopPriceResolver.ParseRuleKey("type:abc", p, f, t, m);
			negativeOk = LF_ShopPriceResolver.ParseRuleKey("type:-4", p, f, t, m);
			unknownPartOk = LF_ShopPriceResolver.ParseRuleKey("type:64,colour:2", p, f, t, m);
			thirdPartOk = LF_ShopPriceResolver.ParseRuleKey("type:64,mode:2,extra:1", p, f, t, m);
			modeFirstOk = LF_ShopPriceResolver.ParseRuleKey("mode:2,type:64", modeFirstPrefab, modeFirstFind, t, m);

			keyPrefab = LF_ShopPriceResolver.RuleKey("{c9d661e5b0714711}Prefabs/Items/Food/Apple.et", "ignored", 5, 5);
			keyFind = LF_ShopPriceResolver.RuleKey("", "Prefabs/Items/Food/", 5, 5);
			keyType = LF_ShopPriceResolver.RuleKey("", "", 64, 2);
		}

		ctx.True(prefabOk, "a prefab key parses");
		ctx.EqualStr("{C9D661E5B0714711}", prefab, "a 16 digit id comes back as a {GUID} resource name");
		ctx.EqualStr("", prefabFind, "a prefab rule sets no find text");
		ctx.True(shortOk, "a short id is still a prefab rule");
		ctx.EqualStr("C9D6", shortPrefab, "an id that is not 16 long is kept as written, not braced");
		ctx.False(emptyPrefabOk, "`prefab:` with no id is refused");
		ctx.True(findOk, "plain text is a find rule");
		ctx.EqualStr("", findPrefab, "a find rule sets no prefab");
		ctx.EqualStr("Prefabs/Items/Food/", findText, "the find text is the key");
		ctx.True(typeOk, "a type key with a mode parses");
		ctx.Equal(64, typeFlags, "the type flags are read");
		ctx.Equal(2, typeMode, "the mode is read");
		ctx.True(typeOnlyOk, "a type key with no mode parses");
		ctx.Equal(64, typeOnlyFlags, "the flags are read without a mode");
		ctx.Equal(SCR_EArsenalItemMode.DEFAULT, typeOnlyMode, "the mode defaults when the key names none");
		ctx.False(emptyOk, "an empty key is refused");
		ctx.False(letterOk, "a type key whose number is not digits is refused");
		ctx.False(negativeOk, "a negative type number is refused (digits only)");
		ctx.False(unknownPartOk, "an unknown second part is refused");
		ctx.False(thirdPartOk, "a third part is refused");
		ctx.True(modeFirstOk, "a key that does not start with type: is plain find text, even when it looks like parts");
		ctx.EqualStr("mode:2,type:64", modeFirstFind, "the whole key is the find text (the mode cannot come first)");
		ctx.EqualStr("", modeFirstPrefab, "find text sets no prefab");

		ctx.EqualStr("prefab:C9D661E5B0714711", keyPrefab, "a prefab rule key is its upper-case GUID, whatever the path or case, and outranks find text and type");
		ctx.EqualStr("Prefabs/Items/Food/", keyFind, "a find rule's key is its text");
		ctx.EqualStr("type:64,mode:2", keyType, "a type rule's key carries flags and mode");
	}
}
