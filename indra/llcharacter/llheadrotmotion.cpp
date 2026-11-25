/**
 * @file llheadrotmotion.cpp
 * @brief Implementation of LLHeadRotMotion class.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "linden_common.h"

#include "llheadrotmotion.h"
#include "llcharacter.h"
#include "llrand.h"
#include "m3math.h"
#include "v3dmath.h"
#include "llcriticaldamp.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const F32 TORSO_LAG = 0.35f;    // torso rotation factor
const F32 IDLE_HEAD_UPDATE_MIN = 2.5f;
const F32 IDLE_HEAD_UPDATE_MAX = 5.5f;
const F32 IDLE_HEAD_MAX_YAW = 1.5f * DEG_TO_RAD;
const F32 IDLE_HEAD_MAX_PITCH = 0.7f * DEG_TO_RAD;
const F32 IDLE_HEAD_BLEND_HALF_LIFE = 0.8f;
const F32 NECK_LAG = 0.5f;      // neck rotation factor
const F32 HEAD_LOOKAT_LAG_HALF_LIFE = 0.15f;        // half-life of lookat targeting for head
const F32 TORSO_LOOKAT_LAG_HALF_LIFE    = 0.27f;        // half-life of lookat targeting for torso
const F32 HEAD_ROTATION_CONSTRAINT = F_PI_BY_TWO * 0.8f;    // limit angle for head rotation
const F32 MIN_HEAD_LOOKAT_DISTANCE = 0.3f;  // minimum distance from head before we turn to look at it
const F32 EYE_JITTER_MIN_TIME = 0.3f; // min amount of time between eye "jitter" motions
const F32 EYE_JITTER_MAX_TIME = 2.5f; // max amount of time between eye "jitter" motions
const F32 EYE_JITTER_MAX_YAW = 0.08f; // max yaw of eye jitter motion
const F32 EYE_JITTER_MAX_PITCH = 0.015f; // max pitch of eye jitter motion
const F32 EYE_LOOK_AWAY_MIN_TIME = 5.f; // min amount of time between eye "look away" motions
const F32 EYE_LOOK_AWAY_MAX_TIME = 15.f; // max amount of time between eye "look away" motions
const F32 EYE_LOOK_BACK_MIN_TIME = 1.f; // min amount of time before looking back after looking away
const F32 EYE_LOOK_BACK_MAX_TIME = 5.f; // max amount of time before looking back after looking away
const F32 EYE_LOOK_AWAY_MAX_YAW = 0.15f; // max yaw of eye look away motion
const F32 EYE_LOOK_AWAY_MAX_PITCH = 0.12f; // max pitch of look away motion
const F32 EYE_ROT_LIMIT_ANGLE = F_PI_BY_TWO * 0.3f; //max angle in radians for eye rotation
const F32 EYE_LOOKAT_LAG_HALF_LIFE = 0.06f; // eyes lead head, but ease into a changed target

const F32 EYE_BLINK_MIN_TIME = 2.2f; // ordinary blink interval lower bound
const F32 EYE_BLINK_MAX_TIME = 6.8f; // ordinary blink interval upper bound
const F32 EYE_BLINK_DOUBLE_MIN = 0.55f; // occasional natural double-blink interval
const F32 EYE_BLINK_DOUBLE_MAX = 0.95f;
const F32 EYE_BLINK_DOUBLE_CHANCE = 0.12f;
const F32 EYE_BLINK_HOLD_TIME = 0.045f; // brief pause at full closure
const F32 EYE_BLINK_CLOSE_SPEED = 0.06f; // gradual eyelid closure
const F32 EYE_BLINK_OPEN_SPEED = 0.085f; // slightly slower reopening
const F32 EYE_BLINK_TIME_DELTA = 0.005f; // time between one eye starting a blink and the other following

//-----------------------------------------------------------------------------
// LLHeadRotMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLHeadRotMotion::LLHeadRotMotion(const LLUUID &id) :
    LLMotion(id),
    mCharacter(NULL),
    mTorsoJoint(NULL),
    mHeadJoint(NULL),
    mIdleHeadNextUpdate(IDLE_HEAD_UPDATE_MIN),
    mIdleHeadYaw(0.f),
    mIdleHeadPitch(0.f),
    mIdleHeadTargetYaw(0.f),
    mIdleHeadTargetPitch(0.f),
    mTorsoRecruitment(0.f),
    mTorsoTurnActive(false)
{
    mName = "head_rot";

    mTorsoState = new LLJointState;
    mNeckState = new LLJointState;
    mHeadState = new LLJointState;
}


//-----------------------------------------------------------------------------
// ~LLHeadRotMotion()
// Class Destructor
//-----------------------------------------------------------------------------
LLHeadRotMotion::~LLHeadRotMotion()
{
}

//-----------------------------------------------------------------------------
// LLHeadRotMotion::onInitialize(LLCharacter *character)
//-----------------------------------------------------------------------------
LLMotion::LLMotionInitStatus LLHeadRotMotion::onInitialize(LLCharacter *character)
{
    if (!character)
        return STATUS_FAILURE;
    mCharacter = character;

    mPelvisJoint = character->getJoint("mPelvis");
    if ( ! mPelvisJoint )
    {
        LL_INFOS() << getName() << ": Can't get pelvis joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mRootJoint = character->getJoint("mRoot");
    if ( ! mRootJoint )
    {
        LL_INFOS() << getName() << ": Can't get root joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mTorsoJoint = character->getJoint("mTorso");
    if ( ! mTorsoJoint )
    {
        LL_INFOS() << getName() << ": Can't get torso joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mHeadJoint = character->getJoint("mHead");
    if ( ! mHeadJoint )
    {
        LL_INFOS() << getName() << ": Can't get head joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mTorsoState->setJoint( character->getJoint("mTorso") );
    if ( ! mTorsoState->getJoint() )
    {
        LL_INFOS() << getName() << ": Can't get torso joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mNeckState->setJoint( character->getJoint("mNeck") );
    if ( ! mNeckState->getJoint() )
    {
        LL_INFOS() << getName() << ": Can't get neck joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mHeadState->setJoint( character->getJoint("mHead") );
    if ( ! mHeadState->getJoint() )
    {
        LL_INFOS() << getName() << ": Can't get head joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mTorsoState->setUsage(LLJointState::ROT);
    mNeckState->setUsage(LLJointState::ROT);
    mHeadState->setUsage(LLJointState::ROT);

    addJointState( mTorsoState );
    addJointState( mNeckState );
    addJointState( mHeadState );

    mLastHeadRot.loadIdentity();

    return STATUS_SUCCESS;
}


//-----------------------------------------------------------------------------
// LLHeadRotMotion::onActivate()
//-----------------------------------------------------------------------------
bool LLHeadRotMotion::onActivate()
{
    mIdleHeadTimer.reset();
    mIdleHeadNextUpdate = IDLE_HEAD_UPDATE_MIN +
        ll_frand(IDLE_HEAD_UPDATE_MAX - IDLE_HEAD_UPDATE_MIN);
    mIdleHeadYaw = 0.f;
    mIdleHeadPitch = 0.f;
    mIdleHeadTargetYaw = 0.f;
    mIdleHeadTargetPitch = 0.f;
    mTorsoTurnTimer.reset();
    mTorsoRecruitment = 0.f;
    mTorsoTurnActive = false;
    return true;
}


//-----------------------------------------------------------------------------
// LLHeadRotMotion::onUpdate()
//-----------------------------------------------------------------------------
bool LLHeadRotMotion::onUpdate(F32 time, U8* joint_mask)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;
    LLQuaternion    targetHeadRotWorld;
    LLQuaternion    currentRootRotWorld = mRootJoint->getWorldRotation();
    LLQuaternion    currentInvRootRotWorld = ~currentRootRotWorld;

    F32 head_slerp_amt = LLSmoothInterpolation::getInterpolant(HEAD_LOOKAT_LAG_HALF_LIFE);
    F32 torso_slerp_amt = LLSmoothInterpolation::getInterpolant(TORSO_LOOKAT_LAG_HALF_LIFE);

    LLVector3* targetPos = (LLVector3*)mCharacter->getAnimationData("LookAtPoint");

    if (targetPos)
    {
        LLVector3 headLookAt = *targetPos;

//      LL_INFOS() << "Look At: " << headLookAt + mHeadJoint->getWorldPosition() << LL_ENDL;

        F32 lookatDistance = headLookAt.normVec();

        if (lookatDistance < MIN_HEAD_LOOKAT_DISTANCE)
        {
            targetHeadRotWorld = mPelvisJoint->getWorldRotation();
        }
        else
        {
            LLVector3 root_up = LLVector3(0.f, 0.f, 1.f) * currentRootRotWorld;
            LLVector3 left(root_up % headLookAt);
            // if look_at has zero length, fail
            // if look_at and skyward are parallel, fail
            //
            // Test both of these conditions with a cross product.

            if (left.magVecSquared() < 0.15f)
            {
                LLVector3 root_at = LLVector3(1.f, 0.f, 0.f) * currentRootRotWorld;
                root_at.mV[VZ] = 0.f;
                root_at.normVec();

                headLookAt = lerp(headLookAt, root_at, 0.4f);
                headLookAt.normVec();

                left = root_up % headLookAt;
            }

            // Make sure look_at and skyward and not parallel
            // and neither are zero length
            LLVector3 up(headLookAt % left);

            targetHeadRotWorld = LLQuaternion(headLookAt, left, up);
        }
    }
    else
    {
        targetHeadRotWorld = currentRootRotWorld;
    }

    // Add a very small local idle head drift only when no explicit look target
    // is active. It is a procedural pose hint, not a network animation, and
    // remains subject to the normal motion-controller joint blending.
    if (targetPos)
    {
        mIdleHeadTargetYaw = 0.f;
        mIdleHeadTargetPitch = 0.f;
        mIdleHeadTimer.reset();
        mIdleHeadNextUpdate = IDLE_HEAD_UPDATE_MIN;
    }
    else if (mIdleHeadTimer.getElapsedTimeF32() >= mIdleHeadNextUpdate)
    {
        mIdleHeadTargetYaw = (ll_frand(2.f) - 1.f) * IDLE_HEAD_MAX_YAW;
        mIdleHeadTargetPitch = (ll_frand(2.f) - 1.f) * IDLE_HEAD_MAX_PITCH;
        mIdleHeadNextUpdate = IDLE_HEAD_UPDATE_MIN +
            ll_frand(IDLE_HEAD_UPDATE_MAX - IDLE_HEAD_UPDATE_MIN);
        mIdleHeadTimer.reset();
    }

    const F32 idle_blend = LLSmoothInterpolation::getInterpolant(IDLE_HEAD_BLEND_HALF_LIFE);
    mIdleHeadYaw += (mIdleHeadTargetYaw - mIdleHeadYaw) * idle_blend;
    mIdleHeadPitch += (mIdleHeadTargetPitch - mIdleHeadPitch) * idle_blend;

    // Facial expression composition publishes a per-avatar, smoothed pitch
    // hint. Keep it inside the existing idle layer: explicit look targets and
    // externally driven head poses are never modified by this contribution.
    F32 facial_head_pitch_bias = 0.f;
    if (!targetPos)
    {
        F32* facial_bias = (F32*)mCharacter->getAnimationData("FacialHeadPitchBias");
        if (facial_bias)
        {
            facial_head_pitch_bias = llclamp(*facial_bias, -1.f * DEG_TO_RAD, 1.f * DEG_TO_RAD);
        }
    }

    LLQuaternion head_rot_local = targetHeadRotWorld * currentInvRootRotWorld;
    if (!targetPos)
    {
        LLQuaternion idle_yaw(mIdleHeadYaw, LLVector3::z_axis);
        LLQuaternion idle_pitch(mIdleHeadPitch + facial_head_pitch_bias, LLVector3::y_axis);
        head_rot_local = idle_yaw * idle_pitch * head_rot_local;
    }
    head_rot_local.constrain(HEAD_ROTATION_CONSTRAINT);

    // Recruit the torso progressively for larger horizontal head turns.
    // Small glances remain mostly in the neck/head; sustained side-looking
    // shares more rotation with the torso without changing motion priority.
    F32 head_roll = 0.f;
    F32 head_pitch = 0.f;
    F32 head_yaw = 0.f;
    head_rot_local.getEulerAngles(&head_roll, &head_pitch, &head_yaw);
    const F32 abs_head_yaw = head_yaw < 0.f ? -head_yaw : head_yaw;
    const F32 yaw_recruitment = llclamp(
        (abs_head_yaw - 12.f * DEG_TO_RAD) / (33.f * DEG_TO_RAD),
        0.f, 1.f);

    // A short glance should not recruit the torso. Require a sustained turn,
    // then ease the torso contribution in and out to avoid a sudden posture shift.
    // Use separate enter/exit thresholds so small tracking fluctuations
    // around the activation angle do not repeatedly restart the timer.
    if (!mTorsoTurnActive && abs_head_yaw > 18.f * DEG_TO_RAD)
    {
        mTorsoTurnActive = true;
        mTorsoTurnTimer.reset();
    }
    else if (mTorsoTurnActive && abs_head_yaw < 12.f * DEG_TO_RAD)
    {
        mTorsoTurnActive = false;
        mTorsoTurnTimer.reset();
    }
    const F32 sustained_turn = mTorsoTurnActive
        ? llclamp(mTorsoTurnTimer.getElapsedTimeF32() / 0.45f, 0.f, 1.f)
        : 0.f;
    const F32 target_recruitment = yaw_recruitment * sustained_turn;
    const F32 recruitment_blend = LLSmoothInterpolation::getInterpolant(0.35f);
    mTorsoRecruitment +=
        (target_recruitment - mTorsoRecruitment) * recruitment_blend;
    const F32 torso_follow = TORSO_LAG + 0.10f * mTorsoRecruitment;

    // Set torso target rotation to lag behind the head, with a little more
    // participation as the avatar turns farther to either side.
    LLQuaternion torso_rot_local = nlerp(torso_follow, LLQuaternion::DEFAULT, head_rot_local);
    mTorsoState->setRotation(nlerp(torso_slerp_amt, mTorsoState->getRotation(), torso_rot_local));

    head_rot_local = nlerp(head_slerp_amt, mLastHeadRot, head_rot_local);
    mLastHeadRot = head_rot_local;

    // Set the head rotation.
    if(mNeckState->getJoint() && mNeckState->getJoint()->getParent())
    {
        LLQuaternion torsoRotLocal =  mNeckState->getJoint()->getParent()->getWorldRotation() * currentInvRootRotWorld;
        head_rot_local = head_rot_local * ~torsoRotLocal;
        mNeckState->setRotation( nlerp(NECK_LAG, LLQuaternion::DEFAULT, head_rot_local) );
        mHeadState->setRotation( nlerp(1.f - NECK_LAG, LLQuaternion::DEFAULT, head_rot_local));
    }

    return true;
}


//-----------------------------------------------------------------------------
// LLHeadRotMotion::onDeactivate()
//-----------------------------------------------------------------------------
void LLHeadRotMotion::onDeactivate()
{
}


//-----------------------------------------------------------------------------
// LLEyeMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLEyeMotion::LLEyeMotion(const LLUUID &id) : LLMotion(id)
{
    mCharacter = NULL;
    mEyeJitterTime = 0.f;
    mEyeJitterYaw = 0.f;
    mEyeJitterPitch = 0.f;

    mEyeLookAwayTime = 0.f;
    mEyeLookAwayYaw = 0.f;
    mEyeLookAwayPitch = 0.f;

    mEyeBlinkTime = 0.f;
    mEyesClosed = false;

    mHeadJoint = NULL;

    mName = "eye_rot";

    mLeftEyeState = new LLJointState;
    mAltLeftEyeState = new LLJointState;

    mRightEyeState = new LLJointState;
    mAltRightEyeState = new LLJointState;
}


//-----------------------------------------------------------------------------
// ~LLEyeMotion()
// Class Destructor
//-----------------------------------------------------------------------------
LLEyeMotion::~LLEyeMotion()
{
}

//-----------------------------------------------------------------------------
// LLEyeMotion::onInitialize(LLCharacter *character)
//-----------------------------------------------------------------------------
LLMotion::LLMotionInitStatus LLEyeMotion::onInitialize(LLCharacter *character)
{
    mCharacter = character;

    mHeadJoint = character->getJoint("mHead");
    if ( ! mHeadJoint )
    {
        LL_INFOS() << getName() << ": Can't get head joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mLeftEyeState->setJoint( character->getJoint("mEyeLeft") );
    if ( ! mLeftEyeState->getJoint() )
    {
        LL_INFOS() << getName() << ": Can't get left eyeball joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    // Alternate face-eye joints are optional. Some compatible avatar rigs
    // expose only the primary eye joints; missing optional joints must not
    // disable the entire eye motion.
    mAltLeftEyeState->setJoint( character->getJoint("mFaceEyeAltLeft") );
    if ( ! mAltLeftEyeState->getJoint() )
    {
        LL_DEBUGS("Avatar") << getName() << ": No alternate left eyeball joint; using primary eyes." << LL_ENDL;
    }

    mRightEyeState->setJoint( character->getJoint("mEyeRight") );
    if ( ! mRightEyeState->getJoint() )
    {
        LL_INFOS() << getName() << ": Can't get right eyeball joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mAltRightEyeState->setJoint( character->getJoint("mFaceEyeAltRight") );
    if ( ! mAltRightEyeState->getJoint() )
    {
        LL_DEBUGS("Avatar") << getName() << ": No alternate right eyeball joint; using primary eyes." << LL_ENDL;
    }

    mLeftEyeState->setUsage(LLJointState::ROT);
    mRightEyeState->setUsage(LLJointState::ROT);
    addJointState( mLeftEyeState );
    addJointState( mRightEyeState );

    // Register alternate eye joints only when present. The update path below
    // treats the alternate pair as an optional second eye rig.
    if (mAltLeftEyeState->getJoint() && mAltRightEyeState->getJoint())
    {
        mAltLeftEyeState->setUsage(LLJointState::ROT);
        mAltRightEyeState->setUsage(LLJointState::ROT);
        addJointState( mAltLeftEyeState );
        addJointState( mAltRightEyeState );
    }

    return STATUS_SUCCESS;
}


//-----------------------------------------------------------------------------
// LLEyeMotion::onActivate()
//-----------------------------------------------------------------------------
bool LLEyeMotion::onActivate()
{
    // Start with a short natural settling period instead of an immediate
    // blink/look-away on the first rendered frame after activation.
    mEyeJitterTimer.reset();
    mEyeBlinkTimer.reset();
    mEyeJitterTime = EYE_JITTER_MIN_TIME + ll_frand(EYE_JITTER_MAX_TIME - EYE_JITTER_MIN_TIME);
    mEyeJitterYaw = 0.f;
    mEyeJitterPitch = 0.f;

    mEyeLookAwayTime = EYE_LOOK_AWAY_MIN_TIME + ll_frand(EYE_LOOK_AWAY_MAX_TIME - EYE_LOOK_AWAY_MIN_TIME);
    mEyeLookAwayYaw = 0.f;
    mEyeLookAwayPitch = 0.f;

    // The first blink is deliberately delayed; subsequent intervals retain
    // the existing randomized cadence.
    mEyeBlinkTime = 1.5f + ll_frand(3.0f);
    mEyesClosed = false;
    mCharacter->setVisualParamWeight("Blink_Left", 0.f);
    mCharacter->setVisualParamWeight("Blink_Right", 0.f);
    mCharacter->updateVisualParams();
    return true;
}

//-----------------------------------------------------------------------------
// LLEyeMotion::adjustEyeTarget()
//-----------------------------------------------------------------------------
void LLEyeMotion::adjustEyeTarget(LLVector3* targetPos, LLJointState& left_eye_state, LLJointState& right_eye_state)
{
    // Compute eye rotation.
    bool has_eye_target = false;
    LLQuaternion    target_eye_rot;
    LLVector3       eye_look_at;
    F32             vergence;

    if (targetPos)
    {
        LLVector3       skyward(0.f, 0.f, 1.f);
        LLVector3       left;
        LLVector3       up;

        eye_look_at = *targetPos;
        has_eye_target = true;
        F32 lookAtDistance = eye_look_at.normVec();

        left.setVec(skyward % eye_look_at);
        up.setVec(eye_look_at % left);

        target_eye_rot = LLQuaternion(eye_look_at, left, up);
        // convert target rotation to head-local coordinates
        target_eye_rot *= ~mHeadJoint->getWorldRotation();
        // eliminate any Euler roll - we're lucky that roll is applied last.
        F32 roll, pitch, yaw;
        target_eye_rot.getEulerAngles(&roll, &pitch, &yaw);
        target_eye_rot.setQuat(0.0f, pitch, yaw);
        // constrain target orientation to be in front of avatar's face
        target_eye_rot.constrain(EYE_ROT_LIMIT_ANGLE);

        // calculate vergence
        F32 interocular_dist = (left_eye_state.getJoint()->getWorldPosition() - right_eye_state.getJoint()->getWorldPosition()).magVec();
        vergence = -atan2((interocular_dist / 2.f), lookAtDistance);
        // <FS:ND> Guess we should take the return value of that llclamp...
        // llclamp(vergence, -F_PI_BY_TWO, 0.f);
        vergence = llclamp(vergence, -F_PI_BY_TWO, 0.f);
        // </FS:ND>
    }
    else
    {
        target_eye_rot = LLQuaternion::DEFAULT;
        vergence = 0.f;

        // Emotional eye direction is an idle-only hint. Never offset an
        // explicit avatar/object/world look target, and keep the range tiny.
        F32* facial_eye_bias = (F32*)mCharacter->getAnimationData("FacialEyePitchBias");
        if (facial_eye_bias)
        {
            const F32 eye_pitch_bias = llclamp(*facial_eye_bias,
                -1.5f * DEG_TO_RAD, 1.5f * DEG_TO_RAD);
            LLQuaternion facial_eye_pitch(eye_pitch_bias, LLVector3::y_axis);
            target_eye_rot = facial_eye_pitch * target_eye_rot;
        }
    }

    //RN: subtract 4 degrees to account for foveal angular offset relative to pupil
    vergence += 4.f * DEG_TO_RAD;

    // calculate eye jitter
    LLQuaternion eye_jitter_rot;

    // vergence not too high...
    if (vergence > -0.05f)
    {
        //...go ahead and jitter
        eye_jitter_rot.setQuat(0.f, mEyeJitterPitch + mEyeLookAwayPitch, mEyeJitterYaw + mEyeLookAwayYaw);
    }
    else
    {
        //...or don't
        eye_jitter_rot.loadIdentity();
    }

    // calculate vergence of eyes as an object gets closer to the avatar's head
    LLQuaternion vergence_quat;

    if (has_eye_target)
    {
        vergence_quat.setQuat(vergence, LLVector3(0.f, 0.f, 1.f));
    }
    else
    {
        vergence_quat.loadIdentity();
    }

    // calculate eye rotations
    LLQuaternion left_eye_rot = target_eye_rot;
    left_eye_rot = vergence_quat * eye_jitter_rot * left_eye_rot;

    LLQuaternion right_eye_rot = target_eye_rot;
    vergence_quat.transQuat();
    right_eye_rot = vergence_quat * eye_jitter_rot * right_eye_rot;

    // Ease eye direction changes over a few frames. The head motion already
    // follows LookAtPoint with a longer half-life, so this keeps the eyes in
    // the lead without snapping them instantly to a new target.
    const F32 eye_slerp_amt = LLSmoothInterpolation::getInterpolant(EYE_LOOKAT_LAG_HALF_LIFE);
    left_eye_state.setRotation(nlerp(eye_slerp_amt, left_eye_state.getRotation(), left_eye_rot));
    right_eye_state.setRotation(nlerp(eye_slerp_amt, right_eye_state.getRotation(), right_eye_rot));
}

//-----------------------------------------------------------------------------
// LLEyeMotion::onUpdate()
//-----------------------------------------------------------------------------
bool LLEyeMotion::onUpdate(F32 time, U8* joint_mask)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;
    //calculate jitter
    if (mEyeJitterTimer.getElapsedTimeF32() > mEyeJitterTime)
    {
        mEyeJitterTime = EYE_JITTER_MIN_TIME + ll_frand(EYE_JITTER_MAX_TIME - EYE_JITTER_MIN_TIME);
        mEyeJitterYaw = (ll_frand(2.f) - 1.f) * EYE_JITTER_MAX_YAW;
        mEyeJitterPitch = (ll_frand(2.f) - 1.f) * EYE_JITTER_MAX_PITCH;
        // make sure lookaway time count gets updated, because we're resetting the timer
        mEyeLookAwayTime -= llmax(0.f, mEyeJitterTimer.getElapsedTimeF32());
        mEyeJitterTimer.reset();
    }
    else if (mEyeJitterTimer.getElapsedTimeF32() > mEyeLookAwayTime)
    {
        if (ll_frand() > 0.1f)
        {
            // blink while moving eyes some percentage of the time
            mEyeBlinkTime = mEyeBlinkTimer.getElapsedTimeF32();
        }
        if (mEyeLookAwayYaw == 0.f && mEyeLookAwayPitch == 0.f)
        {
            mEyeLookAwayYaw = (ll_frand(2.f) - 1.f) * EYE_LOOK_AWAY_MAX_YAW;
            mEyeLookAwayPitch = (ll_frand(2.f) - 1.f) * EYE_LOOK_AWAY_MAX_PITCH;
            mEyeLookAwayTime = EYE_LOOK_BACK_MIN_TIME + ll_frand(EYE_LOOK_BACK_MAX_TIME - EYE_LOOK_BACK_MIN_TIME);
        }
        else
        {
            mEyeLookAwayYaw = 0.f;
            mEyeLookAwayPitch = 0.f;
            mEyeLookAwayTime = EYE_LOOK_AWAY_MIN_TIME + ll_frand(EYE_LOOK_AWAY_MAX_TIME - EYE_LOOK_AWAY_MIN_TIME);
        }
    }

    // do blinking
    if (!mEyesClosed && mEyeBlinkTimer.getElapsedTimeF32() >= mEyeBlinkTime)
    {
        F32 leftEyeBlinkMorph = mEyeBlinkTimer.getElapsedTimeF32() - mEyeBlinkTime;
        F32 rightEyeBlinkMorph = leftEyeBlinkMorph - EYE_BLINK_TIME_DELTA;

        leftEyeBlinkMorph = llclamp(leftEyeBlinkMorph / EYE_BLINK_CLOSE_SPEED, 0.f, 1.f);
        rightEyeBlinkMorph = llclamp(rightEyeBlinkMorph / EYE_BLINK_CLOSE_SPEED, 0.f, 1.f);
        mCharacter->setVisualParamWeight("Blink_Left", leftEyeBlinkMorph);
        mCharacter->setVisualParamWeight("Blink_Right", rightEyeBlinkMorph);
        mCharacter->updateVisualParams();

        if (rightEyeBlinkMorph == 1.f)
        {
            mEyesClosed = true;
            mEyeBlinkTime = EYE_BLINK_HOLD_TIME;
            mEyeBlinkTimer.reset();
        }
    }
    else if (mEyesClosed)
    {
        if (mEyeBlinkTimer.getElapsedTimeF32() >= mEyeBlinkTime)
        {
            F32 leftEyeBlinkMorph = mEyeBlinkTimer.getElapsedTimeF32() - mEyeBlinkTime;
            F32 rightEyeBlinkMorph = leftEyeBlinkMorph - EYE_BLINK_TIME_DELTA;

            leftEyeBlinkMorph = 1.f - llclamp(leftEyeBlinkMorph / EYE_BLINK_OPEN_SPEED, 0.f, 1.f);
            rightEyeBlinkMorph = 1.f - llclamp(rightEyeBlinkMorph / EYE_BLINK_OPEN_SPEED, 0.f, 1.f);
            mCharacter->setVisualParamWeight("Blink_Left", leftEyeBlinkMorph);
            mCharacter->setVisualParamWeight("Blink_Right", rightEyeBlinkMorph);
            mCharacter->updateVisualParams();

            if (rightEyeBlinkMorph == 0.f)
            {
                mEyesClosed = false;
                // Most blinks use a relaxed 2.2–6.8 second cadence. A small
                // minority form a quick double blink, avoiding a perfectly
                // uniform metronomic pattern without frequent twitching.
                if (ll_frand() < EYE_BLINK_DOUBLE_CHANCE)
                {
                    mEyeBlinkTime = EYE_BLINK_DOUBLE_MIN +
                        ll_frand(EYE_BLINK_DOUBLE_MAX - EYE_BLINK_DOUBLE_MIN);
                }
                else
                {
                    mEyeBlinkTime = EYE_BLINK_MIN_TIME +
                        ll_frand(EYE_BLINK_MAX_TIME - EYE_BLINK_MIN_TIME);
                }
                mEyeBlinkTimer.reset();
            }
        }
    }

    LLVector3* targetPos = (LLVector3*)mCharacter->getAnimationData("LookAtPoint");

    adjustEyeTarget(targetPos, *mLeftEyeState, *mRightEyeState);
    if (mAltLeftEyeState->getJoint() && mAltRightEyeState->getJoint())
    {
        adjustEyeTarget(targetPos, *mAltLeftEyeState, *mAltRightEyeState);
    }

    return true;
}


//-----------------------------------------------------------------------------
// LLEyeMotion::onDeactivate()
//-----------------------------------------------------------------------------
void LLEyeMotion::onDeactivate()
{
    LLJoint* joint = mLeftEyeState->getJoint();
    if (joint)
    {
        joint->setRotation(LLQuaternion::DEFAULT);
    }

    joint = mRightEyeState->getJoint();
    if (joint)
    {
        joint->setRotation(LLQuaternion::DEFAULT);
    }

    // Alternate joints are animated only as a complete pair; leave a
    // partially exposed alternate rig untouched during deactivation.
    if (mAltLeftEyeState->getJoint() && mAltRightEyeState->getJoint())
    {
        mAltLeftEyeState->getJoint()->setRotation(LLQuaternion::DEFAULT);
        mAltRightEyeState->getJoint()->setRotation(LLQuaternion::DEFAULT);
    }

    // A motion can be deactivated while an avatar is culled mid-blink.
    // Always restore eyelid morphs so it cannot reappear with closed eyes.
    if (mCharacter)
    {
        mCharacter->setVisualParamWeight("Blink_Left", 0.f);
        mCharacter->setVisualParamWeight("Blink_Right", 0.f);
        mCharacter->updateVisualParams();
    }
    mEyesClosed = false;
}

// End

