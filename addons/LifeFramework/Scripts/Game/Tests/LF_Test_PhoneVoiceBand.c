// red-proof: Observed 2026-11-12 with `tools/cli test --no-build --only LF_Test_PhoneVoiceBand`: dropping the +1 in LF_PhoneVoice.FrequencyFor failed it with `- call 1 is on the first channel above the idle frequency (expected 100001, got 100000)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/phone-voice-band (#147): a call's frequency is inside the private band, never its idle
//! bottom, and the next call gets the next channel; a call's key changes with its salt; the
//! shipped transport is the line at each telephone.
// tier: LOGIC
class LF_Test_PhoneVoiceBand : LF_Test
{
	override string GetName()
	{
		return "comms/phone-voice-band";
	}

	override void Run(LF_TestContext ctx)
	{
		int channels = LF_PhoneVoice.BAND_MAX - LF_PhoneVoice.BAND_MIN;
		ctx.Equal(LF_PhoneVoice.BAND_MIN + 1, LF_PhoneVoice.FrequencyFor(1), "call 1 is on the first channel above the idle frequency");
		ctx.Equal(LF_PhoneVoice.BAND_MIN + 2, LF_PhoneVoice.FrequencyFor(2), "the next call is on the next channel");
		ctx.Equal(LF_PhoneVoice.BAND_MAX, LF_PhoneVoice.FrequencyFor(channels), "the last channel is the top of the band");
		ctx.Equal(LF_PhoneVoice.BAND_MIN + 1, LF_PhoneVoice.FrequencyFor(channels + 1), "channels wrap after the band is used up");

		bool inBand = true;
		array<int> ids = {0, -5, 7, 40000, 65000, 65001, 130000, 999999};
		foreach (int id : ids)
		{
			int f = LF_PhoneVoice.FrequencyFor(id);
			if (f <= LF_PhoneVoice.BAND_MIN || f > LF_PhoneVoice.BAND_MAX)
				inBand = false;
		}
		ctx.True(inBand, "every call id lands inside the band and above the idle frequency");
		ctx.True(LF_PhoneVoice.BAND_MIN > 68000, "the band sits above every vanilla handheld (32000 to 68000 kHz)");

		ctx.True(LF_PhoneVoice.KeyFor(3, 111111) != LF_PhoneVoice.KeyFor(3, 222222), "a new salt is a new key for the same call");
		ctx.True(LF_PhoneVoice.KeyFor(3, 111111) != LF_PhoneVoice.KeyFor(4, 111111), "two calls never share a key");
		ctx.True(LF_PhoneConfig.VoiceTransport() == LF_EPhoneVoiceTransport.LINE, "the shipped transport is the line at each telephone");
	}
}
