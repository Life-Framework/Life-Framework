//------------------------------------------------------------------------------------------------
//! The conversation camera, on the talker's machine only. A scripted camera is spawned locally
//! (never replicated), made the current camera, eased from where the player was looking to ONE shot
//! of the NPC (over the player's shoulder, the NPC from about the chest up), and eased back before
//! the player's own camera is restored. The shot never changes during the talk: no zoom on the NPC's
//! lines and no cut between shots.
//! Same camera prefab and SetCamera hand-over as the film rig (Feature/Film/LF_FilmCameraPath.c).
class LF_DialogueCamera
{
	static const ResourceName CAMERA_PREFAB = "{D6DE32D1C0FCC1C7}Prefabs/Editor/Camera/ManualCameraBase.et";
	static const float EASE_SECONDS = 0.8; // lf-fixed: camera feel, the same for every NPC
	//! The one framing of the whole conversation: the camera stands this far behind, aside of and above
	//! the player's head, looks at the NPC's head, and uses this field of view.
	static const float FOV = 42;
	static const float SHOT_BEHIND = 0.75;
	static const float SHOT_ASIDE = 0.5;
	static const float SHOT_UP = 0.22;
	//! The camera keeps this far off anything between it and its subject.
	static const float COLLISION_MARGIN = 0.85;

	//! The camera never comes closer than this to the centre of the player's head: a sphere that
	//! covers the head, hair and the neck, so no mesh is cut by the near plane.
	static const float HEAD_KEEP_OUT = 0.85;

	protected static bool s_bProofCloseUp;
	protected static float s_fKeepFrom;
	protected static EntityID s_CameraId = EntityID.INVALID;
	protected static EntityID s_PreviousId = EntityID.INVALID;
	protected static EntityID s_NpcId = EntityID.INVALID;
	protected static bool s_bClosing;
	protected static bool s_bHudWasVisible;
	protected static int s_iBlendStartMs;
	protected static vector s_vFromPosition;
	protected static vector s_vFromLook;
	protected static float s_fFromFov;
	protected static vector s_vPosition;
	protected static vector s_vLook;
	protected static float s_fFov;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_CameraId = EntityID.INVALID;
		s_PreviousId = EntityID.INVALID;
		s_NpcId = EntityID.INVALID;
		s_bClosing = false;
		s_bProofCloseUp = false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsActive()
	{
		return s_CameraId != EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: smooth start and stop, 0..1 in, 0..1 out.
	static float Ease(float t)
	{
		t = Math.Clamp(t, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: where the camera stands and looks, from the two heads: behind and beside the player's
	//! head, looking past it at the NPC. The only shot of the conversation.
	static void Frame(vector npcHead, vector playerHead, out vector position, out vector lookAt, out float fov)
	{
		vector toPlayer = playerHead - npcHead;
		toPlayer[1] = 0;
		if (toPlayer.LengthSq() < 0.01)
			toPlayer = "0 0 1";

		toPlayer.Normalize();
		vector right = Vector(toPlayer[2], 0, -toPlayer[0]);
		position = playerHead + toPlayer * SHOT_BEHIND + right * SHOT_ASIDE + Vector(0, SHOT_UP, 0);
		lookAt = npcHead;
		fov = FOV;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the side of the player's head the shoulder shot lives on (the same "right" as Frame).
	static vector ShoulderSide(vector npcHead, vector playerHead)
	{
		vector toPlayer = playerHead - npcHead;
		toPlayer[1] = 0;
		if (toPlayer.LengthSq() < 0.01)
			toPlayer = "0 0 1";

		toPlayer.Normalize();
		return Vector(toPlayer[2], 0, -toPlayer[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: keep a camera position out of a sphere around the player's head. A path that would pass
	//! through the head slides round the shoulder side instead (the part of the offset on the far
	//! side of the head is dropped before pushing out, so the slide has no jump).
	static vector KeepOutOfHead(vector position, vector playerHead, vector shoulderSide, float radius)
	{
		vector offset = position - playerHead;
		if (offset.Length() >= radius)
			return position;

		float lateral = vector.Dot(offset, shoulderSide);
		if (lateral < 0)
			offset = offset - shoulderSide * lateral;

		if (offset.Length() < 0.01)
			offset = shoulderSide * 0.01;

		offset.Normalize();
		return playerHead + offset * radius;
	}

	//------------------------------------------------------------------------------------------------
	//! Pull a camera position in when something stands between it and the point it must see.
	protected static vector Unblocked(vector from, vector position, IEntity exclude)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return position;

		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = position;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = exclude;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1)
			return position;

		return from + (position - from) * (fraction * COLLISION_MARGIN);
	}

	//------------------------------------------------------------------------------------------------
	//! The shot, with collision.
	protected static bool Target(out vector position, out vector lookAt, out float fov)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity player = LF_Utils.GetLocalCharacter();
		if (!world || !player)
			return false;

		IEntity npc = world.FindEntityByID(s_NpcId);
		if (!npc)
			return false;

		vector npcHead = LF_DialogueVoice.HeadOf(npc);
		vector playerHead = LF_DialogueVoice.HeadOf(player);
		Frame(npcHead, playerHead, position, lookAt, fov);
		if (s_bProofCloseUp)
		{
			// Harness only (`dialogue closeup 1`): a face close-up so a proof can film the mouth. The
			// conversation itself never uses it.
			vector npcMat[4];
			npc.GetWorldTransform(npcMat);
			vector forward = npcMat[2];
			forward[1] = 0;
			forward.Normalize();
			vector side = Vector(forward[2], 0, -forward[0]);
			position = npcHead + forward * 0.8 + side * 0.25 + "0 0.03 0";
			lookAt = npcHead + "0 0.04 0";
			fov = 24;
			return true;
		}

		position = Unblocked(playerHead, position, player);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Take the camera for a conversation with this NPC. \return False when there is no camera to
	//! take (headless) or the camera prefab is broken; the conversation then runs without it.
	static bool Start(IEntity npc)
	{
		if (IsActive())
			Finish();

		ArmaReforgerScripted game = GetGame();
		CameraManager cameraManager = game.GetCameraManager();
		if (!cameraManager || !npc)
			return false;

		CameraBase previous = cameraManager.CurrentCamera();
		if (!previous)
		{
			LF_Debug.Warn(LF_Dialogue.FEATURE, "camera not taken: the player has no current camera");
			return false;
		}

		vector mat[4];
		previous.GetWorldCameraTransform(mat);
		IEntity spawned = LF_Utils.SpawnEntityPrefab(CAMERA_PREFAB, mat[3], "0 0 0", false);
		CameraBase camera = CameraBase.Cast(spawned);
		if (!camera)
		{
			if (spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(spawned);

			LF_Debug.Error(LF_Dialogue.FEATURE, "camera not taken: the camera prefab did not spawn a CameraBase");
			return false;
		}

		SCR_ManualCamera manual = SCR_ManualCamera.Cast(camera);
		if (manual)
			manual.SetInputEnabled(false);

		s_NpcId = npc.GetID();
		s_PreviousId = previous.GetID();
		s_CameraId = camera.GetID();
		s_bClosing = false;
		s_vPosition = mat[3];
		s_vLook = mat[3] + mat[2];
		s_fFov = previous.GetVerticalFOV();
		BeginBlend();
		Apply(camera);
		cameraManager.SetCamera(camera);

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		s_bHudWasVisible = false;
		if (hud)
		{
			s_bHudWasVisible = hud.IsVisible();
			hud.SetVisible(false);
		}

		game.GetCallqueue().CallLater(Tick, 0, true);
		LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("camera taken: easing from the player's view to the NPC over %1 s", EASE_SECONDS));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Harness only: eased close-up of the NPC's face for a mouth proof, or back to the shot.
	static void SetProofCloseUp(bool on)
	{
		if (!IsActive() || s_bClosing || s_bProofCloseUp == on)
			return;

		s_bProofCloseUp = on;
		BeginBlend();
	}

	//------------------------------------------------------------------------------------------------
	//! Ease back to the player's own view, then hand the camera back.
	static void Stop()
	{
		if (!IsActive() || s_bClosing)
			return;

		s_bClosing = true;
		BeginBlend();
	}

	//------------------------------------------------------------------------------------------------
	protected static void BeginBlend()
	{
		s_vFromPosition = s_vPosition;
		s_vFromLook = s_vLook;
		s_fFromFov = s_fFov;
		s_fKeepFrom = 0;
		IEntity player = LF_Utils.GetLocalCharacter();
		if (player)
		{
			// The keep-out sphere starts as big as the camera's distance from the head (a first person
			// view sits at the head itself, so it starts tiny) and ends at the distance the end of the
			// ease has, so the ease in and the ease out both slide round the head, never through it.
			s_fKeepFrom = Math.Min(vector.Distance(s_vFromPosition, LF_DialogueVoice.HeadOf(player)), HEAD_KEEP_OUT);
		}

		s_iBlendStartMs = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Apply(CameraBase camera)
	{
		vector dir = s_vLook - s_vPosition;
		if (dir.LengthSq() < 0.0001)
			dir = "0 0 1";

		vector mat[4];
		Math3D.DirectionAndUpMatrix(dir.Normalized(), vector.Up, mat);
		mat[3] = s_vPosition;
		camera.SetTransform(mat);
		camera.SetVerticalFOV(s_fFov);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		BaseWorld world = GetGame().GetWorld();
		CameraBase camera;
		if (world)
			camera = CameraBase.Cast(world.FindEntityByID(s_CameraId));

		if (!camera)
		{
			Finish();
			return;
		}

		vector position;
		vector lookAt;
		float fov;
		bool hasTarget;
		if (s_bClosing)
		{
			CameraBase previous = CameraBase.Cast(world.FindEntityByID(s_PreviousId));
			if (previous)
			{
				vector mat[4];
				previous.GetWorldCameraTransform(mat);
				position = mat[3];
				lookAt = mat[3] + mat[2];
				fov = previous.GetVerticalFOV();
				hasTarget = true;
			}
		}
		else
		{
			hasTarget = Target(position, lookAt, fov);
		}

		if (!hasTarget)
		{
			Finish();
			return;
		}

		float t = (System.GetTickCount() - s_iBlendStartMs) / (EASE_SECONDS * 1000);
		float k = Ease(t);
		s_vPosition = vector.Lerp(s_vFromPosition, position, k);
		IEntity player = LF_Utils.GetLocalCharacter();
		IEntity npc = world.FindEntityByID(s_NpcId);
		if (player && npc)
		{
			// The ease is a straight line; keep it from cutting through the head.
			vector playerHead = LF_DialogueVoice.HeadOf(player);
			float keepTo = Math.Min(vector.Distance(position, playerHead), HEAD_KEEP_OUT);
			s_vPosition = KeepOutOfHead(s_vPosition, playerHead, ShoulderSide(LF_DialogueVoice.HeadOf(npc), playerHead), Math.Lerp(s_fKeepFrom, keepTo, k));
		}

		s_vLook = vector.Lerp(s_vFromLook, lookAt, k);
		s_fFov = Math.Lerp(s_fFromFov, fov, k);
		Apply(camera);
		if (s_bClosing && t >= 1)
			Finish();
	}

	//------------------------------------------------------------------------------------------------
	//! Hand the camera back at once and remove ours. Safe to call twice.
	static void Finish()
	{
		ArmaReforgerScripted game = GetGame();
		game.GetCallqueue().Remove(Tick);
		BaseWorld world = game.GetWorld();
		CameraManager cameraManager = game.GetCameraManager();
		if (world && cameraManager && s_PreviousId != EntityID.INVALID)
		{
			CameraBase previous = CameraBase.Cast(world.FindEntityByID(s_PreviousId));
			if (previous)
				cameraManager.SetCamera(previous);
		}

		bool had = s_CameraId != EntityID.INVALID;
		if (world && had)
		{
			IEntity camera = world.FindEntityByID(s_CameraId);
			if (camera)
				SCR_EntityHelper.DeleteEntityAndChildren(camera);
		}

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (had && hud && s_bHudWasVisible)
			hud.SetVisible(true);

		if (had)
			LF_Debug.Info(LF_Dialogue.FEATURE, "camera given back to the player");

		ResetStatics();
	}
}
