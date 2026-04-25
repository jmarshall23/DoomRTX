/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "../Game_local.h"

/***********************************************************************

	AI Events

***********************************************************************/

const idEventDef AI_FindEnemy("findEnemy", "d", 'e');
const idEventDef AI_FindEnemyAI("findEnemyAI", "d", 'e');
const idEventDef AI_FindEnemyInCombatNodes("findEnemyInCombatNodes", NULL, 'e');
const idEventDef AI_ClosestReachableEnemyOfEntity("closestReachableEnemyOfEntity", "E", 'e');
const idEventDef AI_HeardSound("heardSound", "d", 'e');
const idEventDef AI_SetEnemy("setEnemy", "E");
const idEventDef AI_ClearEnemy("clearEnemy");
const idEventDef AI_MuzzleFlash("muzzleFlash", "s");
const idEventDef AI_CreateMissile("createMissile", "s", 'e');
const idEventDef AI_AttackMissile("attackMissile", "s", 'e');
const idEventDef AI_FireMissileAtTarget("fireMissileAtTarget", "ss", 'e');
const idEventDef AI_LaunchMissile("launchMissile", "vv", 'e');
const idEventDef AI_AttackMelee("attackMelee", "s", 'd');
const idEventDef AI_DirectDamage("directDamage", "es");
const idEventDef AI_RadiusDamageFromJoint("radiusDamageFromJoint", "ss");
const idEventDef AI_BeginAttack("attackBegin", "s");
const idEventDef AI_EndAttack("attackEnd");
const idEventDef AI_MeleeAttackToJoint("meleeAttackToJoint", "ss", 'd');
const idEventDef AI_RandomPath("randomPath", NULL, 'e');
const idEventDef AI_CanBecomeSolid("canBecomeSolid", NULL, 'f');
const idEventDef AI_BecomeSolid("becomeSolid");
const idEventDef AI_BecomeRagdoll("becomeRagdoll", NULL, 'd');
const idEventDef AI_StopRagdoll("stopRagdoll");
const idEventDef AI_SetHealth("setHealth", "f");
const idEventDef AI_GetHealth("getHealth", NULL, 'f');
const idEventDef AI_AllowDamage("allowDamage");
const idEventDef AI_IgnoreDamage("ignoreDamage");
const idEventDef AI_GetCurrentYaw("getCurrentYaw", NULL, 'f');
const idEventDef AI_TurnTo("turnTo", "f");
const idEventDef AI_TurnToPos("turnToPos", "v");
const idEventDef AI_TurnToEntity("turnToEntity", "E");
const idEventDef AI_MoveStatus("moveStatus", NULL, 'd');
const idEventDef AI_StopMove("stopMove");
const idEventDef AI_MoveToCover("moveToCover");
const idEventDef AI_MoveToEnemy("moveToEnemy");
const idEventDef AI_MoveToEnemyHeight("moveToEnemyHeight");
const idEventDef AI_MoveOutOfRange("moveOutOfRange", "ef");
const idEventDef AI_MoveToAttackPosition("moveToAttackPosition", "es");
const idEventDef AI_Wander("wander");
const idEventDef AI_MoveToEntity("moveToEntity", "e");
const idEventDef AI_MoveToPosition("moveToPosition", "v");
const idEventDef AI_SlideTo("slideTo", "vf");
const idEventDef AI_FacingIdeal("facingIdeal", NULL, 'd');
const idEventDef AI_FaceEnemy("faceEnemy");
const idEventDef AI_FaceEntity("faceEntity", "E");
const idEventDef AI_GetCombatNode("getCombatNode", NULL, 'e');
const idEventDef AI_EnemyInCombatCone("enemyInCombatCone", "Ed", 'd');
const idEventDef AI_WaitMove("waitMove");
const idEventDef AI_GetJumpVelocity("getJumpVelocity", "vff", 'v');
const idEventDef AI_EntityInAttackCone("entityInAttackCone", "E", 'd');
const idEventDef AI_CanSeeEntity("canSee", "E", 'd');
const idEventDef AI_SetTalkTarget("setTalkTarget", "E");
const idEventDef AI_GetTalkTarget("getTalkTarget", NULL, 'e');
const idEventDef AI_SetTalkState("setTalkState", "d");
const idEventDef AI_EnemyRange("enemyRange", NULL, 'f');
const idEventDef AI_EnemyRange2D("enemyRange2D", NULL, 'f');
const idEventDef AI_GetEnemy("getEnemy", NULL, 'e');
const idEventDef AI_GetEnemyPos("getEnemyPos", NULL, 'v');
const idEventDef AI_GetEnemyEyePos("getEnemyEyePos", NULL, 'v');
const idEventDef AI_PredictEnemyPos("predictEnemyPos", "f", 'v');
const idEventDef AI_CanHitEnemy("canHitEnemy", NULL, 'd');
const idEventDef AI_CanHitEnemyFromAnim("canHitEnemyFromAnim", "s", 'd');
const idEventDef AI_CanHitEnemyFromJoint("canHitEnemyFromJoint", "s", 'd');
const idEventDef AI_EnemyPositionValid("enemyPositionValid", NULL, 'd');
const idEventDef AI_ChargeAttack("chargeAttack", "s");
const idEventDef AI_TestChargeAttack("testChargeAttack", NULL, 'f');
const idEventDef AI_TestMoveToPosition("testMoveToPosition", "v", 'd');
const idEventDef AI_TestAnimMoveTowardEnemy("testAnimMoveTowardEnemy", "s", 'd');
const idEventDef AI_TestAnimMove("testAnimMove", "s", 'd');
const idEventDef AI_TestMeleeAttack("testMeleeAttack", NULL, 'd');
const idEventDef AI_TestAnimAttack("testAnimAttack", "s", 'd');
const idEventDef AI_Shrivel("shrivel", "f");
const idEventDef AI_Burn("burn");
const idEventDef AI_ClearBurn("clearBurn");
const idEventDef AI_PreBurn("preBurn");
const idEventDef AI_SetSmokeVisibility("setSmokeVisibility", "dd");
const idEventDef AI_NumSmokeEmitters("numSmokeEmitters", NULL, 'd');
const idEventDef AI_WaitAction("waitAction", "s");
const idEventDef AI_StopThinking("stopThinking");
const idEventDef AI_GetTurnDelta("getTurnDelta", NULL, 'f');
const idEventDef AI_GetMoveType("getMoveType", NULL, 'd');
const idEventDef AI_SetMoveType("setMoveType", "d");
const idEventDef AI_SaveMove("saveMove");
const idEventDef AI_RestoreMove("restoreMove");
const idEventDef AI_AllowMovement("allowMovement", "f");
const idEventDef AI_JumpFrame("<jumpframe>");
const idEventDef AI_EnableClip("enableClip");
const idEventDef AI_DisableClip("disableClip");
const idEventDef AI_EnableGravity("enableGravity");
const idEventDef AI_DisableGravity("disableGravity");
const idEventDef AI_EnableAFPush("enableAFPush");
const idEventDef AI_DisableAFPush("disableAFPush");
const idEventDef AI_SetFlySpeed("setFlySpeed", "f");
const idEventDef AI_SetFlyOffset("setFlyOffset", "d");
const idEventDef AI_ClearFlyOffset("clearFlyOffset");
const idEventDef AI_GetClosestHiddenTarget("getClosestHiddenTarget", "s", 'e');
const idEventDef AI_GetRandomTarget("getRandomTarget", "s", 'e');
const idEventDef AI_TravelDistanceToPoint("travelDistanceToPoint", "v", 'f');
const idEventDef AI_TravelDistanceToEntity("travelDistanceToEntity", "e", 'f');
const idEventDef AI_TravelDistanceBetweenPoints("travelDistanceBetweenPoints", "vv", 'f');
const idEventDef AI_TravelDistanceBetweenEntities("travelDistanceBetweenEntities", "ee", 'f');
const idEventDef AI_LookAtEntity("lookAt", "Ef");
const idEventDef AI_LookAtEnemy("lookAtEnemy", "f");
const idEventDef AI_SetJointMod("setBoneMod", "d");
const idEventDef AI_ThrowMoveable("throwMoveable");
const idEventDef AI_ThrowAF("throwAF");
const idEventDef AI_RealKill("<kill>");
const idEventDef AI_Kill("kill");
const idEventDef AI_WakeOnFlashlight("wakeOnFlashlight", "d");
const idEventDef AI_LocateEnemy("locateEnemy");
const idEventDef AI_KickObstacles("kickObstacles", "Ef");
const idEventDef AI_GetObstacle("getObstacle", NULL, 'e');
const idEventDef AI_PushPointIntoAAS("pushPointIntoAAS", "v", 'v');
const idEventDef AI_GetTurnRate("getTurnRate", NULL, 'f');
const idEventDef AI_SetTurnRate("setTurnRate", "f");
const idEventDef AI_AnimTurn("animTurn", "f");
const idEventDef AI_AllowHiddenMovement("allowHiddenMovement", "d");
const idEventDef AI_TriggerParticles("triggerParticles", "s");
const idEventDef AI_FindActorsInBounds("findActorsInBounds", "vv", 'e');
const idEventDef AI_CanReachPosition("canReachPosition", "v", 'd');
const idEventDef AI_CanReachEntity("canReachEntity", "E", 'd');
const idEventDef AI_CanReachEnemy("canReachEnemy", NULL, 'd');
const idEventDef AI_GetReachableEntityPosition("getReachableEntityPosition", "e", 'v');

CLASS_DECLARATION(idActor, idAI)
EVENT(EV_Activate, idAI::Event_Activate)
EVENT(EV_Touch, idAI::Event_Touch)
EVENT(AI_FindEnemy, idAI::Event_FindEnemy)
EVENT(AI_FindEnemyAI, idAI::Event_FindEnemyAI)
EVENT(AI_FindEnemyInCombatNodes, idAI::Event_FindEnemyInCombatNodes)
EVENT(AI_ClosestReachableEnemyOfEntity, idAI::Event_ClosestReachableEnemyOfEntity)
EVENT(AI_HeardSound, idAI::Event_HeardSound)
EVENT(AI_SetEnemy, idAI::Event_SetEnemy)
EVENT(AI_ClearEnemy, idAI::Event_ClearEnemy)
EVENT(AI_MuzzleFlash, idAI::Event_MuzzleFlash)
EVENT(AI_CreateMissile, idAI::Event_CreateMissile)
EVENT(AI_AttackMissile, idAI::Event_AttackMissile)
EVENT(AI_FireMissileAtTarget, idAI::Event_FireMissileAtTarget)
EVENT(AI_LaunchMissile, idAI::Event_LaunchMissile)
EVENT(AI_AttackMelee, idAI::Event_AttackMelee)
EVENT(AI_DirectDamage, idAI::Event_DirectDamage)
EVENT(AI_RadiusDamageFromJoint, idAI::Event_RadiusDamageFromJoint)
EVENT(AI_BeginAttack, idAI::Event_BeginAttack)
EVENT(AI_EndAttack, idAI::Event_EndAttack)
EVENT(AI_MeleeAttackToJoint, idAI::Event_MeleeAttackToJoint)
EVENT(AI_RandomPath, idAI::Event_RandomPath)
EVENT(AI_CanBecomeSolid, idAI::Event_CanBecomeSolid)
EVENT(AI_BecomeSolid, idAI::Event_BecomeSolid)
EVENT(EV_BecomeNonSolid, idAI::Event_BecomeNonSolid)
EVENT(AI_BecomeRagdoll, idAI::Event_BecomeRagdoll)
EVENT(AI_StopRagdoll, idAI::Event_StopRagdoll)
EVENT(AI_SetHealth, idAI::Event_SetHealth)
EVENT(AI_GetHealth, idAI::Event_GetHealth)
EVENT(AI_AllowDamage, idAI::Event_AllowDamage)
EVENT(AI_IgnoreDamage, idAI::Event_IgnoreDamage)
EVENT(AI_GetCurrentYaw, idAI::Event_GetCurrentYaw)
EVENT(AI_TurnTo, idAI::Event_TurnTo)
EVENT(AI_TurnToPos, idAI::Event_TurnToPos)
EVENT(AI_TurnToEntity, idAI::Event_TurnToEntity)
EVENT(AI_MoveStatus, idAI::Event_MoveStatus)
EVENT(AI_StopMove, idAI::Event_StopMove)
EVENT(AI_MoveToCover, idAI::Event_MoveToCover)
EVENT(AI_MoveToEnemy, idAI::Event_MoveToEnemy)
EVENT(AI_MoveToEnemyHeight, idAI::Event_MoveToEnemyHeight)
EVENT(AI_MoveOutOfRange, idAI::Event_MoveOutOfRange)
EVENT(AI_MoveToAttackPosition, idAI::Event_MoveToAttackPosition)
EVENT(AI_Wander, idAI::Event_Wander)
EVENT(AI_MoveToEntity, idAI::Event_MoveToEntity)
EVENT(AI_MoveToPosition, idAI::Event_MoveToPosition)
EVENT(AI_SlideTo, idAI::Event_SlideTo)
EVENT(AI_FacingIdeal, idAI::Event_FacingIdeal)
EVENT(AI_FaceEnemy, idAI::Event_FaceEnemy)
EVENT(AI_FaceEntity, idAI::Event_FaceEntity)
EVENT(AI_WaitAction, idAI::Event_WaitAction)
EVENT(AI_GetCombatNode, idAI::Event_GetCombatNode)
EVENT(AI_EnemyInCombatCone, idAI::Event_EnemyInCombatCone)
EVENT(AI_WaitMove, idAI::Event_WaitMove)
EVENT(AI_GetJumpVelocity, idAI::Event_GetJumpVelocity)
EVENT(AI_EntityInAttackCone, idAI::Event_EntityInAttackCone)
EVENT(AI_CanSeeEntity, idAI::Event_CanSeeEntity)
EVENT(AI_SetTalkTarget, idAI::Event_SetTalkTarget)
EVENT(AI_GetTalkTarget, idAI::Event_GetTalkTarget)
EVENT(AI_SetTalkState, idAI::Event_SetTalkState)
EVENT(AI_EnemyRange, idAI::Event_EnemyRange)
EVENT(AI_EnemyRange2D, idAI::Event_EnemyRange2D)
EVENT(AI_GetEnemy, idAI::Event_GetEnemy)
EVENT(AI_GetEnemyPos, idAI::Event_GetEnemyPos)
EVENT(AI_GetEnemyEyePos, idAI::Event_GetEnemyEyePos)
EVENT(AI_PredictEnemyPos, idAI::Event_PredictEnemyPos)
EVENT(AI_CanHitEnemy, idAI::Event_CanHitEnemy)
EVENT(AI_CanHitEnemyFromAnim, idAI::Event_CanHitEnemyFromAnim)
EVENT(AI_CanHitEnemyFromJoint, idAI::Event_CanHitEnemyFromJoint)
EVENT(AI_EnemyPositionValid, idAI::Event_EnemyPositionValid)
EVENT(AI_ChargeAttack, idAI::Event_ChargeAttack)
EVENT(AI_TestChargeAttack, idAI::Event_TestChargeAttack)
EVENT(AI_TestAnimMoveTowardEnemy, idAI::Event_TestAnimMoveTowardEnemy)
EVENT(AI_TestAnimMove, idAI::Event_TestAnimMove)
EVENT(AI_TestMoveToPosition, idAI::Event_TestMoveToPosition)
EVENT(AI_TestMeleeAttack, idAI::Event_TestMeleeAttack)
EVENT(AI_TestAnimAttack, idAI::Event_TestAnimAttack)
EVENT(AI_Shrivel, idAI::Event_Shrivel)
EVENT(AI_Burn, idAI::Event_Burn)
EVENT(AI_PreBurn, idAI::Event_PreBurn)
EVENT(AI_SetSmokeVisibility, idAI::Event_SetSmokeVisibility)
EVENT(AI_NumSmokeEmitters, idAI::Event_NumSmokeEmitters)
EVENT(AI_ClearBurn, idAI::Event_ClearBurn)
EVENT(AI_StopThinking, idAI::Event_StopThinking)
EVENT(AI_GetTurnDelta, idAI::Event_GetTurnDelta)
EVENT(AI_GetMoveType, idAI::Event_GetMoveType)
EVENT(AI_SetMoveType, idAI::Event_SetMoveType)
EVENT(AI_SaveMove, idAI::Event_SaveMove)
EVENT(AI_RestoreMove, idAI::Event_RestoreMove)
EVENT(AI_AllowMovement, idAI::Event_AllowMovement)
EVENT(AI_JumpFrame, idAI::Event_JumpFrame)
EVENT(AI_EnableClip, idAI::Event_EnableClip)
EVENT(AI_DisableClip, idAI::Event_DisableClip)
EVENT(AI_EnableGravity, idAI::Event_EnableGravity)
EVENT(AI_DisableGravity, idAI::Event_DisableGravity)
EVENT(AI_EnableAFPush, idAI::Event_EnableAFPush)
EVENT(AI_DisableAFPush, idAI::Event_DisableAFPush)
EVENT(AI_SetFlySpeed, idAI::Event_SetFlySpeed)
EVENT(AI_SetFlyOffset, idAI::Event_SetFlyOffset)
EVENT(AI_ClearFlyOffset, idAI::Event_ClearFlyOffset)
EVENT(AI_GetClosestHiddenTarget, idAI::Event_GetClosestHiddenTarget)
EVENT(AI_GetRandomTarget, idAI::Event_GetRandomTarget)
EVENT(AI_TravelDistanceToPoint, idAI::Event_TravelDistanceToPoint)
EVENT(AI_TravelDistanceToEntity, idAI::Event_TravelDistanceToEntity)
EVENT(AI_TravelDistanceBetweenPoints, idAI::Event_TravelDistanceBetweenPoints)
EVENT(AI_TravelDistanceBetweenEntities, idAI::Event_TravelDistanceBetweenEntities)
EVENT(AI_LookAtEntity, idAI::Event_LookAtEntity)
EVENT(AI_LookAtEnemy, idAI::Event_LookAtEnemy)
EVENT(AI_SetJointMod, idAI::Event_SetJointMod)
EVENT(AI_ThrowMoveable, idAI::Event_ThrowMoveable)
EVENT(AI_ThrowAF, idAI::Event_ThrowAF)
EVENT(EV_GetAngles, idAI::Event_GetAngles)
EVENT(EV_SetAngles, idAI::Event_SetAngles)
EVENT(AI_RealKill, idAI::Event_RealKill)
EVENT(AI_Kill, idAI::Event_Kill)
EVENT(AI_WakeOnFlashlight, idAI::Event_WakeOnFlashlight)
EVENT(AI_LocateEnemy, idAI::Event_LocateEnemy)
EVENT(AI_KickObstacles, idAI::Event_KickObstacles)
EVENT(AI_GetObstacle, idAI::Event_GetObstacle)
EVENT(AI_PushPointIntoAAS, idAI::Event_PushPointIntoAAS)
EVENT(AI_GetTurnRate, idAI::Event_GetTurnRate)
EVENT(AI_SetTurnRate, idAI::Event_SetTurnRate)
EVENT(AI_AnimTurn, idAI::Event_AnimTurn)
EVENT(AI_AllowHiddenMovement, idAI::Event_AllowHiddenMovement)
EVENT(AI_TriggerParticles, idAI::Event_TriggerParticles)
EVENT(AI_FindActorsInBounds, idAI::Event_FindActorsInBounds)
EVENT(AI_CanReachPosition, idAI::Event_CanReachPosition)
EVENT(AI_CanReachEntity, idAI::Event_CanReachEntity)
EVENT(AI_CanReachEnemy, idAI::Event_CanReachEnemy)
EVENT(AI_GetReachableEntityPosition, idAI::Event_GetReachableEntityPosition)
END_CLASS



/*
=====================
idAI::Native_Activate
=====================
*/
void idAI::Native_Activate(idEntity* activator) {
	Activate(activator);
}

/*
=====================
idAI::Native_Touch
=====================
*/
void idAI::Native_Touch(idEntity* other, trace_t* trace) {
	if (!enemy.GetEntity() && !other->fl.notarget && (ReactionTo(other) & ATTACK_ON_ACTIVATE)) {
		Activate(other);
	}
	AI_PUSHED = true;
}

/*
=====================
idAI::Native_FindEnemy
=====================
*/
idEntity* idAI::Native_FindEnemy(int useFOV) {
	int			i;
	idEntity* ent;
	idActor* actor;

	if (gameLocal.InPlayerPVS(this)) {
		for (i = 0; i < gameLocal.numClients; i++) {
			ent = gameLocal.entities[i];

			if (!ent || !ent->IsType(idActor::Type)) {
				continue;
			}

			actor = static_cast<idActor*>(ent);
			if ((actor->health <= 0) || !(ReactionTo(actor) & ATTACK_ON_SIGHT)) {
				continue;
			}

			if (CanSee(actor, useFOV != 0)) {
				return actor;
				return NULL;
			}
		}
	}

	return NULL;
}

/*
=====================
idAI::Native_FindEnemyAI
=====================
*/
idEntity* idAI::Native_FindEnemyAI(int useFOV) {
	idEntity* ent;
	idActor* actor;
	idActor* bestEnemy;
	float		bestDist;
	float		dist;
	idVec3		delta;
	pvsHandle_t pvs;

	pvs = gameLocal.pvs.SetupCurrentPVS(GetPVSAreas(), GetNumPVSAreas());

	bestDist = idMath::INFINITY;
	bestEnemy = NULL;
	for (ent = gameLocal.activeEntities.Next(); ent != NULL; ent = ent->activeNode.Next()) {
		if (ent->fl.hidden || ent->fl.isDormant || !ent->IsType(idActor::Type)) {
			continue;
		}

		actor = static_cast<idActor*>(ent);
		if ((actor->health <= 0) || !(ReactionTo(actor) & ATTACK_ON_SIGHT)) {
			continue;
		}

		if (!gameLocal.pvs.InCurrentPVS(pvs, actor->GetPVSAreas(), actor->GetNumPVSAreas())) {
			continue;
		}

		delta = physicsObj.GetOrigin() - actor->GetPhysics()->GetOrigin();
		dist = delta.LengthSqr();
		if ((dist < bestDist) && CanSee(actor, useFOV != 0)) {
			bestDist = dist;
			bestEnemy = actor;
		}
	}

	gameLocal.pvs.FreeCurrentPVS(pvs);
	return bestEnemy;
}

/*
=====================
idAI::Native_FindEnemyInCombatNodes
=====================
*/
idEntity* idAI::Native_FindEnemyInCombatNodes(void) {
	int				i, j;
	idCombatNode* node;
	idEntity* ent;
	idEntity* targetEnt;
	idActor* actor;

	if (!gameLocal.InPlayerPVS(this)) {
		// don't locate the player when we're not in his PVS
		return NULL;
		return NULL;
	}

	for (i = 0; i < gameLocal.numClients; i++) {
		ent = gameLocal.entities[i];

		if (!ent || !ent->IsType(idActor::Type)) {
			continue;
		}

		actor = static_cast<idActor*>(ent);
		if ((actor->health <= 0) || !(ReactionTo(actor) & ATTACK_ON_SIGHT)) {
			continue;
		}

		for (j = 0; j < targets.Num(); j++) {
			targetEnt = targets[j].GetEntity();
			if (!targetEnt || !targetEnt->IsType(idCombatNode::Type)) {
				continue;
			}

			node = static_cast<idCombatNode*>(targetEnt);
			if (!node->IsDisabled() && node->EntityInView(actor, actor->GetPhysics()->GetOrigin())) {
				return actor;
				return NULL;
			}
		}
	}

	return NULL;
}

/*
=====================
idAI::Native_ClosestReachableEnemyOfEntity
=====================
*/
idEntity* idAI::Native_ClosestReachableEnemyOfEntity(idEntity* team_mate) {
	idActor* actor;
	idActor* ent;
	idActor* bestEnt;
	float	bestDistSquared;
	float	distSquared;
	idVec3	delta;
	int		areaNum;
	int		enemyAreaNum;
	aasPath_t path;

	if (!team_mate->IsType(idActor::Type)) {
		gameLocal.Error("Entity '%s' is not an AI character or player", team_mate->GetName());
	}

	actor = static_cast<idActor*>(team_mate);

	const idVec3& origin = physicsObj.GetOrigin();
	areaNum = PointReachableAreaNum(origin);

	bestDistSquared = idMath::INFINITY;
	bestEnt = NULL;
	for (ent = actor->enemyList.Next(); ent != NULL; ent = ent->enemyNode.Next()) {
		if (ent->fl.hidden) {
			continue;
		}
		delta = ent->GetPhysics()->GetOrigin() - origin;
		distSquared = delta.LengthSqr();
		if (distSquared < bestDistSquared) {
			const idVec3& enemyPos = ent->GetPhysics()->GetOrigin();
			enemyAreaNum = PointReachableAreaNum(enemyPos);
			if ((areaNum != 0) && PathToGoal(path, areaNum, origin, enemyAreaNum, enemyPos)) {
				bestEnt = ent;
				bestDistSquared = distSquared;
			}
		}
	}

	return bestEnt;
}

/*
=====================
idAI::Native_HeardSound
=====================
*/
idEntity* idAI::Native_HeardSound(int ignore_team) {
	// check if we heard any sounds in the last frame
	idActor* actor = gameLocal.GetAlertEntity();
	if (actor && (!ignore_team || (ReactionTo(actor) & ATTACK_ON_SIGHT)) && gameLocal.InPlayerPVS(this)) {
		idVec3 pos = actor->GetPhysics()->GetOrigin();
		idVec3 org = physicsObj.GetOrigin();
		float dist = (pos - org).LengthSqr();
		if (dist < Square(AI_HEARING_RANGE)) {
			return actor;
			return NULL;
		}
	}

	return NULL;
}

/*
=====================
idAI::Native_SetEnemy
=====================
*/
void idAI::Native_SetEnemy(idEntity* ent) {
	if (!ent) {
		ClearEnemy();
	}
	else if (!ent->IsType(idActor::Type)) {
		gameLocal.Error("'%s' is not an idActor (player or ai controlled character)", ent->name.c_str());
	}
	else {
		SetEnemy(static_cast<idActor*>(ent));
	}
}

/*
=====================
idAI::Native_ClearEnemy
=====================
*/
void idAI::Native_ClearEnemy(void) {
	ClearEnemy();
}

/*
=====================
idAI::Native_MuzzleFlash
=====================
*/
void idAI::Native_MuzzleFlash(const char* jointname) {
	idVec3	muzzle;
	idMat3	axis;

	GetMuzzle(jointname, muzzle, axis);
	TriggerWeaponEffects(muzzle);
}

/*
=====================
idAI::Native_CreateMissile
=====================
*/
idEntity* idAI::Native_CreateMissile(const char* jointname) {
	idVec3 muzzle;
	idMat3 axis;

	if (!projectileDef) {
		gameLocal.Warning("%s (%s) doesn't have a projectile specified", name.c_str(), GetEntityDefName());
		return NULL;
	}

	GetMuzzle(jointname, muzzle, axis);
	CreateProjectile(muzzle, viewAxis[0] * physicsObj.GetGravityAxis());
	if (projectile.GetEntity()) {
		if (!jointname || !jointname[0]) {
			projectile.GetEntity()->Bind(this, true);
		}
		else {
			projectile.GetEntity()->BindToJoint(this, jointname, true);
		}
	}
	return projectile.GetEntity();
}

/*
=====================
idAI::Native_AttackMissile
=====================
*/
idEntity* idAI::Native_AttackMissile(const char* jointname) {
	idProjectile* proj;

	proj = LaunchProjectile(jointname, enemy.GetEntity(), true);
	return proj;
}

/*
=====================
idAI::Native_FireMissileAtTarget
=====================
*/
idEntity* idAI::Native_FireMissileAtTarget(const char* jointname, const char* targetname) {
	idEntity* aent;
	idProjectile* proj;

	aent = gameLocal.FindEntity(targetname);
	if (!aent) {
		gameLocal.Warning("Entity '%s' not found for 'fireMissileAtTarget'", targetname);
	}

	proj = LaunchProjectile(jointname, aent, false);
	return proj;
}

/*
=====================
idAI::Native_LaunchMissile
=====================
*/
idEntity* idAI::Native_LaunchMissile(const idVec3& org, const idAngles& ang) {
	idVec3		start;
	trace_t		tr;
	idBounds	projBounds;
	const idClipModel* projClip;
	idMat3		axis;
	float		distance;

	if (!projectileDef) {
		gameLocal.Warning("%s (%s) doesn't have a projectile specified", name.c_str(), GetEntityDefName());
		return NULL;
		return NULL;
	}

	axis = ang.ToMat3();
	if (!projectile.GetEntity()) {
		CreateProjectile(org, axis[0]);
	}

	// make sure the projectile starts inside the monster bounding box
	const idBounds& ownerBounds = physicsObj.GetAbsBounds();
	projClip = projectile.GetEntity()->GetPhysics()->GetClipModel();
	projBounds = projClip->GetBounds().Rotate(projClip->GetAxis());

	// check if the owner bounds is bigger than the projectile bounds
	if (((ownerBounds[1][0] - ownerBounds[0][0]) > (projBounds[1][0] - projBounds[0][0])) &&
		((ownerBounds[1][1] - ownerBounds[0][1]) > (projBounds[1][1] - projBounds[0][1])) &&
		((ownerBounds[1][2] - ownerBounds[0][2]) > (projBounds[1][2] - projBounds[0][2]))) {
		if ((ownerBounds - projBounds).RayIntersection(org, viewAxis[0], distance)) {
			start = org + distance * viewAxis[0];
		}
		else {
			start = ownerBounds.GetCenter();
		}
	}
	else {
		// projectile bounds bigger than the owner bounds, so just start it from the center
		start = ownerBounds.GetCenter();
	}

	gameLocal.clip.Translation(tr, start, org, projClip, projClip->GetAxis(), MASK_SHOT_RENDERMODEL, this);

	// launch the projectile
	idProjectile* launchedProjectile = projectile.GetEntity();
	launchedProjectile->Launch(tr.endpos, axis[0], vec3_origin);
	projectile = NULL;

	TriggerWeaponEffects(tr.endpos);

	lastAttackTime = gameLocal.time;
	return launchedProjectile;
}

/*
=====================
idAI::Native_AttackMelee
=====================
*/
int idAI::Native_AttackMelee(const char* meleeDefName) {
	bool hit;

	hit = AttackMelee(meleeDefName);
	return hit;
}

/*
=====================
idAI::Native_DirectDamage
=====================
*/
void idAI::Native_DirectDamage(idEntity* damageTarget, const char* damageDefName) {
	DirectDamage(damageDefName, damageTarget);
}

/*
=====================
idAI::Native_RadiusDamageFromJoint
=====================
*/
void idAI::Native_RadiusDamageFromJoint(const char* jointname, const char* damageDefName) {
	jointHandle_t joint;
	idVec3 org;
	idMat3 axis;

	if (!jointname || !jointname[0]) {
		org = physicsObj.GetOrigin();
	}
	else {
		joint = animator.GetJointHandle(jointname);
		if (joint == INVALID_JOINT) {
			gameLocal.Error("Unknown joint '%s' on %s", jointname, GetEntityDefName());
		}
		GetJointWorldTransform(joint, gameLocal.time, org, axis);
	}

	gameLocal.RadiusDamage(org, this, this, this, this, damageDefName);
}

/*
=====================
idAI::Native_RandomPath
=====================
*/
idEntity* idAI::Native_RandomPath(void) {
	idPathCorner* path;

	path = idPathCorner::RandomPath(this, NULL);
	return path;
}

/*
=====================
idAI::Native_BeginAttack
=====================
*/
void idAI::Native_BeginAttack(const char* name) {
	BeginAttack(name);
}

/*
=====================
idAI::Native_EndAttack
=====================
*/
void idAI::Native_EndAttack(void) {
	EndAttack();
}

/*
=====================
idAI::Native_MeleeAttackToJoint
=====================
*/
int idAI::Native_MeleeAttackToJoint(const char* jointname, const char* meleeDefName) {
	jointHandle_t	joint;
	idVec3			start;
	idVec3			end;
	idMat3			axis;
	trace_t			trace;
	idEntity* hitEnt;

	joint = animator.GetJointHandle(jointname);
	if (joint == INVALID_JOINT) {
		gameLocal.Error("Unknown joint '%s' on %s", jointname, GetEntityDefName());
	}
	animator.GetJointTransform(joint, gameLocal.time, end, axis);
	end = physicsObj.GetOrigin() + (end + modelOffset) * viewAxis * physicsObj.GetGravityAxis();
	start = GetEyePosition();

	if (ai_debugMove.GetBool()) {
		gameRenderWorld->DebugLine(colorYellow, start, end, gameLocal.msec);
	}

	gameLocal.clip.TranslationEntities(trace, start, end, NULL, mat3_identity, MASK_SHOT_BOUNDINGBOX, this);
	if (trace.fraction < 1.0f) {
		hitEnt = gameLocal.GetTraceEntity(trace);
		if (hitEnt && hitEnt->IsType(idActor::Type)) {
			DirectDamage(meleeDefName, hitEnt);
			return true;
			return false;
		}
	}

	return false;
}

/*
=====================
idAI::Native_CanBecomeSolid
=====================
*/
float idAI::Native_CanBecomeSolid(void) {
	int			i;
	int			num;
	idEntity* hit;
	idClipModel* cm;
	idClipModel* clipModels[MAX_GENTITIES];

	num = gameLocal.clip.ClipModelsTouchingBounds(physicsObj.GetAbsBounds(), MASK_MONSTERSOLID, clipModels, MAX_GENTITIES);
	for (i = 0; i < num; i++) {
		cm = clipModels[i];

		// don't check render entities
		if (cm->IsRenderModel()) {
			continue;
		}

		hit = cm->GetEntity();
		if ((hit == this) || !hit->fl.takedamage) {
			continue;
		}

		if (physicsObj.ClipContents(cm)) {
			return false;
			return 0.0f;
		}
	}

	return true;
}

/*
=====================
idAI::Native_BecomeSolid
=====================
*/
void idAI::Native_BecomeSolid(void) {
	physicsObj.EnableClip();
	if (spawnArgs.GetBool("big_monster")) {
		physicsObj.SetContents(0);
	}
	else if (use_combat_bbox) {
		physicsObj.SetContents(CONTENTS_BODY | CONTENTS_SOLID);
	}
	else {
		physicsObj.SetContents(CONTENTS_BODY);
	}
	physicsObj.GetClipModel()->Link(gameLocal.clip);
	fl.takedamage = !spawnArgs.GetBool("noDamage");
}

/*
=====================
idAI::Native_BecomeNonSolid
=====================
*/
void idAI::Native_BecomeNonSolid(void) {
	fl.takedamage = false;
	physicsObj.SetContents(0);
	physicsObj.GetClipModel()->Unlink();
}

/*
=====================
idAI::Native_BecomeRagdoll
=====================
*/
int idAI::Native_BecomeRagdoll(void) {
	bool result;

	result = StartRagdoll();
	return result;
}

/*
=====================
idAI::Native_StopRagdoll
=====================
*/
void idAI::Native_StopRagdoll(void) {
	StopRagdoll();

	// set back the monster physics
	SetPhysics(&physicsObj);
}

/*
=====================
idAI::Native_SetHealth
=====================
*/
void idAI::Native_SetHealth(float newHealth) {
	health = newHealth;
	fl.takedamage = true;
	if (health > 0) {
		AI_DEAD = false;
	}
	else {
		AI_DEAD = true;
	}
}

/*
=====================
idAI::Native_GetHealth
=====================
*/
float idAI::Native_GetHealth(void) {
	return health;
}

/*
=====================
idAI::Native_AllowDamage
=====================
*/
void idAI::Native_AllowDamage(void) {
	fl.takedamage = true;
}

/*
=====================
idAI::Native_IgnoreDamage
=====================
*/
void idAI::Native_IgnoreDamage(void) {
	fl.takedamage = false;
}

/*
=====================
idAI::Native_GetCurrentYaw
=====================
*/
float idAI::Native_GetCurrentYaw(void) {
	return current_yaw;
}

/*
=====================
idAI::Native_TurnTo
=====================
*/
void idAI::Native_TurnTo(float angle) {
	TurnToward(angle);
}

/*
=====================
idAI::Native_TurnToPos
=====================
*/
void idAI::Native_TurnToPos(const idVec3& pos) {
	TurnToward(pos);
}

/*
=====================
idAI::Native_TurnToEntity
=====================
*/
void idAI::Native_TurnToEntity(idEntity* ent) {
	if (ent) {
		TurnToward(ent->GetPhysics()->GetOrigin());
	}
}

/*
=====================
idAI::Native_MoveStatus
=====================
*/
int idAI::Native_MoveStatus(void) {
	return move.moveStatus;
}

/*
=====================
idAI::Native_StopMove
=====================
*/
void idAI::Native_StopMove(void) {
	StopMove(MOVE_STATUS_DONE);
}

/*
=====================
idAI::Native_MoveToCover
=====================
*/
void idAI::Native_MoveToCover(void) {
	idActor* enemyEnt = enemy.GetEntity();

	StopMove(MOVE_STATUS_DEST_NOT_FOUND);
	if (!enemyEnt || !MoveToCover(enemyEnt, lastVisibleEnemyPos)) {
		return;
	}
}

/*
=====================
idAI::Native_MoveToEnemy
=====================
*/
void idAI::Native_MoveToEnemy(void) {
	StopMove(MOVE_STATUS_DEST_NOT_FOUND);
	if (!enemy.GetEntity() || !MoveToEnemy()) {
		return;
	}
}

/*
=====================
idAI::Native_MoveToEnemyHeight
=====================
*/
void idAI::Native_MoveToEnemyHeight(void) {
	StopMove(MOVE_STATUS_DEST_NOT_FOUND);
	MoveToEnemyHeight();
}

/*
=====================
idAI::Native_MoveOutOfRange
=====================
*/
void idAI::Native_MoveOutOfRange(idEntity* entity, float range) {
	StopMove(MOVE_STATUS_DEST_NOT_FOUND);
	MoveOutOfRange(entity, range);
}

/*
=====================
idAI::Native_MoveToAttackPosition
=====================
*/
void idAI::Native_MoveToAttackPosition(idEntity* entity, const char* attack_anim) {
	int anim;

	StopMove(MOVE_STATUS_DEST_NOT_FOUND);

	anim = GetAnim(ANIMCHANNEL_LEGS, attack_anim);
	if (!anim) {
		gameLocal.Error("Unknown anim '%s'", attack_anim);
	}

	MoveToAttackPosition(entity, anim);
}

/*
=====================
idAI::Native_MoveToEntity
=====================
*/
void idAI::Native_MoveToEntity(idEntity* ent) {
	StopMove(MOVE_STATUS_DEST_NOT_FOUND);
	if (ent) {
		MoveToEntity(ent);
	}
}

/*
=====================
idAI::Native_MoveToPosition
=====================
*/
void idAI::Native_MoveToPosition(const idVec3& pos) {
	StopMove(MOVE_STATUS_DONE);
	MoveToPosition(pos);
}

/*
=====================
idAI::Native_SlideTo
=====================
*/
void idAI::Native_SlideTo(const idVec3& pos, float time) {
	SlideToPosition(pos, time);
}

/*
=====================
idAI::Native_Wander
=====================
*/
void idAI::Native_Wander(void) {
	WanderAround();
}

/*
=====================
idAI::Native_FacingIdeal
=====================
*/
int idAI::Native_FacingIdeal(void) {
	bool facing = FacingIdeal();
	return facing;
}

/*
=====================
idAI::Native_FaceEnemy
=====================
*/
void idAI::Native_FaceEnemy(void) {
	FaceEnemy();
}

/*
=====================
idAI::Native_FaceEntity
=====================
*/
void idAI::Native_FaceEntity(idEntity* ent) {
	FaceEntity(ent);
}

/*
=====================
idAI::Native_GetCombatNode
=====================
*/
idEntity* idAI::Native_GetCombatNode(void) {
	int				i;
	float			dist;
	idEntity* targetEnt;
	idCombatNode* node;
	float			bestDist;
	idCombatNode* bestNode;
	idActor* enemyEnt = enemy.GetEntity();

	if (!targets.Num()) {
		// no combat nodes
		return NULL;
		return NULL;
	}

	if (!enemyEnt || !EnemyPositionValid()) {
		// don't return a combat node if we don't have an enemy or
		// if we can see he's not in the last place we saw him
		return NULL;
		return NULL;
	}

	// find the closest attack node that can see our enemy and is closer than our enemy
	bestNode = NULL;
	const idVec3& myPos = physicsObj.GetOrigin();
	bestDist = (myPos - lastVisibleEnemyPos).LengthSqr();
	for (i = 0; i < targets.Num(); i++) {
		targetEnt = targets[i].GetEntity();
		if (!targetEnt || !targetEnt->IsType(idCombatNode::Type)) {
			continue;
		}

		node = static_cast<idCombatNode*>(targetEnt);
		if (!node->IsDisabled() && node->EntityInView(enemyEnt, lastVisibleEnemyPos)) {
			idVec3 org = node->GetPhysics()->GetOrigin();
			dist = (myPos - org).LengthSqr();
			if (dist < bestDist) {
				bestNode = node;
				bestDist = dist;
			}
		}
	}

	return bestNode;
}

/*
=====================
idAI::Native_EnemyInCombatCone
=====================
*/
int idAI::Native_EnemyInCombatCone(idEntity* ent, int use_current_enemy_location) {
	idCombatNode* node;
	bool			result;
	idActor* enemyEnt = enemy.GetEntity();

	if (!targets.Num()) {
		// no combat nodes
		return false;
		return false;
	}

	if (!enemyEnt) {
		// have to have an enemy
		return false;
		return false;
	}

	if (!ent || !ent->IsType(idCombatNode::Type)) {
		// not a combat node
		return false;
		return false;
	}

	node = static_cast<idCombatNode*>(ent);
	if (use_current_enemy_location) {
		const idVec3& pos = enemyEnt->GetPhysics()->GetOrigin();
		result = node->EntityInView(enemyEnt, pos);
	}
	else {
		result = node->EntityInView(enemyEnt, lastVisibleEnemyPos);
	}

	return result;
}

/*
=====================
idAI::Native_GetJumpVelocity
=====================
*/
idVec3 idAI::Native_GetJumpVelocity(const idVec3& pos, float speed, float max_height) {
	idVec3 start;
	idVec3 end;
	idVec3 dir;
	float dist;
	bool result;
	idEntity* enemyEnt = enemy.GetEntity();

	if (!enemyEnt) {
		return vec3_zero;
		return vec3_zero;
	}

	if (speed <= 0.0f) {
		gameLocal.Error("Invalid speed.  speed must be > 0.");
	}

	start = physicsObj.GetOrigin();
	end = pos;
	dir = end - start;
	dist = dir.Normalize();
	if (dist > 16.0f) {
		dist -= 16.0f;
		end -= dir * 16.0f;
	}

	result = PredictTrajectory(start, end, speed, physicsObj.GetGravity(), physicsObj.GetClipModel(), MASK_MONSTERSOLID, max_height, this, enemyEnt, ai_debugMove.GetBool() ? 4000 : 0, dir);
	if (result) {
		return dir * speed;
	}
	else {
		return vec3_zero;
	}
}

/*
=====================
idAI::Native_EntityInAttackCone
=====================
*/
int idAI::Native_EntityInAttackCone(idEntity* ent) {
	float	attack_cone;
	idVec3	delta;
	float	yaw;
	float	relYaw;

	if (!ent) {
		return false;
		return false;
	}

	delta = ent->GetPhysics()->GetOrigin() - GetEyePosition();

	// get our gravity normal
	const idVec3& gravityDir = GetPhysics()->GetGravityNormal();

	// infinite vertical vision, so project it onto our orientation plane
	delta -= gravityDir * (gravityDir * delta);

	delta.Normalize();
	yaw = delta.ToYaw();

	attack_cone = spawnArgs.GetFloat("attack_cone", "70");
	relYaw = idMath::AngleNormalize180(ideal_yaw - yaw);
	if (idMath::Fabs(relYaw) < (attack_cone * 0.5f)) {
		return true;
	}
	else {
		return false;
	}
}

/*
=====================
idAI::Native_CanSeeEntity
=====================
*/
int idAI::Native_CanSeeEntity(idEntity* ent) {
	if (!ent) {
		return false;
		return false;
	}

	bool cansee = CanSee(ent, false);
	return cansee;
}

/*
=====================
idAI::Native_SetTalkTarget
=====================
*/
void idAI::Native_SetTalkTarget(idEntity* target) {
	if (target && !target->IsType(idActor::Type)) {
		gameLocal.Error("Cannot set talk target to '%s'.  Not a character or player.", target->GetName());
	}
	talkTarget = static_cast<idActor*>(target);
	if (target) {
		AI_TALK = true;
	}
	else {
		AI_TALK = false;
	}
}

/*
=====================
idAI::Native_GetTalkTarget
=====================
*/
idEntity* idAI::Native_GetTalkTarget(void) {
	return talkTarget.GetEntity();
}

/*
=====================
idAI::Native_SetTalkState
=====================
*/
void idAI::Native_SetTalkState(int state) {
	if ((state < 0) || (state >= NUM_TALK_STATES)) {
		gameLocal.Error("Invalid talk state (%d)", state);
	}

	talk_state = static_cast<talkState_t>(state);
}

/*
=====================
idAI::Native_EnemyRange
=====================
*/
float idAI::Native_EnemyRange(void) {
	float dist;
	idActor* enemyEnt = enemy.GetEntity();

	if (enemyEnt) {
		dist = (enemyEnt->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin()).Length();
	}
	else {
		// Just some really high number
		dist = idMath::INFINITY;
	}

	return dist;
}

/*
=====================
idAI::Native_EnemyRange2D
=====================
*/
float idAI::Native_EnemyRange2D(void) {
	float dist;
	idActor* enemyEnt = enemy.GetEntity();

	if (enemyEnt) {
		dist = (enemyEnt->GetPhysics()->GetOrigin().ToVec2() - GetPhysics()->GetOrigin().ToVec2()).Length();
	}
	else {
		// Just some really high number
		dist = idMath::INFINITY;
	}

	return dist;
}

/*
=====================
idAI::Native_GetEnemy
=====================
*/
idEntity* idAI::Native_GetEnemy(void) {
	return enemy.GetEntity();
}

/*
=====================
idAI::Native_GetEnemyPos
=====================
*/
idVec3 idAI::Native_GetEnemyPos(void) {
	return lastVisibleEnemyPos;
}

/*
=====================
idAI::Native_GetEnemyEyePos
=====================
*/
idVec3 idAI::Native_GetEnemyEyePos(void) {
	return lastVisibleEnemyPos + lastVisibleEnemyEyeOffset;
}

/*
=====================
idAI::Native_PredictEnemyPos
=====================
*/
idVec3 idAI::Native_PredictEnemyPos(float time) {
	predictedPath_t path;
	idActor* enemyEnt = enemy.GetEntity();

	// if no enemy set
	if (!enemyEnt) {
		return physicsObj.GetOrigin();
		return vec3_zero;
	}

	// predict the enemy movement
	idAI::PredictPath(enemyEnt, aas, lastVisibleEnemyPos, enemyEnt->GetPhysics()->GetLinearVelocity(), SEC2MS(time), SEC2MS(time), (move.moveType == MOVETYPE_FLY) ? SE_BLOCKED : (SE_BLOCKED | SE_ENTER_LEDGE_AREA), path);

	return path.endPos;
}

/*
=====================
idAI::Native_CanHitEnemy
=====================
*/
int idAI::Native_CanHitEnemy(void) {
	trace_t	tr;
	idEntity* hit;

	idActor* enemyEnt = enemy.GetEntity();
	if (!AI_ENEMY_VISIBLE || !enemyEnt) {
		return false;
		return false;
	}

	// don't check twice per frame
	if (gameLocal.time == lastHitCheckTime) {
		return lastHitCheckResult;
		return false;
	}

	lastHitCheckTime = gameLocal.time;

	idVec3 toPos = enemyEnt->GetEyePosition();
	idVec3 eye = GetEyePosition();
	idVec3 dir;

	// expand the ray out as far as possible so we can detect anything behind the enemy
	dir = toPos - eye;
	dir.Normalize();
	toPos = eye + dir * MAX_WORLD_SIZE;
	gameLocal.clip.TracePoint(tr, eye, toPos, MASK_SHOT_BOUNDINGBOX, this);
	hit = gameLocal.GetTraceEntity(tr);
	if (tr.fraction >= 1.0f || (hit == enemyEnt)) {
		lastHitCheckResult = true;
	}
	else if ((tr.fraction < 1.0f) && (hit->IsType(idAI::Type)) &&
		(static_cast<idAI*>(hit)->team != team)) {
		lastHitCheckResult = true;
	}
	else {
		lastHitCheckResult = false;
	}

	return lastHitCheckResult;
}

/*
=====================
idAI::Native_CanHitEnemyFromAnim
=====================
*/
int idAI::Native_CanHitEnemyFromAnim(const char* animname) {
	int		anim;
	idVec3	dir;
	idVec3	local_dir;
	idVec3	fromPos;
	idMat3	axis;
	idVec3	start;
	trace_t	tr;
	float	distance;

	idActor* enemyEnt = enemy.GetEntity();
	if (!AI_ENEMY_VISIBLE || !enemyEnt) {
		return false;
		return false;
	}

	anim = GetAnim(ANIMCHANNEL_LEGS, animname);
	if (!anim) {
		return false;
		return false;
	}

	// just do a ray test if close enough
	if (enemyEnt->GetPhysics()->GetAbsBounds().IntersectsBounds(physicsObj.GetAbsBounds().Expand(16.0f))) {
		return Native_CanHitEnemy();
		return false;
	}

	// calculate the world transform of the launch position
	const idVec3& org = physicsObj.GetOrigin();
	dir = lastVisibleEnemyPos - org;
	physicsObj.GetGravityAxis().ProjectVector(dir, local_dir);
	local_dir.z = 0.0f;
	local_dir.ToVec2().Normalize();
	axis = local_dir.ToMat3();
	fromPos = physicsObj.GetOrigin() + missileLaunchOffset[anim] * axis;

	if (projectileClipModel == NULL) {
		CreateProjectileClipModel();
	}

	// check if the owner bounds is bigger than the projectile bounds
	const idBounds& ownerBounds = physicsObj.GetAbsBounds();
	const idBounds& projBounds = projectileClipModel->GetBounds();
	if (((ownerBounds[1][0] - ownerBounds[0][0]) > (projBounds[1][0] - projBounds[0][0])) &&
		((ownerBounds[1][1] - ownerBounds[0][1]) > (projBounds[1][1] - projBounds[0][1])) &&
		((ownerBounds[1][2] - ownerBounds[0][2]) > (projBounds[1][2] - projBounds[0][2]))) {
		if ((ownerBounds - projBounds).RayIntersection(org, viewAxis[0], distance)) {
			start = org + distance * viewAxis[0];
		}
		else {
			start = ownerBounds.GetCenter();
		}
	}
	else {
		// projectile bounds bigger than the owner bounds, so just start it from the center
		start = ownerBounds.GetCenter();
	}

	gameLocal.clip.Translation(tr, start, fromPos, projectileClipModel, mat3_identity, MASK_SHOT_RENDERMODEL, this);
	fromPos = tr.endpos;

	if (GetAimDir(fromPos, enemy.GetEntity(), this, dir)) {
		return true;
	}
	else {
		return false;
	}
}

/*
=====================
idAI::Native_CanHitEnemyFromJoint
=====================
*/
int idAI::Native_CanHitEnemyFromJoint(const char* jointname) {
	trace_t	tr;
	idVec3	muzzle;
	idMat3	axis;
	idVec3	start;
	float	distance;

	idActor* enemyEnt = enemy.GetEntity();
	if (!AI_ENEMY_VISIBLE || !enemyEnt) {
		return false;
		return false;
	}

	// don't check twice per frame
	if (gameLocal.time == lastHitCheckTime) {
		return lastHitCheckResult;
		return false;
	}

	lastHitCheckTime = gameLocal.time;

	const idVec3& org = physicsObj.GetOrigin();
	idVec3 toPos = enemyEnt->GetEyePosition();
	jointHandle_t joint = animator.GetJointHandle(jointname);
	if (joint == INVALID_JOINT) {
		gameLocal.Error("Unknown joint '%s' on %s", jointname, GetEntityDefName());
	}
	animator.GetJointTransform(joint, gameLocal.time, muzzle, axis);
	muzzle = org + (muzzle + modelOffset) * viewAxis * physicsObj.GetGravityAxis();

	if (projectileClipModel == NULL) {
		CreateProjectileClipModel();
	}

	// check if the owner bounds is bigger than the projectile bounds
	const idBounds& ownerBounds = physicsObj.GetAbsBounds();
	const idBounds& projBounds = projectileClipModel->GetBounds();
	if (((ownerBounds[1][0] - ownerBounds[0][0]) > (projBounds[1][0] - projBounds[0][0])) &&
		((ownerBounds[1][1] - ownerBounds[0][1]) > (projBounds[1][1] - projBounds[0][1])) &&
		((ownerBounds[1][2] - ownerBounds[0][2]) > (projBounds[1][2] - projBounds[0][2]))) {
		if ((ownerBounds - projBounds).RayIntersection(org, viewAxis[0], distance)) {
			start = org + distance * viewAxis[0];
		}
		else {
			start = ownerBounds.GetCenter();
		}
	}
	else {
		// projectile bounds bigger than the owner bounds, so just start it from the center
		start = ownerBounds.GetCenter();
	}

	gameLocal.clip.Translation(tr, start, muzzle, projectileClipModel, mat3_identity, MASK_SHOT_BOUNDINGBOX, this);
	muzzle = tr.endpos;

	gameLocal.clip.Translation(tr, muzzle, toPos, projectileClipModel, mat3_identity, MASK_SHOT_BOUNDINGBOX, this);
	if (tr.fraction >= 1.0f || (gameLocal.GetTraceEntity(tr) == enemyEnt)) {
		lastHitCheckResult = true;
	}
	else {
		lastHitCheckResult = false;
	}

	return lastHitCheckResult;
}

/*
=====================
idAI::Native_EnemyPositionValid
=====================
*/
int idAI::Native_EnemyPositionValid(void) {
	bool result;

	result = EnemyPositionValid();
	return result;
}

/*
=====================
idAI::Native_ChargeAttack
=====================
*/
void idAI::Native_ChargeAttack(const char* damageDef) {
	idActor* enemyEnt = enemy.GetEntity();

	StopMove(MOVE_STATUS_DEST_NOT_FOUND);
	if (enemyEnt) {
		idVec3 enemyOrg;

		if (move.moveType == MOVETYPE_FLY) {
			// position destination so that we're in the enemy's view
			enemyOrg = enemyEnt->GetEyePosition();
			enemyOrg -= enemyEnt->GetPhysics()->GetGravityNormal() * fly_offset;
		}
		else {
			enemyOrg = enemyEnt->GetPhysics()->GetOrigin();
		}

		BeginAttack(damageDef);
		DirectMoveToPosition(enemyOrg);
		TurnToward(enemyOrg);
	}
}

/*
=====================
idAI::Native_TestChargeAttack
=====================
*/
float idAI::Native_TestChargeAttack(void) {
	trace_t trace;
	idActor* enemyEnt = enemy.GetEntity();
	predictedPath_t path;
	idVec3 end;

	if (!enemyEnt) {
		return 0.0f;
		return 0.0f;
	}

	if (move.moveType == MOVETYPE_FLY) {
		// position destination so that we're in the enemy's view
		end = enemyEnt->GetEyePosition();
		end -= enemyEnt->GetPhysics()->GetGravityNormal() * fly_offset;
	}
	else {
		end = enemyEnt->GetPhysics()->GetOrigin();
	}

	idAI::PredictPath(this, aas, physicsObj.GetOrigin(), end - physicsObj.GetOrigin(), 1000, 1000, (move.moveType == MOVETYPE_FLY) ? SE_BLOCKED : (SE_ENTER_OBSTACLE | SE_BLOCKED | SE_ENTER_LEDGE_AREA), path);

	if (ai_debugMove.GetBool()) {
		gameRenderWorld->DebugLine(colorGreen, physicsObj.GetOrigin(), end, gameLocal.msec);
		gameRenderWorld->DebugBounds(path.endEvent == 0 ? colorYellow : colorRed, physicsObj.GetBounds(), end, gameLocal.msec);
	}

	if ((path.endEvent == 0) || (path.blockingEntity == enemyEnt)) {
		idVec3 delta = end - physicsObj.GetOrigin();
		float time = delta.LengthFast();
		return time;
	}
	else {
		return 0.0f;
	}
}

/*
=====================
idAI::Native_TestAnimMoveTowardEnemy
=====================
*/
int idAI::Native_TestAnimMoveTowardEnemy(const char* animname) {
	int				anim;
	predictedPath_t path;
	idVec3			moveVec;
	float			yaw;
	idVec3			delta;
	idActor* enemyEnt;

	enemyEnt = enemy.GetEntity();
	if (!enemyEnt) {
		return false;
		return false;
	}

	anim = GetAnim(ANIMCHANNEL_LEGS, animname);
	if (!anim) {
		gameLocal.DWarning("missing '%s' animation on '%s' (%s)", animname, name.c_str(), GetEntityDefName());
		return false;
		return false;
	}

	delta = enemyEnt->GetPhysics()->GetOrigin() - physicsObj.GetOrigin();
	yaw = delta.ToYaw();

	moveVec = animator.TotalMovementDelta(anim) * idAngles(0.0f, yaw, 0.0f).ToMat3() * physicsObj.GetGravityAxis();
	idAI::PredictPath(this, aas, physicsObj.GetOrigin(), moveVec, 1000, 1000, (move.moveType == MOVETYPE_FLY) ? SE_BLOCKED : (SE_ENTER_OBSTACLE | SE_BLOCKED | SE_ENTER_LEDGE_AREA), path);

	if (ai_debugMove.GetBool()) {
		gameRenderWorld->DebugLine(colorGreen, physicsObj.GetOrigin(), physicsObj.GetOrigin() + moveVec, gameLocal.msec);
		gameRenderWorld->DebugBounds(path.endEvent == 0 ? colorYellow : colorRed, physicsObj.GetBounds(), physicsObj.GetOrigin() + moveVec, gameLocal.msec);
	}

	return path.endEvent == 0;
}

/*
=====================
idAI::Native_TestAnimMove
=====================
*/
int idAI::Native_TestAnimMove(const char* animname) {
	int				anim;
	predictedPath_t path;
	idVec3			moveVec;

	anim = GetAnim(ANIMCHANNEL_LEGS, animname);
	if (!anim) {
		gameLocal.DWarning("missing '%s' animation on '%s' (%s)", animname, name.c_str(), GetEntityDefName());
		return false;
		return false;
	}

	moveVec = animator.TotalMovementDelta(anim) * idAngles(0.0f, ideal_yaw, 0.0f).ToMat3() * physicsObj.GetGravityAxis();
	idAI::PredictPath(this, aas, physicsObj.GetOrigin(), moveVec, 1000, 1000, (move.moveType == MOVETYPE_FLY) ? SE_BLOCKED : (SE_ENTER_OBSTACLE | SE_BLOCKED | SE_ENTER_LEDGE_AREA), path);

	if (ai_debugMove.GetBool()) {
		gameRenderWorld->DebugLine(colorGreen, physicsObj.GetOrigin(), physicsObj.GetOrigin() + moveVec, gameLocal.msec);
		gameRenderWorld->DebugBounds(path.endEvent == 0 ? colorYellow : colorRed, physicsObj.GetBounds(), physicsObj.GetOrigin() + moveVec, gameLocal.msec);
	}

	return path.endEvent == 0;
}

/*
=====================
idAI::Native_TestMoveToPosition
=====================
*/
int idAI::Native_TestMoveToPosition(const idVec3& position) {
	predictedPath_t path;

	idAI::PredictPath(this, aas, physicsObj.GetOrigin(), position - physicsObj.GetOrigin(), 1000, 1000, (move.moveType == MOVETYPE_FLY) ? SE_BLOCKED : (SE_ENTER_OBSTACLE | SE_BLOCKED | SE_ENTER_LEDGE_AREA), path);

	if (ai_debugMove.GetBool()) {
		gameRenderWorld->DebugLine(colorGreen, physicsObj.GetOrigin(), position, gameLocal.msec);
		gameRenderWorld->DebugBounds(colorYellow, physicsObj.GetBounds(), position, gameLocal.msec);
		if (path.endEvent) {
			gameRenderWorld->DebugBounds(colorRed, physicsObj.GetBounds(), path.endPos, gameLocal.msec);
		}
	}

	return path.endEvent == 0;
}

/*
=====================
idAI::Native_TestMeleeAttack
=====================
*/
int idAI::Native_TestMeleeAttack(void) {
	bool result = TestMelee();
	return result;
}

/*
=====================
idAI::Native_TestAnimAttack
=====================
*/
int idAI::Native_TestAnimAttack(const char* animname) {
	int				anim;
	predictedPath_t path;

	anim = GetAnim(ANIMCHANNEL_LEGS, animname);
	if (!anim) {
		gameLocal.DWarning("missing '%s' animation on '%s' (%s)", animname, name.c_str(), GetEntityDefName());
		return false;
		return false;
	}

	idAI::PredictPath(this, aas, physicsObj.GetOrigin(), animator.TotalMovementDelta(anim), 1000, 1000, (move.moveType == MOVETYPE_FLY) ? SE_BLOCKED : (SE_ENTER_OBSTACLE | SE_BLOCKED | SE_ENTER_LEDGE_AREA), path);

	return path.blockingEntity && (path.blockingEntity == enemy.GetEntity());
}

/*
=====================
idAI::Native_PreBurn
=====================
*/
void idAI::Native_PreBurn(void) {
	// for now this just turns shadows off
	renderEntity.noShadow = true;
}

/*
=====================
idAI::Native_Burn
=====================
*/
void idAI::Native_Burn(void) {
	renderEntity.shaderParms[SHADERPARM_TIME_OF_DEATH] = gameLocal.time * 0.001f;
	SpawnParticles("smoke_burnParticleSystem");
	UpdateVisuals();
}

/*
=====================
idAI::Native_ClearBurn
=====================
*/
void idAI::Native_ClearBurn(void) {
	renderEntity.noShadow = spawnArgs.GetBool("noshadows");
	renderEntity.shaderParms[SHADERPARM_TIME_OF_DEATH] = 0.0f;
	UpdateVisuals();
}

/*
=====================
idAI::Native_SetSmokeVisibility
=====================
*/
void idAI::Native_SetSmokeVisibility(int num, int on) {
	int i;
	int time;

	if (num >= particles.Num()) {
		gameLocal.Warning("Particle #%d out of range (%d particles) on entity '%s'", num, particles.Num(), name.c_str());
		return;
	}

	if (on != 0) {
		time = gameLocal.time;
		BecomeActive(TH_UPDATEPARTICLES);
	}
	else {
		time = 0;
	}

	if (num >= 0) {
		particles[num].time = time;
	}
	else {
		for (i = 0; i < particles.Num(); i++) {
			particles[i].time = time;
		}
	}

	UpdateVisuals();
}

/*
=====================
idAI::Native_NumSmokeEmitters
=====================
*/
int idAI::Native_NumSmokeEmitters(void) {
	return particles.Num();
}

/*
=====================
idAI::Native_StopThinking
=====================
*/
void idAI::Native_StopThinking(void) {
	BecomeInactive(TH_THINK);
}

/*
=====================
idAI::Native_GetTurnDelta
=====================
*/
float idAI::Native_GetTurnDelta(void) {
	float amount;

	if (turnRate) {
		amount = idMath::AngleNormalize180(ideal_yaw - current_yaw);
		return amount;
	}
	else {
		return 0.0f;
	}
}

/*
=====================
idAI::Native_GetMoveType
=====================
*/
int idAI::Native_GetMoveType(void) {
	return move.moveType;
}

/*
=====================
idAI::Native_SetMoveType
=====================
*/
void idAI::Native_SetMoveType(int moveType) {
	if ((moveType < 0) || (moveType >= NUM_MOVETYPES)) {
		gameLocal.Error("Invalid movetype %d", moveType);
	}

	move.moveType = static_cast<moveType_t>(moveType);
	if (move.moveType == MOVETYPE_FLY) {
		travelFlags = TFL_WALK | TFL_AIR | TFL_FLY;
	}
	else {
		travelFlags = TFL_WALK | TFL_AIR;
	}
}

/*
=====================
idAI::Native_SaveMove
=====================
*/
void idAI::Native_SaveMove(void) {
	savedMove = move;
}

/*
=====================
idAI::Native_RestoreMove
=====================
*/
void idAI::Native_RestoreMove(void) {
	idVec3 goalPos;
	idVec3 dest;

	switch (savedMove.moveCommand) {
	case MOVE_NONE:
		StopMove(savedMove.moveStatus);
		break;

	case MOVE_FACE_ENEMY:
		FaceEnemy();
		break;

	case MOVE_FACE_ENTITY:
		FaceEntity(savedMove.goalEntity.GetEntity());
		break;

	case MOVE_TO_ENEMY:
		MoveToEnemy();
		break;

	case MOVE_TO_ENEMYHEIGHT:
		MoveToEnemyHeight();
		break;

	case MOVE_TO_ENTITY:
		MoveToEntity(savedMove.goalEntity.GetEntity());
		break;

	case MOVE_OUT_OF_RANGE:
		MoveOutOfRange(savedMove.goalEntity.GetEntity(), savedMove.range);
		break;

	case MOVE_TO_ATTACK_POSITION:
		MoveToAttackPosition(savedMove.goalEntity.GetEntity(), savedMove.anim);
		break;

	case MOVE_TO_COVER:
		MoveToCover(savedMove.goalEntity.GetEntity(), lastVisibleEnemyPos);
		break;

	case MOVE_TO_POSITION:
		MoveToPosition(savedMove.moveDest);
		break;

	case MOVE_TO_POSITION_DIRECT:
		DirectMoveToPosition(savedMove.moveDest);
		break;

	case MOVE_SLIDE_TO_POSITION:
		SlideToPosition(savedMove.moveDest, savedMove.duration);
		break;

	case MOVE_WANDER:
		WanderAround();
		break;
	}

	if (GetMovePos(goalPos)) {
		CheckObstacleAvoidance(goalPos, dest);
	}
}

/*
=====================
idAI::Native_AllowMovement
=====================
*/
void idAI::Native_AllowMovement(float flag) {
	allowMove = (flag != 0.0f);
}

/*
=====================
idAI::Native_JumpFrame
=====================
*/
void idAI::Native_JumpFrame(void) {
	AI_JUMP = true;
}

/*
=====================
idAI::Native_EnableClip
=====================
*/
void idAI::Native_EnableClip(void) {
	physicsObj.SetClipMask(MASK_MONSTERSOLID);
	disableGravity = false;
}

/*
=====================
idAI::Native_DisableClip
=====================
*/
void idAI::Native_DisableClip(void) {
	physicsObj.SetClipMask(0);
	disableGravity = true;
}

/*
=====================
idAI::Native_EnableGravity
=====================
*/
void idAI::Native_EnableGravity(void) {
	disableGravity = false;
}

/*
=====================
idAI::Native_DisableGravity
=====================
*/
void idAI::Native_DisableGravity(void) {
	disableGravity = true;
}

/*
=====================
idAI::Native_EnableAFPush
=====================
*/
void idAI::Native_EnableAFPush(void) {
	af_push_moveables = true;
}

/*
=====================
idAI::Native_DisableAFPush
=====================
*/
void idAI::Native_DisableAFPush(void) {
	af_push_moveables = false;
}

/*
=====================
idAI::Native_SetFlySpeed
=====================
*/
void idAI::Native_SetFlySpeed(float speed) {
	if (move.speed == fly_speed) {
		move.speed = speed;
	}
	fly_speed = speed;
}

/*
=====================
idAI::Native_SetFlyOffset
=====================
*/
void idAI::Native_SetFlyOffset(int offset) {
	fly_offset = offset;
}

/*
=====================
idAI::Native_ClearFlyOffset
=====================
*/
void idAI::Native_ClearFlyOffset(void) {
	spawnArgs.GetInt("fly_offset", "0", fly_offset);
}

/*
=====================
idAI::Native_GetClosestHiddenTarget
=====================
*/
idEntity* idAI::Native_GetClosestHiddenTarget(const char* type) {
	int	i;
	idEntity* ent;
	idEntity* bestEnt;
	float time;
	float bestTime;
	const idVec3& org = physicsObj.GetOrigin();
	idActor* enemyEnt = enemy.GetEntity();

	if (!enemyEnt) {
		// no enemy to hide from
		return NULL;
		return NULL;
	}

	if (targets.Num() == 1) {
		ent = targets[0].GetEntity();
		if (ent && idStr::Cmp(ent->GetEntityDefName(), type) == 0) {
			if (!EntityCanSeePos(enemyEnt, lastVisibleEnemyPos, ent->GetPhysics()->GetOrigin())) {
				return ent;
				return NULL;
			}
		}
		return NULL;
		return NULL;
	}

	bestEnt = NULL;
	bestTime = idMath::INFINITY;
	for (i = 0; i < targets.Num(); i++) {
		ent = targets[i].GetEntity();
		if (ent && idStr::Cmp(ent->GetEntityDefName(), type) == 0) {
			const idVec3& destOrg = ent->GetPhysics()->GetOrigin();
			time = TravelDistance(org, destOrg);
			if ((time >= 0.0f) && (time < bestTime)) {
				if (!EntityCanSeePos(enemyEnt, lastVisibleEnemyPos, destOrg)) {
					bestEnt = ent;
					bestTime = time;
				}
			}
		}
	}
	return bestEnt;
}

/*
=====================
idAI::Native_GetRandomTarget
=====================
*/
idEntity* idAI::Native_GetRandomTarget(const char* type) {
	int	i;
	int	num;
	int which;
	idEntity* ent;
	idEntity* ents[MAX_GENTITIES];

	num = 0;
	for (i = 0; i < targets.Num(); i++) {
		ent = targets[i].GetEntity();
		if (ent && idStr::Cmp(ent->GetEntityDefName(), type) == 0) {
			ents[num++] = ent;
			if (num >= MAX_GENTITIES) {
				break;
			}
		}
	}

	if (!num) {
		return NULL;
		return NULL;
	}

	which = gameLocal.random.RandomInt(num);
	return ents[which];
}

/*
=====================
idAI::Native_TravelDistanceToPoint
=====================
*/
float idAI::Native_TravelDistanceToPoint(const idVec3& pos) {
	float time;

	time = TravelDistance(physicsObj.GetOrigin(), pos);
	return time;
}

/*
=====================
idAI::Native_TravelDistanceToEntity
=====================
*/
float idAI::Native_TravelDistanceToEntity(idEntity* ent) {
	float time;

	time = TravelDistance(physicsObj.GetOrigin(), ent->GetPhysics()->GetOrigin());
	return time;
}

/*
=====================
idAI::Native_TravelDistanceBetweenPoints
=====================
*/
float idAI::Native_TravelDistanceBetweenPoints(const idVec3& source, const idVec3& dest) {
	float time;

	time = TravelDistance(source, dest);
	return time;
}

/*
=====================
idAI::Native_TravelDistanceBetweenEntities
=====================
*/
float idAI::Native_TravelDistanceBetweenEntities(idEntity* source, idEntity* dest) {
	float time;

	assert(source);
	assert(dest);
	time = TravelDistance(source->GetPhysics()->GetOrigin(), dest->GetPhysics()->GetOrigin());
	return time;
}

/*
=====================
idAI::Native_LookAtEntity
=====================
*/
void idAI::Native_LookAtEntity(idEntity* ent, float duration) {
	if (ent == this) {
		ent = NULL;
	}

	if ((ent != focusEntity.GetEntity()) || (focusTime < gameLocal.time)) {
		focusEntity = ent;
		alignHeadTime = gameLocal.time;
		forceAlignHeadTime = gameLocal.time + SEC2MS(1);
		blink_time = 0;
	}

	focusTime = gameLocal.time + SEC2MS(duration);
}

/*
=====================
idAI::Native_LookAtEnemy
=====================
*/
void idAI::Native_LookAtEnemy(float duration) {
	idActor* enemyEnt;

	enemyEnt = enemy.GetEntity();
	if ((enemyEnt != focusEntity.GetEntity()) || (focusTime < gameLocal.time)) {
		focusEntity = enemyEnt;
		alignHeadTime = gameLocal.time;
		forceAlignHeadTime = gameLocal.time + SEC2MS(1);
		blink_time = 0;
	}

	focusTime = gameLocal.time + SEC2MS(duration);
}

/*
=====================
idAI::Native_SetJointMod
=====================
*/
void idAI::Native_SetJointMod(int allow) {
	allowJointMod = (allow != 0);
}

/*
=====================
idAI::Native_ThrowMoveable
=====================
*/
void idAI::Native_ThrowMoveable(void) {
	idEntity* ent;
	idEntity* moveable = NULL;

	for (ent = GetNextTeamEntity(); ent != NULL; ent = ent->GetNextTeamEntity()) {
		if (ent->GetBindMaster() == this && ent->IsType(idMoveable::Type)) {
			moveable = ent;
			break;
		}
	}
	if (moveable) {
		moveable->Unbind();
		moveable->PostEventMS(&EV_SetOwner, 200, NULL);
	}
}

/*
=====================
idAI::Native_ThrowAF
=====================
*/
void idAI::Native_ThrowAF(void) {
	idEntity* ent;
	idEntity* af = NULL;

	for (ent = GetNextTeamEntity(); ent != NULL; ent = ent->GetNextTeamEntity()) {
		if (ent->GetBindMaster() == this && ent->IsType(idAFEntity_Base::Type)) {
			af = ent;
			break;
		}
	}
	if (af) {
		af->Unbind();
		af->PostEventMS(&EV_SetOwner, 200, NULL);
	}
}

/*
=====================
idAI::Native_SetAngles
=====================
*/
void idAI::Native_SetAngles(idAngles const& ang) {
	current_yaw = ang.yaw;
	viewAxis = idAngles(0, current_yaw, 0).ToMat3();
}

/*
=====================
idAI::Native_GetAngles
=====================
*/
idVec3 idAI::Native_GetAngles(void) {
	return idVec3(0.0f, current_yaw, 0.0f);
}

/*
=====================
idAI::Native_RealKill
=====================
*/
void idAI::Native_RealKill(void) {
	health = 0;

	if (af.IsLoaded()) {
		// clear impacts
		af.Rest();

		// physics is turned off by calling af.Rest()
		BecomeActive(TH_PHYSICS);
	}

	Killed(this, this, 0, vec3_zero, INVALID_JOINT);
}

/*
=====================
idAI::Native_Kill
=====================
*/
void idAI::Native_Kill(void) {
	PostEventMS(&AI_RealKill, 0);
}

/*
=====================
idAI::Native_WakeOnFlashlight
=====================
*/
void idAI::Native_WakeOnFlashlight(int enable) {
	wakeOnFlashlight = (enable != 0);
}

/*
=====================
idAI::Native_LocateEnemy
=====================
*/
void idAI::Native_LocateEnemy(void) {
	idActor* enemyEnt;
	int areaNum;

	enemyEnt = enemy.GetEntity();
	if (!enemyEnt) {
		return;
	}

	enemyEnt->GetAASLocation(aas, lastReachableEnemyPos, areaNum);
	SetEnemyPosition();
	UpdateEnemyPosition();
}

/*
=====================
idAI::Native_KickObstacles
=====================
*/
void idAI::Native_KickObstacles(idEntity* kickEnt, float force) {
	idVec3 dir;
	idEntity* obEnt;

	if (kickEnt) {
		obEnt = kickEnt;
	}
	else {
		obEnt = move.obstacle.GetEntity();
	}

	if (obEnt) {
		dir = obEnt->GetPhysics()->GetOrigin() - physicsObj.GetOrigin();
		dir.Normalize();
	}
	else {
		dir = viewAxis[0];
	}
	KickObstacles(dir, force, obEnt);
}

/*
=====================
idAI::Native_GetObstacle
=====================
*/
idEntity* idAI::Native_GetObstacle(void) {
	return move.obstacle.GetEntity();
}

/*
=====================
idAI::Native_PushPointIntoAAS
=====================
*/
idVec3 idAI::Native_PushPointIntoAAS(const idVec3& pos) {
	int		areaNum;
	idVec3	newPos;

	areaNum = PointReachableAreaNum(pos);
	if (areaNum) {
		newPos = pos;
		aas->PushPointIntoAreaNum(areaNum, newPos);
		return newPos;
	}
	else {
		return pos;
	}
}

/*
=====================
idAI::Native_GetTurnRate
=====================
*/
float idAI::Native_GetTurnRate(void) {
	return turnRate;
}

/*
=====================
idAI::Native_SetTurnRate
=====================
*/
void idAI::Native_SetTurnRate(float rate) {
	turnRate = rate;
}

/*
=====================
idAI::Native_AnimTurn
=====================
*/
void idAI::Native_AnimTurn(float angles) {
	turnVel = 0.0f;
	anim_turn_angles = angles;
	if (angles) {
		anim_turn_yaw = current_yaw;
		anim_turn_amount = idMath::Fabs(idMath::AngleNormalize180(current_yaw - ideal_yaw));
		if (anim_turn_amount > anim_turn_angles) {
			anim_turn_amount = anim_turn_angles;
		}
	}
	else {
		anim_turn_amount = 0.0f;
		animator.CurrentAnim(ANIMCHANNEL_LEGS)->SetSyncedAnimWeight(0, 1.0f);
		animator.CurrentAnim(ANIMCHANNEL_LEGS)->SetSyncedAnimWeight(1, 0.0f);
		animator.CurrentAnim(ANIMCHANNEL_TORSO)->SetSyncedAnimWeight(0, 1.0f);
		animator.CurrentAnim(ANIMCHANNEL_TORSO)->SetSyncedAnimWeight(1, 0.0f);
	}
}

/*
=====================
idAI::Native_AllowHiddenMovement
=====================
*/
void idAI::Native_AllowHiddenMovement(int enable) {
	allowHiddenMovement = (enable != 0);
}

/*
=====================
idAI::Native_TriggerParticles
=====================
*/
void idAI::Native_TriggerParticles(const char* jointName) {
	TriggerParticles(jointName);
}

/*
=====================
idAI::Native_FindActorsInBounds
=====================
*/
idEntity* idAI::Native_FindActorsInBounds(const idVec3& mins, const idVec3& maxs) {
	idEntity* ent;
	idEntity* entityList[MAX_GENTITIES];
	int			numListedEntities;
	int			i;

	numListedEntities = gameLocal.clip.EntitiesTouchingBounds(idBounds(mins, maxs), CONTENTS_BODY, entityList, MAX_GENTITIES);
	for (i = 0; i < numListedEntities; i++) {
		ent = entityList[i];
		if (ent != this && !ent->IsHidden() && (ent->health > 0) && ent->IsType(idActor::Type)) {
			return ent;
			return NULL;
		}
	}

	return NULL;
}

/*
=====================
idAI::Native_CanReachPosition
=====================
*/
int idAI::Native_CanReachPosition(const idVec3& pos) {
	aasPath_t	path;
	int			toAreaNum;
	int			areaNum;

	toAreaNum = PointReachableAreaNum(pos);
	areaNum = PointReachableAreaNum(physicsObj.GetOrigin());
	if (!toAreaNum || !PathToGoal(path, areaNum, physicsObj.GetOrigin(), toAreaNum, pos)) {
		return false;
	}
	else {
		return true;
	}
}

/*
=====================
idAI::Native_CanReachEntity
=====================
*/
int idAI::Native_CanReachEntity(idEntity* ent) {
	aasPath_t	path;
	int			toAreaNum;
	int			areaNum;
	idVec3		pos;

	if (!ent) {
		return false;
		return false;
	}

	if (move.moveType != MOVETYPE_FLY) {
		if (!ent->GetFloorPos(64.0f, pos)) {
			return false;
			return false;
		}
		if (ent->IsType(idActor::Type) && static_cast<idActor*>(ent)->OnLadder()) {
			return false;
			return false;
		}
	}
	else {
		pos = ent->GetPhysics()->GetOrigin();
	}

	toAreaNum = PointReachableAreaNum(pos);
	if (!toAreaNum) {
		return false;
		return false;
	}

	const idVec3& org = physicsObj.GetOrigin();
	areaNum = PointReachableAreaNum(org);
	if (!toAreaNum || !PathToGoal(path, areaNum, org, toAreaNum, pos)) {
		return false;
	}
	else {
		return true;
	}
}

/*
=====================
idAI::Native_CanReachEnemy
=====================
*/
int idAI::Native_CanReachEnemy(void) {
	aasPath_t	path;
	int			toAreaNum;
	int			areaNum;
	idVec3		pos;
	idActor* enemyEnt;

	enemyEnt = enemy.GetEntity();
	if (!enemyEnt) {
		return false;
		return false;
	}

	if (move.moveType != MOVETYPE_FLY) {
		if (enemyEnt->OnLadder()) {
			return false;
			return false;
		}
		enemyEnt->GetAASLocation(aas, pos, toAreaNum);
	}
	else {
		pos = enemyEnt->GetPhysics()->GetOrigin();
		toAreaNum = PointReachableAreaNum(pos);
	}

	if (!toAreaNum) {
		return false;
		return false;
	}

	const idVec3& org = physicsObj.GetOrigin();
	areaNum = PointReachableAreaNum(org);
	if (!PathToGoal(path, areaNum, org, toAreaNum, pos)) {
		return false;
	}
	else {
		return true;
	}
}

/*
=====================
idAI::Native_GetReachableEntityPosition
=====================
*/
idVec3 idAI::Native_GetReachableEntityPosition(idEntity* ent) {
	int		toAreaNum;
	idVec3	pos;

	if (move.moveType != MOVETYPE_FLY) {
		if (!ent->GetFloorPos(64.0f, pos)) {
			// NOTE: not a good way to return 'false'
			return vec3_zero;
		}
		if (ent->IsType(idActor::Type) && static_cast<idActor*>(ent)->OnLadder()) {
			// NOTE: not a good way to return 'false'
			return vec3_zero;
		}
	}
	else {
		pos = ent->GetPhysics()->GetOrigin();
	}

	if (aas) {
		toAreaNum = PointReachableAreaNum(pos);
		aas->PushPointIntoAreaNum(toAreaNum, pos);
	}

	return pos;
}

/*
=====================
idAI::Native_WaitAction
=====================
*/
void idAI::Native_WaitAction(const char* waitForState) {
	SetWaitState(waitForState);
}

/*
=====================
idAI::Native_WaitActionDone
=====================
*/
bool idAI::Native_WaitActionDone(void) const {
	return (WaitState() == NULL);
}

/*
=====================
idAI::Native_WaitMoveDone
=====================
*/
bool idAI::Native_WaitMoveDone(void) const {
	return MoveDone();
}

/*
=====================
idAI::Native_Shrivel
=====================
*/
bool idAI::Native_Shrivel(float shrivel_time, bool firstFrame) {
	float t;

	if (firstFrame) {
		if (shrivel_time <= 0.0f) {
			return true;
		}

		shrivel_rate = 0.001f / shrivel_time;
		shrivel_start = gameLocal.time;
	}

	t = (gameLocal.time - shrivel_start) * shrivel_rate;
	if (t > 0.25f) {
		renderEntity.noShadow = true;
	}
	if (t > 1.0f) {
		t = 1.0f;
		renderEntity.shaderParms[SHADERPARM_MD5_SKINSCALE] = 1.0f - t * 0.5f;
		UpdateVisuals();
		return true;
	}

	renderEntity.shaderParms[SHADERPARM_MD5_SKINSCALE] = 1.0f - t * 0.5f;
	UpdateVisuals();
	return false;
}

/*
=====================
idAI::Event_Activate
=====================
*/
void idAI::Event_Activate(idEntity* activator) {
	Native_Activate(activator);
}

/*
=====================
idAI::Event_Touch
=====================
*/
void idAI::Event_Touch(idEntity* other, trace_t* trace) {
	Native_Touch(other, trace);
}

/*
=====================
idAI::Event_FindEnemy
=====================
*/
void idAI::Event_FindEnemy(int useFOV) {
	idThread::ReturnEntity(Native_FindEnemy(useFOV));
}

/*
=====================
idAI::Event_FindEnemyAI
=====================
*/
void idAI::Event_FindEnemyAI(int useFOV) {
	idThread::ReturnEntity(Native_FindEnemyAI(useFOV));
}

/*
=====================
idAI::Event_FindEnemyInCombatNodes
=====================
*/
void idAI::Event_FindEnemyInCombatNodes(void) {
	idThread::ReturnEntity(Native_FindEnemyInCombatNodes());
}

/*
=====================
idAI::Event_ClosestReachableEnemyOfEntity
=====================
*/
void idAI::Event_ClosestReachableEnemyOfEntity(idEntity* team_mate) {
	idThread::ReturnEntity(Native_ClosestReachableEnemyOfEntity(team_mate));
}

/*
=====================
idAI::Event_HeardSound
=====================
*/
void idAI::Event_HeardSound(int ignore_team) {
	idThread::ReturnEntity(Native_HeardSound(ignore_team));
}

/*
=====================
idAI::Event_SetEnemy
=====================
*/
void idAI::Event_SetEnemy(idEntity* ent) {
	Native_SetEnemy(ent);
}

/*
=====================
idAI::Event_ClearEnemy
=====================
*/
void idAI::Event_ClearEnemy(void) {
	Native_ClearEnemy();
}

/*
=====================
idAI::Event_MuzzleFlash
=====================
*/
void idAI::Event_MuzzleFlash(const char* jointname) {
	Native_MuzzleFlash(jointname);
}

/*
=====================
idAI::Event_CreateMissile
=====================
*/
void idAI::Event_CreateMissile(const char* jointname) {
	idThread::ReturnEntity(Native_CreateMissile(jointname));
}

/*
=====================
idAI::Event_AttackMissile
=====================
*/
void idAI::Event_AttackMissile(const char* jointname) {
	idThread::ReturnEntity(Native_AttackMissile(jointname));
}

/*
=====================
idAI::Event_FireMissileAtTarget
=====================
*/
void idAI::Event_FireMissileAtTarget(const char* jointname, const char* targetname) {
	idThread::ReturnEntity(Native_FireMissileAtTarget(jointname, targetname));
}

/*
=====================
idAI::Event_LaunchMissile
=====================
*/
void idAI::Event_LaunchMissile(const idVec3& org, const idAngles& ang) {
	idThread::ReturnEntity(Native_LaunchMissile(org, ang));
}

/*
=====================
idAI::Event_AttackMelee
=====================
*/
void idAI::Event_AttackMelee(const char* meleeDefName) {
	idThread::ReturnInt(Native_AttackMelee(meleeDefName));
}

/*
=====================
idAI::Event_DirectDamage
=====================
*/
void idAI::Event_DirectDamage(idEntity* damageTarget, const char* damageDefName) {
	Native_DirectDamage(damageTarget, damageDefName);
}

/*
=====================
idAI::Event_RadiusDamageFromJoint
=====================
*/
void idAI::Event_RadiusDamageFromJoint(const char* jointname, const char* damageDefName) {
	Native_RadiusDamageFromJoint(jointname, damageDefName);
}

/*
=====================
idAI::Event_RandomPath
=====================
*/
void idAI::Event_RandomPath(void) {
	idThread::ReturnEntity(Native_RandomPath());
}

/*
=====================
idAI::Event_BeginAttack
=====================
*/
void idAI::Event_BeginAttack(const char* name) {
	Native_BeginAttack(name);
}

/*
=====================
idAI::Event_EndAttack
=====================
*/
void idAI::Event_EndAttack(void) {
	Native_EndAttack();
}

/*
=====================
idAI::Event_MeleeAttackToJoint
=====================
*/
void idAI::Event_MeleeAttackToJoint(const char* jointname, const char* meleeDefName) {
	idThread::ReturnInt(Native_MeleeAttackToJoint(jointname, meleeDefName));
}

/*
=====================
idAI::Event_CanBecomeSolid
=====================
*/
void idAI::Event_CanBecomeSolid(void) {
	idThread::ReturnFloat(Native_CanBecomeSolid());
}

/*
=====================
idAI::Event_BecomeSolid
=====================
*/
void idAI::Event_BecomeSolid(void) {
	Native_BecomeSolid();
}

/*
=====================
idAI::Event_BecomeNonSolid
=====================
*/
void idAI::Event_BecomeNonSolid(void) {
	Native_BecomeNonSolid();
}

/*
=====================
idAI::Event_BecomeRagdoll
=====================
*/
void idAI::Event_BecomeRagdoll(void) {
	idThread::ReturnInt(Native_BecomeRagdoll());
}

/*
=====================
idAI::Event_StopRagdoll
=====================
*/
void idAI::Event_StopRagdoll(void) {
	Native_StopRagdoll();
}

/*
=====================
idAI::Event_SetHealth
=====================
*/
void idAI::Event_SetHealth(float newHealth) {
	Native_SetHealth(newHealth);
}

/*
=====================
idAI::Event_GetHealth
=====================
*/
void idAI::Event_GetHealth(void) {
	idThread::ReturnFloat(Native_GetHealth());
}

/*
=====================
idAI::Event_AllowDamage
=====================
*/
void idAI::Event_AllowDamage(void) {
	Native_AllowDamage();
}

/*
=====================
idAI::Event_IgnoreDamage
=====================
*/
void idAI::Event_IgnoreDamage(void) {
	Native_IgnoreDamage();
}

/*
=====================
idAI::Event_GetCurrentYaw
=====================
*/
void idAI::Event_GetCurrentYaw(void) {
	idThread::ReturnFloat(Native_GetCurrentYaw());
}

/*
=====================
idAI::Event_TurnTo
=====================
*/
void idAI::Event_TurnTo(float angle) {
	Native_TurnTo(angle);
}

/*
=====================
idAI::Event_TurnToPos
=====================
*/
void idAI::Event_TurnToPos(const idVec3& pos) {
	Native_TurnToPos(pos);
}

/*
=====================
idAI::Event_TurnToEntity
=====================
*/
void idAI::Event_TurnToEntity(idEntity* ent) {
	Native_TurnToEntity(ent);
}

/*
=====================
idAI::Event_MoveStatus
=====================
*/
void idAI::Event_MoveStatus(void) {
	idThread::ReturnInt(Native_MoveStatus());
}

/*
=====================
idAI::Event_StopMove
=====================
*/
void idAI::Event_StopMove(void) {
	Native_StopMove();
}

/*
=====================
idAI::Event_MoveToCover
=====================
*/
void idAI::Event_MoveToCover(void) {
	Native_MoveToCover();
}

/*
=====================
idAI::Event_MoveToEnemy
=====================
*/
void idAI::Event_MoveToEnemy(void) {
	Native_MoveToEnemy();
}

/*
=====================
idAI::Event_MoveToEnemyHeight
=====================
*/
void idAI::Event_MoveToEnemyHeight(void) {
	Native_MoveToEnemyHeight();
}

/*
=====================
idAI::Event_MoveOutOfRange
=====================
*/
void idAI::Event_MoveOutOfRange(idEntity* entity, float range) {
	Native_MoveOutOfRange(entity, range);
}

/*
=====================
idAI::Event_MoveToAttackPosition
=====================
*/
void idAI::Event_MoveToAttackPosition(idEntity* entity, const char* attack_anim) {
	Native_MoveToAttackPosition(entity, attack_anim);
}

/*
=====================
idAI::Event_MoveToEntity
=====================
*/
void idAI::Event_MoveToEntity(idEntity* ent) {
	Native_MoveToEntity(ent);
}

/*
=====================
idAI::Event_MoveToPosition
=====================
*/
void idAI::Event_MoveToPosition(const idVec3& pos) {
	Native_MoveToPosition(pos);
}

/*
=====================
idAI::Event_SlideTo
=====================
*/
void idAI::Event_SlideTo(const idVec3& pos, float time) {
	Native_SlideTo(pos, time);
}

/*
=====================
idAI::Event_Wander
=====================
*/
void idAI::Event_Wander(void) {
	Native_Wander();
}

/*
=====================
idAI::Event_FacingIdeal
=====================
*/
void idAI::Event_FacingIdeal(void) {
	idThread::ReturnInt(Native_FacingIdeal());
}

/*
=====================
idAI::Event_FaceEnemy
=====================
*/
void idAI::Event_FaceEnemy(void) {
	Native_FaceEnemy();
}

/*
=====================
idAI::Event_FaceEntity
=====================
*/
void idAI::Event_FaceEntity(idEntity* ent) {
	Native_FaceEntity(ent);
}

/*
=====================
idAI::Event_WaitAction
=====================
*/
void idAI::Event_WaitAction(const char* waitForState) {
	if (idThread::BeginMultiFrameEvent(this, &AI_WaitAction)) {
		Native_WaitAction(waitForState);
	}

	if (Native_WaitActionDone()) {
		idThread::EndMultiFrameEvent(this, &AI_WaitAction);
	}
}

/*
=====================
idAI::Event_GetCombatNode
=====================
*/
void idAI::Event_GetCombatNode(void) {
	idThread::ReturnEntity(Native_GetCombatNode());
}

/*
=====================
idAI::Event_EnemyInCombatCone
=====================
*/
void idAI::Event_EnemyInCombatCone(idEntity* ent, int use_current_enemy_location) {
	idThread::ReturnInt(Native_EnemyInCombatCone(ent, use_current_enemy_location));
}

/*
=====================
idAI::Event_WaitMove
=====================
*/
void idAI::Event_WaitMove(void) {
	idThread::BeginMultiFrameEvent(this, &AI_WaitMove);

	if (Native_WaitMoveDone()) {
		idThread::EndMultiFrameEvent(this, &AI_WaitMove);
	}
}

/*
=====================
idAI::Event_GetJumpVelocity
=====================
*/
void idAI::Event_GetJumpVelocity(const idVec3& pos, float speed, float max_height) {
	idThread::ReturnVector(Native_GetJumpVelocity(pos, speed, max_height));
}

/*
=====================
idAI::Event_EntityInAttackCone
=====================
*/
void idAI::Event_EntityInAttackCone(idEntity* ent) {
	idThread::ReturnInt(Native_EntityInAttackCone(ent));
}

/*
=====================
idAI::Event_CanSeeEntity
=====================
*/
void idAI::Event_CanSeeEntity(idEntity* ent) {
	idThread::ReturnInt(Native_CanSeeEntity(ent));
}

/*
=====================
idAI::Event_SetTalkTarget
=====================
*/
void idAI::Event_SetTalkTarget(idEntity* target) {
	Native_SetTalkTarget(target);
}

/*
=====================
idAI::Event_GetTalkTarget
=====================
*/
void idAI::Event_GetTalkTarget(void) {
	idThread::ReturnEntity(Native_GetTalkTarget());
}

/*
=====================
idAI::Event_SetTalkState
=====================
*/
void idAI::Event_SetTalkState(int state) {
	Native_SetTalkState(state);
}

/*
=====================
idAI::Event_EnemyRange
=====================
*/
void idAI::Event_EnemyRange(void) {
	idThread::ReturnFloat(Native_EnemyRange());
}

/*
=====================
idAI::Event_EnemyRange2D
=====================
*/
void idAI::Event_EnemyRange2D(void) {
	idThread::ReturnFloat(Native_EnemyRange2D());
}

/*
=====================
idAI::Event_GetEnemy
=====================
*/
void idAI::Event_GetEnemy(void) {
	idThread::ReturnEntity(Native_GetEnemy());
}

/*
=====================
idAI::Event_GetEnemyPos
=====================
*/
void idAI::Event_GetEnemyPos(void) {
	idThread::ReturnVector(Native_GetEnemyPos());
}

/*
=====================
idAI::Event_GetEnemyEyePos
=====================
*/
void idAI::Event_GetEnemyEyePos(void) {
	idThread::ReturnVector(Native_GetEnemyEyePos());
}

/*
=====================
idAI::Event_PredictEnemyPos
=====================
*/
void idAI::Event_PredictEnemyPos(float time) {
	idThread::ReturnVector(Native_PredictEnemyPos(time));
}

/*
=====================
idAI::Event_CanHitEnemy
=====================
*/
void idAI::Event_CanHitEnemy(void) {
	idThread::ReturnInt(Native_CanHitEnemy());
}

/*
=====================
idAI::Event_CanHitEnemyFromAnim
=====================
*/
void idAI::Event_CanHitEnemyFromAnim(const char* animname) {
	idThread::ReturnInt(Native_CanHitEnemyFromAnim(animname));
}

/*
=====================
idAI::Event_CanHitEnemyFromJoint
=====================
*/
void idAI::Event_CanHitEnemyFromJoint(const char* jointname) {
	idThread::ReturnInt(Native_CanHitEnemyFromJoint(jointname));
}

/*
=====================
idAI::Event_EnemyPositionValid
=====================
*/
void idAI::Event_EnemyPositionValid(void) {
	idThread::ReturnInt(Native_EnemyPositionValid());
}

/*
=====================
idAI::Event_ChargeAttack
=====================
*/
void idAI::Event_ChargeAttack(const char* damageDef) {
	Native_ChargeAttack(damageDef);
}

/*
=====================
idAI::Event_TestChargeAttack
=====================
*/
void idAI::Event_TestChargeAttack(void) {
	idThread::ReturnFloat(Native_TestChargeAttack());
}

/*
=====================
idAI::Event_TestAnimMoveTowardEnemy
=====================
*/
void idAI::Event_TestAnimMoveTowardEnemy(const char* animname) {
	idThread::ReturnInt(Native_TestAnimMoveTowardEnemy(animname));
}

/*
=====================
idAI::Event_TestAnimMove
=====================
*/
void idAI::Event_TestAnimMove(const char* animname) {
	idThread::ReturnInt(Native_TestAnimMove(animname));
}

/*
=====================
idAI::Event_TestMoveToPosition
=====================
*/
void idAI::Event_TestMoveToPosition(const idVec3& position) {
	idThread::ReturnInt(Native_TestMoveToPosition(position));
}

/*
=====================
idAI::Event_TestMeleeAttack
=====================
*/
void idAI::Event_TestMeleeAttack(void) {
	idThread::ReturnInt(Native_TestMeleeAttack());
}

/*
=====================
idAI::Event_TestAnimAttack
=====================
*/
void idAI::Event_TestAnimAttack(const char* animname) {
	idThread::ReturnInt(Native_TestAnimAttack(animname));
}

/*
=====================
idAI::Event_Shrivel
=====================
*/
void idAI::Event_Shrivel(float shrivel_time) {
	const bool firstFrame = idThread::BeginMultiFrameEvent(this, &AI_Shrivel);
	if (Native_Shrivel(shrivel_time, firstFrame)) {
		idThread::EndMultiFrameEvent(this, &AI_Shrivel);
	}
}

/*
=====================
idAI::Event_PreBurn
=====================
*/
void idAI::Event_PreBurn(void) {
	Native_PreBurn();
}

/*
=====================
idAI::Event_Burn
=====================
*/
void idAI::Event_Burn(void) {
	Native_Burn();
}

/*
=====================
idAI::Event_ClearBurn
=====================
*/
void idAI::Event_ClearBurn(void) {
	Native_ClearBurn();
}

/*
=====================
idAI::Event_SetSmokeVisibility
=====================
*/
void idAI::Event_SetSmokeVisibility(int num, int on) {
	Native_SetSmokeVisibility(num, on);
}

/*
=====================
idAI::Event_NumSmokeEmitters
=====================
*/
void idAI::Event_NumSmokeEmitters(void) {
	idThread::ReturnInt(Native_NumSmokeEmitters());
}

/*
=====================
idAI::Event_StopThinking
=====================
*/
void idAI::Event_StopThinking(void) {
	Native_StopThinking();
	idThread* thread = idThread::CurrentThread();
	if (thread) {
		thread->DoneProcessing();
	}
}

/*
=====================
idAI::Event_GetTurnDelta
=====================
*/
void idAI::Event_GetTurnDelta(void) {
	idThread::ReturnFloat(Native_GetTurnDelta());
}

/*
=====================
idAI::Event_GetMoveType
=====================
*/
void idAI::Event_GetMoveType(void) {
	idThread::ReturnInt(Native_GetMoveType());
}

/*
=====================
idAI::Event_SetMoveType
=====================
*/
void idAI::Event_SetMoveType(int moveType) {
	Native_SetMoveType(moveType);
}

/*
=====================
idAI::Event_SaveMove
=====================
*/
void idAI::Event_SaveMove(void) {
	Native_SaveMove();
}

/*
=====================
idAI::Event_RestoreMove
=====================
*/
void idAI::Event_RestoreMove(void) {
	Native_RestoreMove();
}

/*
=====================
idAI::Event_AllowMovement
=====================
*/
void idAI::Event_AllowMovement(float flag) {
	Native_AllowMovement(flag);
}

/*
=====================
idAI::Event_JumpFrame
=====================
*/
void idAI::Event_JumpFrame(void) {
	Native_JumpFrame();
}

/*
=====================
idAI::Event_EnableClip
=====================
*/
void idAI::Event_EnableClip(void) {
	Native_EnableClip();
}

/*
=====================
idAI::Event_DisableClip
=====================
*/
void idAI::Event_DisableClip(void) {
	Native_DisableClip();
}

/*
=====================
idAI::Event_EnableGravity
=====================
*/
void idAI::Event_EnableGravity(void) {
	Native_EnableGravity();
}

/*
=====================
idAI::Event_DisableGravity
=====================
*/
void idAI::Event_DisableGravity(void) {
	Native_DisableGravity();
}

/*
=====================
idAI::Event_EnableAFPush
=====================
*/
void idAI::Event_EnableAFPush(void) {
	Native_EnableAFPush();
}

/*
=====================
idAI::Event_DisableAFPush
=====================
*/
void idAI::Event_DisableAFPush(void) {
	Native_DisableAFPush();
}

/*
=====================
idAI::Event_SetFlySpeed
=====================
*/
void idAI::Event_SetFlySpeed(float speed) {
	Native_SetFlySpeed(speed);
}

/*
=====================
idAI::Event_SetFlyOffset
=====================
*/
void idAI::Event_SetFlyOffset(int offset) {
	Native_SetFlyOffset(offset);
}

/*
=====================
idAI::Event_ClearFlyOffset
=====================
*/
void idAI::Event_ClearFlyOffset(void) {
	Native_ClearFlyOffset();
}

/*
=====================
idAI::Event_GetClosestHiddenTarget
=====================
*/
void idAI::Event_GetClosestHiddenTarget(const char* type) {
	idThread::ReturnEntity(Native_GetClosestHiddenTarget(type));
}

/*
=====================
idAI::Event_GetRandomTarget
=====================
*/
void idAI::Event_GetRandomTarget(const char* type) {
	idThread::ReturnEntity(Native_GetRandomTarget(type));
}

/*
=====================
idAI::Event_TravelDistanceToPoint
=====================
*/
void idAI::Event_TravelDistanceToPoint(const idVec3& pos) {
	idThread::ReturnFloat(Native_TravelDistanceToPoint(pos));
}

/*
=====================
idAI::Event_TravelDistanceToEntity
=====================
*/
void idAI::Event_TravelDistanceToEntity(idEntity* ent) {
	idThread::ReturnFloat(Native_TravelDistanceToEntity(ent));
}

/*
=====================
idAI::Event_TravelDistanceBetweenPoints
=====================
*/
void idAI::Event_TravelDistanceBetweenPoints(const idVec3& source, const idVec3& dest) {
	idThread::ReturnFloat(Native_TravelDistanceBetweenPoints(source, dest));
}

/*
=====================
idAI::Event_TravelDistanceBetweenEntities
=====================
*/
void idAI::Event_TravelDistanceBetweenEntities(idEntity* source, idEntity* dest) {
	idThread::ReturnFloat(Native_TravelDistanceBetweenEntities(source, dest));
}

/*
=====================
idAI::Event_LookAtEntity
=====================
*/
void idAI::Event_LookAtEntity(idEntity* ent, float duration) {
	Native_LookAtEntity(ent, duration);
}

/*
=====================
idAI::Event_LookAtEnemy
=====================
*/
void idAI::Event_LookAtEnemy(float duration) {
	Native_LookAtEnemy(duration);
}

/*
=====================
idAI::Event_SetJointMod
=====================
*/
void idAI::Event_SetJointMod(int allow) {
	Native_SetJointMod(allow);
}

/*
=====================
idAI::Event_ThrowMoveable
=====================
*/
void idAI::Event_ThrowMoveable(void) {
	Native_ThrowMoveable();
}

/*
=====================
idAI::Event_ThrowAF
=====================
*/
void idAI::Event_ThrowAF(void) {
	Native_ThrowAF();
}

/*
=====================
idAI::Event_SetAngles
=====================
*/
void idAI::Event_SetAngles(idAngles const& ang) {
	Native_SetAngles(ang);
}

/*
=====================
idAI::Event_GetAngles
=====================
*/
void idAI::Event_GetAngles(void) {
	idThread::ReturnVector(Native_GetAngles());
}

/*
=====================
idAI::Event_RealKill
=====================
*/
void idAI::Event_RealKill(void) {
	Native_RealKill();
}

/*
=====================
idAI::Event_Kill
=====================
*/
void idAI::Event_Kill(void) {
	Native_Kill();
}

/*
=====================
idAI::Event_WakeOnFlashlight
=====================
*/
void idAI::Event_WakeOnFlashlight(int enable) {
	Native_WakeOnFlashlight(enable);
}

/*
=====================
idAI::Event_LocateEnemy
=====================
*/
void idAI::Event_LocateEnemy(void) {
	Native_LocateEnemy();
}

/*
=====================
idAI::Event_KickObstacles
=====================
*/
void idAI::Event_KickObstacles(idEntity* kickEnt, float force) {
	Native_KickObstacles(kickEnt, force);
}

/*
=====================
idAI::Event_GetObstacle
=====================
*/
void idAI::Event_GetObstacle(void) {
	idThread::ReturnEntity(Native_GetObstacle());
}

/*
=====================
idAI::Event_PushPointIntoAAS
=====================
*/
void idAI::Event_PushPointIntoAAS(const idVec3& pos) {
	idThread::ReturnVector(Native_PushPointIntoAAS(pos));
}

/*
=====================
idAI::Event_GetTurnRate
=====================
*/
void idAI::Event_GetTurnRate(void) {
	idThread::ReturnFloat(Native_GetTurnRate());
}

/*
=====================
idAI::Event_SetTurnRate
=====================
*/
void idAI::Event_SetTurnRate(float rate) {
	Native_SetTurnRate(rate);
}

/*
=====================
idAI::Event_AnimTurn
=====================
*/
void idAI::Event_AnimTurn(float angles) {
	Native_AnimTurn(angles);
}

/*
=====================
idAI::Event_AllowHiddenMovement
=====================
*/
void idAI::Event_AllowHiddenMovement(int enable) {
	Native_AllowHiddenMovement(enable);
}

/*
=====================
idAI::Event_TriggerParticles
=====================
*/
void idAI::Event_TriggerParticles(const char* jointName) {
	Native_TriggerParticles(jointName);
}

/*
=====================
idAI::Event_FindActorsInBounds
=====================
*/
void idAI::Event_FindActorsInBounds(const idVec3& mins, const idVec3& maxs) {
	idThread::ReturnEntity(Native_FindActorsInBounds(mins, maxs));
}

/*
=====================
idAI::Event_CanReachPosition
=====================
*/
void idAI::Event_CanReachPosition(const idVec3& pos) {
	idThread::ReturnInt(Native_CanReachPosition(pos));
}

/*
=====================
idAI::Event_CanReachEntity
=====================
*/
void idAI::Event_CanReachEntity(idEntity* ent) {
	idThread::ReturnInt(Native_CanReachEntity(ent));
}

/*
=====================
idAI::Event_CanReachEnemy
=====================
*/
void idAI::Event_CanReachEnemy(void) {
	idThread::ReturnInt(Native_CanReachEnemy());
}

/*
=====================
idAI::Event_GetReachableEntityPosition
=====================
*/
void idAI::Event_GetReachableEntityPosition(idEntity* ent) {
	idThread::ReturnVector(Native_GetReachableEntityPosition(ent));
}

