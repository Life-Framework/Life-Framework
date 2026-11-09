// red-proof: observed 2026-11-09 with `toolsi test --no-build --only LF_Test_Pager`: LF_Pager.Page writing only the first pager failed it with `- the second receiver's pager holds one message (expected 1, got 0)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/pager (#249): a call writes to every paged receiver's pager once; a pager nobody was
//! paged to stays empty; a pager keeps only the last N messages, newest first; only calls with the
//! pager channel page; the pager card shows the messages.
// tier: WORLD
class LF_Test_Pager : LF_Test
{
	static const ResourceName PAGER_PREFAB = "{747E7E1669676AA1}Prefabs/Items/Roleplay/LF_Pager.et";

	override string GetName()
	{
		return "comms/pager";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officerPagerEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		IEntity medicPagerEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		IEntity civilianPagerEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		ctx.True(officerPagerEntity && medicPagerEntity && civilianPagerEntity, "the pager prefab spawns");
		if (!officerPagerEntity || !medicPagerEntity || !civilianPagerEntity)
			return;

		LF_PagerComponent officerPager = LF_PagerComponent.Cast(officerPagerEntity.FindComponent(LF_PagerComponent));
		LF_PagerComponent medicPager = LF_PagerComponent.Cast(medicPagerEntity.FindComponent(LF_PagerComponent));
		LF_PagerComponent civilianPager = LF_PagerComponent.Cast(civilianPagerEntity.FindComponent(LF_PagerComponent));
		ctx.True(officerPager && medicPager && civilianPager, "the pager prefab carries LF_PagerComponent");
		if (officerPager && medicPager && civilianPager)
		{
			array<int> receivers = {};
			receivers.Insert(1);
			receivers.Insert(2);
			LF_OnServiceCall call = LF_OnServiceCall.Create("caller-uid", "police", "Village payphone", "#LF-Payphone_Summary_Crime", "radio,board,pager", "96.5 0 263", receivers);

			ctx.True(LF_Pager.HasPagerChannel(call.m_sChannels), "a call listing the pager channel pages");
			ctx.False(LF_Pager.HasPagerChannel("radio,board"), "a call without the pager channel does not");

			array<LF_PagerComponent> paged = {};
			paged.Insert(officerPager);
			paged.Insert(medicPager);
			ctx.Equal(2, LF_Pager.Page(call, paged, "21:14"), "one call writes to both on-duty receivers' pagers");
			ctx.Equal(1, officerPager.MessageCount(), "the officer's pager holds one message");
			ctx.Equal(1, medicPager.MessageCount(), "the second receiver's pager holds one message");
			ctx.Equal(0, civilianPager.MessageCount(), "a pager that was not paged stays empty");

			array<string> messages = {};
			officerPager.GetMessages(messages);
			ctx.True(messages.Count() == 1 && messages[0].Contains("#LF-Payphone_Summary_Crime") && messages[0].Contains("Village payphone"), "the message names what and where");

			array<ref LF_UIPart> card = LF_Pager.BuildParts(messages);
			ctx.Equal(3, card.Count(), "the pager card is a title, the message and a close button");

			int keep = LF_ServiceCallsConfig.PagerMessages();
			for (int i = 0; i < keep + 3; i++)
			{
				LF_Pager.Page(call, paged, "21:" + (20 + i));
			}

			officerPager.GetMessages(messages);
			ctx.Equal(keep, messages.Count(), "a pager keeps only the last N messages");
			ctx.True(messages[0].StartsWith("21:" + (20 + keep + 2)), "the newest message is first");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(officerPagerEntity);
		SCR_EntityHelper.DeleteEntityAndChildren(medicPagerEntity);
		SCR_EntityHelper.DeleteEntityAndChildren(civilianPagerEntity);
	}
}
