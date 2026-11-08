// Ported from thebonbon/EveronLife (MIT), enhancement/Main_World:
// EL_ToolHitZone.c (tool-gated hit, hit particle, shake) and
// EL_DestructionMultiPhaseSingleComponent.c (EL_RandomPrefabSpawnable: ore pieces
// spawned around the rock with a random offset). Used with the author's permission,
// relicensed MIT (docs/decisions.md, 2026-11-08).
// Copyright (c) 2022 EveronLife
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Mining", description: "Ore rock: pickaxe hits break ore off and drop it beside the rock")]
class LF_OreNodeComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! An ore rock you mine by swinging a pickaxe at it. EL_DestructibleResourceHitZone
//! accepts only the tools the rock lists (the pickaxe) and hands every accepted
//! melee hit to OnToolHit on the server. Damage piles up; each `m_fDamagePerBreak`
//! breaks a piece off and the node row's yield drops as real items beside the rock.
//! After the row's m_iMaxGathers pieces the rock crumbles (hidden, no collision)
//! and grows back after the row's cooldown. Clients see the spent state through
//! an RplProp and the hit through an unreliable broadcast (particle, shake, rock
//! break sound). The spent state is not saved: a restart grows every rock back.
class LF_OreNodeComponent : ScriptComponent
{
	static const string FEATURE = "Mining";

	[Attribute("iron", UIWidgets.EditBox, "Node id in Configs/Resources/ResourceChains.conf: ore item, items per piece, pieces before the rock is spent, grow-back cooldown", category: "Ore")]
	protected string m_sNodeId;

	[Attribute("60", UIWidgets.Auto, "Pickaxe damage that breaks one piece off. A pickaxe hit deals 20, so 60 is three swings", category: "Ore")]
	protected float m_fDamagePerBreak;

	[Attribute("300", UIWidgets.Auto, "Grow-back seconds when the node row names no cooldown", category: "Ore")]
	protected float m_fRespawnSeconds;

	[Attribute("{1FAA1339FE4492AF}Prefabs/Particles/OreHit.et", UIWidgets.ResourceNamePicker, "Particle at the hit point on every accepted hit", "et", category: "Feedback")]
	protected ResourceName m_HitParticle;

	[Attribute("{F3279D744368E450}Prefabs/Resources/Mining/GatherVFX.et", UIWidgets.ResourceNamePicker, "Particle when a piece breaks off or the rock crumbles", "et", category: "Feedback")]
	protected ResourceName m_BreakParticle;

	[Attribute("0.02", UIWidgets.Auto, "How far the rock shakes on a hit, in metres", category: "Feedback")]
	protected float m_fShake;

	[RplProp(onRplName: "OnDepletedChanged")]
	protected bool m_bDepleted;

	protected ref LF_OreYieldState m_State = new LF_OreYieldState();
	protected int m_iDropped;
	protected vector m_vRestOrigin;
	protected bool m_bRestOriginKnown;

	//------------------------------------------------------------------------------------------------
	string GetNodeId()
	{
		return m_sNodeId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDepleted()
	{
		return m_bDepleted;
	}

	//------------------------------------------------------------------------------------------------
	//! Items this rock has dropped since the session started.
	int GetDroppedCount()
	{
		return m_iDropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Pieces broken off since the rock last grew back.
	int GetBreaks()
	{
		return m_State.m_iBreaks;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAuthority()
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (!rpl)
			return Replication.IsServer();

		return rpl.IsMaster();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one accepted pickaxe hit. Returns the number of items it dropped.
	//! \param damage Effective damage the hit zone computed for the tool.
	//! \param hitPos World hit point. \param hitDir Swing direction (toward the rock).
	//! \param tool The swung tool, checked against the node row's tool when it names one.
	int OnToolHit(float damage, vector hitPos, vector hitDir, IEntity tool)
	{
		IEntity owner = GetOwner();
		if (!IsAuthority())
			return 0;

		if (m_bDepleted)
		{
			EL_Debug.Info(FEATURE, string.Format("hit on spent %1 rock ignored", m_sNodeId));
			return 0;
		}

		EL_ResourceNodeDef row = EL_ResourceChainCatalog.FindNode(m_sNodeId);
		if (!EL_ResourceChainCatalog.IsNodeUsable(row))
		{
			EL_Debug.Error(FEATURE, string.Format("ore rock %1 refused a hit: node id '%2' has no usable row in ResourceChains.conf", EL_Utils.GetPrefabName(owner), m_sNodeId));
			return 0;
		}

		if (!row.m_ToolPrefab.IsEmpty() && !LF_OreNodeComponent.SameResource(row.m_ToolPrefab, EL_Utils.GetPrefabName(tool)))
		{
			EL_Debug.Info(FEATURE, string.Format("hit on %1 rock refused: %2 is not %3", m_sNodeId, EL_Utils.GetPrefabName(tool), row.m_ToolPrefab));
			return 0;
		}

		if (hitDir.LengthSq() < 0.0001)
			hitDir = owner.GetOrigin() - hitPos;
		if (hitDir.LengthSq() < 0.0001)
			hitDir = vector.Forward;
		hitDir.Normalize();

		int breaks = LF_OreYield.Strike(m_State, damage, m_fDamagePerBreak, row.m_iMaxGathers);
		int items = LF_OreYield.ItemsFor(breaks, row.m_iYield);
		int dropped = 0;
		for (int i = 0; i < items; i++)
		{
			if (DropPiece(row.m_ItemPrefab, hitPos, hitDir))
				dropped++;
		}
		m_iDropped = m_iDropped + dropped;

		EL_Debug.Info(FEATURE, string.Format("%1 rock hit for %2: piece %3/%4 at %5/%6 damage, dropped %7", m_sNodeId, damage, m_State.m_iBreaks, row.m_iMaxGathers, m_State.m_fDamage, m_fDamagePerBreak, dropped));
		if (items > 0 && dropped < items)
			EL_Debug.Error(FEATURE, string.Format("%1 rock broke %2 pieces but only %3 of %4 dropped", m_sNodeId, breaks, dropped, items));

		bool spent = m_State.m_bDepleted;
		Rpc(RpcDo_HitFx, hitPos, hitDir, breaks > 0, spent);
		if (!System.IsConsoleApp())
			PlayHitFx(hitPos, hitDir, breaks > 0, spent);

		if (spent)
		{
			m_bDepleted = true;
			ApplyDepleted();
			Replication.BumpMe();
			float delayMs = LF_OreYield.RespawnDelayMs(row.m_fCooldownMs, m_fRespawnSeconds);
			GetGame().GetCallqueue().Remove(Regrow);
			GetGame().GetCallqueue().CallLater(Regrow, delayMs, false);
			EL_Debug.Info(FEATURE, string.Format("%1 rock spent after %2 pieces; grows back in %3 s", m_sNodeId, m_State.m_iBreaks, delayMs / 1000));
		}

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the rock grows back whole. The grow-back timer calls this; a test may too.
	void Regrow()
	{
		if (!IsAuthority())
			return;

		GetGame().GetCallqueue().Remove(Regrow);
		LF_OreYield.Refill(m_State);
		if (!m_bDepleted)
			return;

		m_bDepleted = false;
		ApplyDepleted();
		Replication.BumpMe();
		EL_Debug.Info(FEATURE, string.Format("%1 rock grew back", m_sNodeId));
	}

	//------------------------------------------------------------------------------------------------
	//! Same prefab, compared by GUID so a path's casing cannot split them.
	static bool SameResource(ResourceName a, ResourceName b)
	{
		if (a.IsEmpty() || b.IsEmpty())
			return false;

		return GuidOf(a) == GuidOf(b);
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}path" -> "GUID" in upper case; a bare path comes back upper-cased whole.
	static string GuidOf(ResourceName resource)
	{
		string text = resource;
		if (text.StartsWith("{"))
		{
			int close = text.IndexOf("}");
			if (close > 1)
				text = text.Substring(1, close - 1);
		}

		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one ore item just outside the rock face, toward the miner, and lets
	//! it fall. A dynamic body gets a small pop; a static one is set on the ground.
	protected bool DropPiece(ResourceName prefab, vector hitPos, vector hitDir)
	{
		vector away = -hitDir;
		away[1] = 0;
		if (away.LengthSq() < 0.0001)
			away = vector.Forward;
		away.Normalize();

		vector side = away * vector.Up;
		vector pos = hitPos + away * Math.RandomFloatInclusive(0.35, 0.6) + side * Math.RandomFloatInclusive(-0.3, 0.3);
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pos[0], pos[2]);
		pos[1] = Math.Max(hitPos[1], ground + 0.15);

		IEntity piece = EL_Utils.SpawnEntityPrefab(prefab, pos, Vector(Math.RandomFloat(0, 360), 0, 0));
		if (!piece)
			return false;

		Physics physics = piece.GetPhysics();
		if (physics && physics.IsDynamic())
		{
			vector velocity = away * Math.RandomFloatInclusive(1.0, 2.0) + vector.Up * Math.RandomFloatInclusive(1.0, 2.0) + side * Math.RandomFloatInclusive(-0.5, 0.5);
			physics.SetVelocity(velocity);
			physics.SetAngularVelocity(Vector(Math.RandomFloat(-6, 6), Math.RandomFloat(-6, 6), Math.RandomFloat(-6, 6)));
			return true;
		}

		vector settled = piece.GetOrigin();
		settled[1] = ground + 0.05;
		piece.SetOrigin(settled);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_HitFx(vector hitPos, vector hitDir, bool broke, bool spent)
	{
		PlayHitFx(hitPos, hitDir, broke, spent);
	}

	//------------------------------------------------------------------------------------------------
	//! Every rendering peer: dust at the hit point and a short shake. A piece breaking
	//! off adds a puff and the vanilla rock-break sound; a spent rock crumbles.
	protected void PlayHitFx(vector hitPos, vector hitDir, bool broke, bool spent)
	{
		IEntity owner = GetOwner();
		if (!m_HitParticle.IsEmpty())
			EL_Utils.SpawnEntityPrefab(m_HitParticle, hitPos, (-hitDir).VectorToAngles(), false);

		if (broke && !m_BreakParticle.IsEmpty())
			EL_Utils.SpawnEntityPrefab(m_BreakParticle, hitPos - hitDir * 0.3, vector.Zero, false);

		if (broke)
		{
			int phaseFromEnd = 1;
			if (spent)
				phaseFromEnd = 0;
			SCR_DestructionUtility.PlaySound(owner, SCR_EMaterialSoundTypeBreak.BREAK_ROCK, 2, 1 - phaseFromEnd);
		}

		if (!spent && m_fShake > 0)
			Shake();
	}

	//------------------------------------------------------------------------------------------------
	//! Bonbon's shake: a few 50 ms nudges left and right, then back to rest.
	protected void Shake()
	{
		IEntity owner = GetOwner();
		if (!m_bRestOriginKnown)
		{
			m_vRestOrigin = owner.GetOrigin();
			m_bRestOriginKnown = true;
		}

		ScriptCallQueue queue = GetGame().GetCallqueue();
		queue.Remove(SetShakeOrigin);
		vector transform[4];
		owner.GetWorldTransform(transform);
		vector right = transform[0];
		for (int i = 0; i < 4; i++)
		{
			float sign = 1;
			if (i % 2 == 1)
				sign = -1;
			queue.CallLater(SetShakeOrigin, i * 50, false, m_vRestOrigin + right * (m_fShake * sign));
		}
		queue.CallLater(SetShakeOrigin, 200, false, m_vRestOrigin);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetShakeOrigin(vector origin)
	{
		IEntity owner = GetOwner();
		if (owner)
			owner.SetOrigin(origin);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the spent flag arrived (also on join).
	protected void OnDepletedChanged()
	{
		ApplyDepleted();
	}

	//------------------------------------------------------------------------------------------------
	//! A spent rock is gone from sight and from collision until it grows back.
	protected void ApplyDepleted()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (m_bDepleted)
		{
			owner.ClearFlags(EntityFlags.VISIBLE, true);
			SCR_PhysicsHelper.ChangeSimulationState(owner, SimulationState.NONE, true);
			return;
		}

		owner.SetFlags(EntityFlags.VISIBLE, true);
		SCR_PhysicsHelper.ChangeSimulationState(owner, SimulationState.COLLISION, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(Regrow);
			queue.Remove(SetShakeOrigin);
		}

		super.OnDelete(owner);
	}
}
