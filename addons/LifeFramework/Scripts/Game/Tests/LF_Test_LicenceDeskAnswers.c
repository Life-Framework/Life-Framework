// red-proof: with `RedProof()` on the answer rules are not asked, so every key stays "?" and every argument "?", which fails
// the key, counter-override, hours, days, fee and "no argument" checks.

//! license/desk-answers: what the licence desk tells a player after a visit (LF_LicenceDesk.AnswerKey, CounterAnswerKey,
//! DrivingAnswerKey, AnswerArg and DrivingAnswerArg). Each outcome maps to its own string key (an outcome with no row reads
//! as "closed", never as a success), the shop counter overrides only the four outcomes it words differently and falls back
//! to the desk's wording for the rest, the permit wait is shown in whole hours rounded UP (89 minutes is 2 hours, never "1"),
//! a ban is shown in whole days rounded up, a refusal for cash carries the fee as money, and a success carries no argument.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LicenceDeskAnswers : LF_Test
{
	override string GetName()
	{
		return "license/desk-answers";
	}

	override void Run(LF_TestContext ctx)
	{
		string issued = "?";
		string applied = "?";
		string waiting = "?";
		string alreadyHeld = "?";
		string wanted = "?";
		string openCrime = "?";
		string convicted = "?";
		string noCash = "?";
		string notAllowed = "?";
		string banned = "?";
		string failed = "?";
		string noClock = "?";
		string counterIssued = "?";
		string counterApplied = "?";
		string counterFailed = "?";
		string counterNoClock = "?";
		string counterWaiting = "?";
		string counterNoCash = "?";
		string drivingIssued = "?";
		string drivingHeld = "?";
		string drivingNoCash = "?";
		string drivingNotAllowed = "?";
		string drivingBanned = "?";
		string drivingWanted = "?";
		string hoursExact = "?";
		string hoursRoundedUp = "?";
		string hoursOneMinute = "?";
		string hoursWaiting = "?";
		string daysOne = "?";
		string daysRoundedUp = "?";
		string cashArg = "?";
		string issuedArg = "?";
		string wantedArg = "?";
		string drivingCashArg = "?";
		string drivingIssuedArg = "?";
		if (!RedProof())
		{
			issued = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.ISSUED);
			applied = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.APPLIED);
			waiting = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.WAITING);
			alreadyHeld = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.ALREADY_HELD);
			wanted = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.WANTED);
			openCrime = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.OPEN_CRIME);
			convicted = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.CONVICTED);
			noCash = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.NOT_ENOUGH_CASH);
			notAllowed = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.NOT_ALLOWED);
			banned = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.BANNED);
			failed = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.FAILED);
			noClock = LF_LicenceDesk.AnswerKey(LF_ELicenceDesk.NO_CLOCK);

			counterIssued = LF_LicenceDesk.CounterAnswerKey(LF_ELicenceDesk.ISSUED);
			counterApplied = LF_LicenceDesk.CounterAnswerKey(LF_ELicenceDesk.APPLIED);
			counterFailed = LF_LicenceDesk.CounterAnswerKey(LF_ELicenceDesk.FAILED);
			counterNoClock = LF_LicenceDesk.CounterAnswerKey(LF_ELicenceDesk.NO_CLOCK);
			counterWaiting = LF_LicenceDesk.CounterAnswerKey(LF_ELicenceDesk.WAITING);
			counterNoCash = LF_LicenceDesk.CounterAnswerKey(LF_ELicenceDesk.NOT_ENOUGH_CASH);

			drivingIssued = LF_LicenceDesk.DrivingAnswerKey(LF_ELicenceDesk.ISSUED);
			drivingHeld = LF_LicenceDesk.DrivingAnswerKey(LF_ELicenceDesk.ALREADY_HELD);
			drivingNoCash = LF_LicenceDesk.DrivingAnswerKey(LF_ELicenceDesk.NOT_ENOUGH_CASH);
			drivingNotAllowed = LF_LicenceDesk.DrivingAnswerKey(LF_ELicenceDesk.NOT_ALLOWED);
			drivingBanned = LF_LicenceDesk.DrivingAnswerKey(LF_ELicenceDesk.BANNED);
			drivingWanted = LF_LicenceDesk.DrivingAnswerKey(LF_ELicenceDesk.WANTED);

			hoursExact = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.APPLIED, 120);
			hoursRoundedUp = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.APPLIED, 89);
			hoursOneMinute = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.APPLIED, 1);
			hoursWaiting = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.WAITING, 61);
			daysOne = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.BANNED, LF_LicenceDesk.MINUTES_PER_DAY);
			daysRoundedUp = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.BANNED, LF_LicenceDesk.MINUTES_PER_DAY + 1);
			cashArg = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.NOT_ENOUGH_CASH, 0);
			issuedArg = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.ISSUED, 500);
			wantedArg = LF_LicenceDesk.AnswerArg(LF_ELicenceDesk.WANTED, 500);
			drivingCashArg = LF_LicenceDesk.DrivingAnswerArg(LF_ELicenceDesk.NOT_ENOUGH_CASH);
			drivingIssuedArg = LF_LicenceDesk.DrivingAnswerArg(LF_ELicenceDesk.ISSUED);
		}

		ctx.EqualStr("#LF-LicenceDesk_Issued", issued, "issued has its own answer");
		ctx.EqualStr("#LF-LicenceDesk_Applied", applied, "applied has its own answer");
		ctx.EqualStr("#LF-LicenceDesk_Waiting", waiting, "waiting has its own answer");
		ctx.EqualStr("#LF-LicenceDesk_AlreadyHeld", alreadyHeld, "already held has its own answer");
		ctx.EqualStr("#LF-LicenceDesk_Wanted", wanted, "a wanted player is told so");
		ctx.EqualStr("#LF-LicenceDesk_OpenCrime", openCrime, "an open crime is told so");
		ctx.EqualStr("#LF-LicenceDesk_Convicted", convicted, "a conviction is told so");
		ctx.EqualStr("#LF-LicenceDesk_NoCash", noCash, "short of cash is told so");
		ctx.EqualStr("#LF-LicenceDesk_NotAllowed", notAllowed, "not allowed is told so");
		ctx.EqualStr("#LF-LicenceDesk_Banned", banned, "a ban is told so");
		ctx.EqualStr("#LF-LicenceDesk_Closed", failed, "a failed visit reads as closed, never as a success");
		ctx.EqualStr("#LF-LicenceDesk_Closed", noClock, "a desk with no clock reads as closed");

		ctx.EqualStr("#LF-LicenceCounter_Issued", counterIssued, "the shop counter words an issue its own way");
		ctx.EqualStr("#LF-LicenceCounter_Applied", counterApplied, "the shop counter words an application its own way");
		ctx.EqualStr("#LF-LicenceCounter_Closed", counterFailed, "the shop counter closes on a failed visit");
		ctx.EqualStr("#LF-LicenceCounter_Closed", counterNoClock, "the shop counter closes on a missing clock");
		ctx.EqualStr("#LF-LicenceDesk_Waiting", counterWaiting, "an outcome the counter does not reword uses the desk's wording");
		ctx.EqualStr("#LF-LicenceDesk_NoCash", counterNoCash, "short of cash uses the desk's wording at the counter too");

		ctx.EqualStr("#LF-LicenceDesk_DrivingIssued", drivingIssued, "buying the driving licence has its own issued answer");
		ctx.EqualStr("#LF-LicenceDesk_DrivingAlreadyHeld", drivingHeld, "an owned driving licence is told so");
		ctx.EqualStr("#LF-LicenceDesk_NoCash", drivingNoCash, "short of cash is the shared answer");
		ctx.EqualStr("#LF-LicenceDesk_DrivingNotAllowed", drivingNotAllowed, "not allowed has the driving wording");
		ctx.EqualStr("#LF-LicenceDesk_DrivingBanned", drivingBanned, "a driving ban has the driving wording");
		ctx.EqualStr("#LF-LicenceDesk_Closed", drivingWanted, "an outcome the driving desk never gives reads as closed");

		ctx.EqualStr("2", hoursExact, "120 minutes is 2 hours");
		ctx.EqualStr("2", hoursRoundedUp, "89 minutes shows as 2 hours (rounded up, never an early collection)");
		ctx.EqualStr("1", hoursOneMinute, "one minute left still shows 1 hour");
		ctx.EqualStr("2", hoursWaiting, "a waiting permit shows its hours the same way");
		ctx.EqualStr("1", daysOne, "a ban of exactly one day shows 1");
		ctx.EqualStr("2", daysRoundedUp, "a minute over a day shows 2 days");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(LF_LicencesConfig.PermitFee()), cashArg, "short of cash for the permit names the permit fee as money");
		ctx.EqualStr("", issuedArg, "an issue carries no argument");
		ctx.EqualStr("", wantedArg, "a refusal for a record carries no argument");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(LF_LicencesConfig.DriverLicenceFee()), drivingCashArg, "short of cash for the driving licence names its own fee as money");
		ctx.EqualStr("", drivingIssuedArg, "a driving licence issue carries no argument");
	}
}
