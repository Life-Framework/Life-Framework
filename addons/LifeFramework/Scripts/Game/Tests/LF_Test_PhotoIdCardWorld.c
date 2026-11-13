// red-proof: Observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_PhotoIdCard,LF_Test_PhotoIdCardWorld`:
// made LF_PhotoIdCard.IsValid return true for any known number (no void check); got
// `[LFTEST] FAIL identity/photo-id-card-world`, `- the lost card's number is void`. Restored; green.
// Then with the RepairCarried call in LF_IdCard.Grant commented out (review finding, 2026-11-13):
// `- the stolen card is printed for its holder, not over with the thief (expected 'identity-world-holder', got 'identity-world-thief')`,
// `- and keeps its number (expected 'EV 000002', got 'EV 000005')`. Restored; green.
// tier: WORLD
//! Photo identity card (#902) on a real body: a granted card carries its number and the registry row
//! the photo (the body's look) and the date; a card restored blank is printed again from its number;
//! the clerk refuses a second card while the valid one is carried, refuses without the fee, and
//! otherwise takes the fee, prints a new card with a new number and voids the old one; a spawn
//! hands out no fresh card for a lost one; the clerk's conversation hooks exist.
class LF_Test_PhotoIdCardWorld : LF_Test
{
	protected static const string HOLDER = "identity-world-holder";

	override string GetName()
	{
		return "identity/photo-id-card-world";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		array<ref LF_PhotoIdRow> kept = LF_PhotoIdCard.ExportAll();
		LF_PhotoIdCard.ResetStatics();

		CheckGrant(ctx, body);
		CheckReprint(ctx);
		CheckClerk(ctx, body);
		CheckSpawn(ctx, body);
		// A body with a backpack: room for the stolen card and their own.
		IEntity thief = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (thief)
			CheckStolen(ctx, thief);

		LF_TestCharacterFixture.Delete(thief);

		ctx.True(LF_DialogueHook.Find("IdCardMissing") != null, "the clerk's condition hook exists");
		ctx.True(LF_DialogueHook.Find("IdCardReplace") != null, "the clerk's replacement hook exists");

		LF_TestCharacterFixture.Delete(body);
		LF_PhotoIdCard.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGrant(LF_TestContext ctx, IEntity body)
	{
		ctx.True(LF_IdCard.Grant(body, HOLDER, "Ada", "Novak", 31), "the registered character is handed a card");
		LF_IdCardComponent card = LF_IdCard.FindShown(body, HOLDER);
		ctx.NotNull(card, "the body carries the card");
		if (!card)
			return;

		string number = LF_IdCard.NumberOf(card);
		ctx.EqualStr(LF_PhotoIdCard.FormatNumber(1), number, "the card is numbered from the registry");
		LF_PhotoIdRow row = LF_PhotoIdCard.FindNumber(number);
		ctx.NotNull(row, "the number has its registry row");
		if (row)
		{
			ctx.EqualStr(LF_WantedLook.Capture(body), row.m_sLook, "the photo is the body as it stands at the desk");
			ctx.True(row.m_sLook.Contains(LF_WantedLook.GuidOf(LF_TestCharacterFixture.JACKET_PREFAB)), "the photo wears the jacket");
			ctx.EqualStr(LF_PhotoIdCard.IssueDate(), row.m_sIssued, "the date of issue is today's in-game date");
			ctx.EqualStr("Novak", row.m_sLastName, "the row holds the printed surname");
		}

		ctx.True(LF_IdCard.IsValid(card), "a new card is valid");
		ctx.True(LF_IdCard.CarriesValid(body, HOLDER), "the holder carries their valid card");
		ctx.False(LF_IdCard.Grant(body, HOLDER, "Ada", "Novak", 31), "a second card is refused while the valid one is carried");
		ctx.Equal(1, LF_PhotoIdCard.Count(), "a refused grant records no card");
	}

	//------------------------------------------------------------------------------------------------
	//! A card restored after a restart: the prefab with its number and nothing printed.
	protected void CheckReprint(LF_TestContext ctx)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(LF_IdCard.PREFAB, "0 0 0");
		ctx.NotNull(item, "the card prefab spawns");
		if (!item)
			return;

		LF_IdCardComponent blank = LF_IdCardComponent.Cast(item.FindComponent(LF_IdCardComponent));
		LF_SerialComponent serial = LF_SerialComponent.Cast(item.FindComponent(LF_SerialComponent));
		ctx.True(blank != null && serial != null, "the card prefab carries the print and the number");
		if (blank && serial)
		{
			ctx.False(LF_IdCard.EnsurePrinted(blank), "a card with no number stays blank");
			serial.RestoreSerial("EV 999999");
			ctx.False(LF_IdCard.EnsurePrinted(blank), "a number the registry does not hold stays blank");
			serial.RestoreSerial(LF_PhotoIdCard.FormatNumber(1));
			ctx.True(LF_IdCard.EnsurePrinted(blank), "a restored card is printed from its number");
			ctx.EqualStr(HOLDER, blank.GetHolderId(), "it is printed for the holder of that number");
			ctx.EqualStr("Ada", blank.GetFirstName(), "with the registered name");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckClerk(LF_TestContext ctx, IEntity body)
	{
		LF_TestCashWallet wallet = LF_TestCashWallet.Create(500, 1000);
		ctx.Equal(LF_EIdReplace.STILL_CARRIED, LF_IdCardClerk.ReplaceFor(body, HOLDER, "Ada", "Novak", 31, wallet, 50), "the clerk will not replace a card that is carried");
		ctx.Equal(500, wallet.Carried(), "a refused replacement costs nothing");

		// Lose it: the card goes from the pocket (dropped, stolen).
		LF_IdCardComponent lost = LF_IdCard.FindShown(body, HOLDER);
		string lostNumber = LF_IdCard.NumberOf(lost);
		if (lost)
			SCR_EntityHelper.DeleteEntityAndChildren(lost.GetOwner());

		ctx.False(LF_IdCard.CarriesValid(body, HOLDER), "the holder no longer carries a card");
		LF_TestCashWallet poor = LF_TestCashWallet.Create(49, 1000);
		ctx.Equal(LF_EIdReplace.NOT_ENOUGH_CASH, LF_IdCardClerk.ReplaceFor(body, HOLDER, "Ada", "Novak", 31, poor, 50), "short of the fee: no card");
		ctx.Equal(49, poor.Carried(), "and no cash taken");

		ctx.Equal(LF_EIdReplace.OK, LF_IdCardClerk.ReplaceFor(body, HOLDER, "Ada", "Novak", 31, wallet, 50), "the clerk replaces a lost card");
		ctx.Equal(450, wallet.Carried(), "for the fee");
		LF_IdCardComponent replacement = LF_IdCard.FindShown(body, HOLDER);
		ctx.True(replacement != null && LF_IdCard.IsValid(replacement), "the holder carries a valid replacement");
		ctx.EqualStr(LF_PhotoIdCard.FormatNumber(2), LF_IdCard.NumberOf(replacement), "with a new number");
		ctx.False(LF_PhotoIdCard.IsValid(lostNumber), "the lost card's number is void");
		ctx.Equal(LF_EIdReplace.NEVER_ISSUED, LF_IdCardClerk.ReplaceFor(body, "identity-world-nobody", "No", "Body", 20, wallet, 50), "someone never issued a card is sent to register");
	}

	//------------------------------------------------------------------------------------------------
	//! Someone else's card restored blank in a pocket (it loaded before the registry did) is printed
	//! for its holder before a grant looks at the pocket: a grant never prints over it.
	protected void CheckStolen(LF_TestContext ctx, IEntity thief)
	{
		array<ref LF_PhotoIdRow> rows = LF_PhotoIdCard.ExportAll();
		string stolenNumber = LF_PhotoIdCard.Current(HOLDER).m_sNumber;
		LF_PhotoIdCard.ResetStatics();
		IEntity item = LF_Utils.SpawnEntityPrefab(LF_IdCard.PREFAB, thief.GetOrigin());
		InventoryStorageManagerComponent storage = LF_Component<InventoryStorageManagerComponent>.Find(thief);
		ctx.NotNull(item, "the stolen card spawns");
		ctx.NotNull(storage, "the thief has an inventory");
		if (!item || !storage)
		{
			LF_PhotoIdCard.ApplyAll(rows);
			return;
		}

		bool inserted = storage.TryInsertItem(item);
		ctx.True(inserted, "a stolen card goes into the pocket");

		LF_IdCardComponent stolen = LF_IdCardComponent.Cast(item.FindComponent(LF_IdCardComponent));
		LF_SerialComponent number = LF_SerialComponent.Cast(item.FindComponent(LF_SerialComponent));
		ctx.True(stolen != null && number != null, "the stolen card has its print and its number");
		if (!stolen || !number)
		{
			LF_PhotoIdCard.ApplyAll(rows);
			return;
		}

		number.RestoreSerial(stolenNumber);
		ctx.False(stolen.IsIssued(), "it loaded before the registry: blank, with its number");
		LF_PhotoIdCard.ApplyAll(rows);

		ctx.True(LF_IdCard.Grant(thief, "identity-world-thief", "Tom", "Thief", 40), "the thief is handed their own card");
		ctx.EqualStr(HOLDER, stolen.GetHolderId(), "the stolen card is printed for its holder, not over with the thief");
		ctx.EqualStr(stolenNumber, LF_IdCard.NumberOf(stolen), "and keeps its number");
		ctx.True(LF_IdCard.CarriesValid(thief, "identity-world-thief"), "the thief carries their own card beside it");
	}

	//------------------------------------------------------------------------------------------------
	//! A lost card is not handed out again at the next spawn (the clerk replaces it); the first card
	//! of a resident with no row is.
	protected void CheckSpawn(LF_TestContext ctx, IEntity body)
	{
		LF_IdCardComponent card = LF_IdCard.FindShown(body, HOLDER);
		if (card)
			SCR_EntityHelper.DeleteEntityAndChildren(card.GetOwner());

		LF_PlayerAccount account = LF_PlayerAccount.Create("identity-world-account");
		account.AddCharacter(LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Eva", "Spawn", 28), true);
		string holderId = account.GetActiveCharacter().GetId();

		ctx.Equal(LF_EIdSpawn.FIRST_CARD, LF_IdCard.OnSpawn(body, account, false, true), "a resident who earned a card and has no row gets one at spawn");
		ctx.True(LF_IdCard.CarriesValid(body, holderId), "and carries it");
		LF_IdCardComponent first = LF_IdCard.FindShown(body, holderId);
		if (first)
			SCR_EntityHelper.DeleteEntityAndChildren(first.GetOwner());

		int rows = LF_PhotoIdCard.Count();
		ctx.Equal(LF_EIdSpawn.NOTHING, LF_IdCard.OnSpawn(body, account, false, true), "a lost card is not handed out again at spawn");
		ctx.False(LF_IdCard.CarriesValid(body, holderId), "the body has no card");
		ctx.Equal(rows, LF_PhotoIdCard.Count(), "and no card was recorded");
		ctx.Equal(LF_EIdSpawn.FREE_REPLACEMENT, LF_IdCard.OnSpawn(body, account, true, true), "a mode with no clerk replaces it at spawn");
		ctx.True(LF_IdCard.CarriesValid(body, holderId), "and the body carries the replacement");
	}
}
