/**
 * @file llviewergenericmessage.cpp
 * @brief Handle processing of "generic messages" which contain short lists of strings.
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "llviewergenericmessage.h"

#include "lldispatcher.h"
#include "lluuid.h"
#include "message.h"
#include "llgenericstreamingmessage.h"
#include "llhttpnode.h"
#include "llnotificationsutil.h"
#include "llviewerregion.h"
#include "llviewerobjectlist.h"
#include "llagentcamera.h"
#include "llvoavatar.h"
#include "lltimer.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llgltfmateriallist.h"

#include <map>

LLDispatcher gGenericDispatcher;


namespace
{
    struct LLAvatarInteractionMotions
    {
        LLUUID requester;
        LLUUID target;
        LLUUID requester_motion;
        LLUUID target_motion;
        F64 queued_at;
    };

    // Keep only motions explicitly supplied by the server. Session ownership
    // prevents an end event from stopping AO or user motions.
    std::map<LLUUID, LLAvatarInteractionMotions> sAvatarInteractionMotions;
    std::map<LLUUID, LLAvatarInteractionMotions> sPendingAvatarInteractionMotions;

    bool avatar_interaction_try_start_paired_motions(const LLUUID& session_id,
        const LLAvatarInteractionMotions& motions)
    {
        LLViewerObject* requester_object = gObjectList.findObject(motions.requester);
        LLViewerObject* target_object = gObjectList.findObject(motions.target);
        LLVOAvatar* requester_avatar = requester_object && requester_object->isAvatar()
            ? (LLVOAvatar*)requester_object : nullptr;
        LLVOAvatar* target_avatar = target_object && target_object->isAvatar()
            ? (LLVOAvatar*)target_object : nullptr;

        // Wait until both avatar objects exist. Never start only one participant.
        if (!requester_avatar || !target_avatar || requester_avatar->isDead() || target_avatar->isDead())
            return false;

        // A paired contact animation is only meaningful while the participants
        // are physically close. They may have moved after accepting the invite.
        // Keep the request pending so a deliberate local approach can bring
        // them into range; never move either avatar automatically here.
        const F32 separation =
            (requester_avatar->getPositionAgent() - target_avatar->getPositionAgent()).length();
        constexpr F32 MIN_PAIRED_MOTION_DISTANCE = 0.45f;
        constexpr F32 MAX_PAIRED_MOTION_DISTANCE = 2.0f;
        if (separation < MIN_PAIRED_MOTION_DISTANCE ||
            separation > MAX_PAIRED_MOTION_DISTANCE)
            return false;

        // Firestorm's avatar alignment code uses the rotated local X axis as
        // the avatar's forward direction. Require both participants to face
        // one another before playing a contact animation.
        LLVector3 requester_to_target = target_avatar->getPositionAgent() -
            requester_avatar->getPositionAgent();
        requester_to_target.mV[VZ] = 0.f;
        if (requester_to_target.normalize() < 0.001f)
            return false;

        LLVector3 requester_forward = LLVector3::x_axis * requester_avatar->getRotation();
        LLVector3 target_forward = LLVector3::x_axis * target_avatar->getRotation();
        requester_forward.mV[VZ] = 0.f;
        target_forward.mV[VZ] = 0.f;
        if (requester_forward.normalize() < 0.001f ||
            target_forward.normalize() < 0.001f)
            return false;

        constexpr F32 MIN_FACING_DOT = 0.5f; // within 60 degrees of face-to-face
        if ((requester_forward * requester_to_target) < MIN_FACING_DOT ||
            (target_forward * -requester_to_target) < MIN_FACING_DOT)
            return false;

        const bool requester_started = requester_avatar->startMotion(motions.requester_motion);
        const bool target_started = target_avatar->startMotion(motions.target_motion);
        if (requester_started && target_started)
        {
            sAvatarInteractionMotions[session_id] = motions;
            return true;
        }

        // Roll back a partial start before the next retry.
        if (requester_started)
            requester_avatar->stopMotion(motions.requester_motion);
        if (target_started)
            target_avatar->stopMotion(motions.target_motion);
        return false;
    }

    void avatar_interaction_start_paired_motions(const LLSD& body,
        const LLUUID& requester, const LLUUID& target, const LLUUID& session_id)
    {
        if (session_id.isNull() || sAvatarInteractionMotions.find(session_id) != sAvatarInteractionMotions.end() ||
            sPendingAvatarInteractionMotions.find(session_id) != sPendingAvatarInteractionMotions.end())
            return;
        if (!body.has("requester_animation") || !body.has("target_animation"))
            return;

        LLAvatarInteractionMotions motions;
        motions.requester = requester;
        motions.target = target;
        motions.requester_motion = body["requester_animation"].asUUID();
        motions.target_motion = body["target_animation"].asUUID();
        motions.queued_at = LLTimer::getTotalSeconds();

        if (motions.requester_motion.isNull() || motions.target_motion.isNull())
            return;

        sPendingAvatarInteractionMotions[session_id] = motions;
        avatar_interaction_try_start_paired_motions(session_id, motions);
        if (sAvatarInteractionMotions.find(session_id) != sAvatarInteractionMotions.end())
            sPendingAvatarInteractionMotions.erase(session_id);
    }

    void avatar_interaction_stop_paired_motions(const LLUUID& session_id)
    {
        sPendingAvatarInteractionMotions.erase(session_id);
        auto found = sAvatarInteractionMotions.find(session_id);
        if (found == sAvatarInteractionMotions.end())
            return;

        const LLAvatarInteractionMotions motions = found->second;
        sAvatarInteractionMotions.erase(found);

        LLViewerObject* requester_object = gObjectList.findObject(motions.requester);
        LLViewerObject* target_object = gObjectList.findObject(motions.target);
        if (requester_object && requester_object->isAvatar())
            ((LLVOAvatar*)requester_object)->stopMotion(motions.requester_motion);
        if (target_object && target_object->isAvatar())
            ((LLVOAvatar*)target_object)->stopMotion(motions.target_motion);
    }

    std::string avatar_interaction_label(const std::string& interaction)
    {
        if (interaction == "handshake") return "a handshake";
        if (interaction == "high_five") return "a high-five";
        if (interaction == "shoulder_tap") return "a shoulder tap";
        if (interaction == "hug") return "a hug";
        if (interaction == "kiss") return "a kiss";
        if (interaction == "holding_hands") return "holding hands";
        if (interaction == "affection") return "an affectionate pose";
        return "an avatar interaction";
    }

    std::string avatar_interaction_status_message(const LLSD& body)
    {
        const std::string status = body["status"].asString();
        const std::string interaction = avatar_interaction_label(body["interaction"].asString());

        if (status == "accepted")
            return "Your invitation for " + interaction + " was accepted.";
        if (status == "rejected")
            return "Your invitation for " + interaction + " was declined.";
        if (status == "cancelled")
            return "The invitation for " + interaction + " was cancelled.";
        if (status == "expired" && body.has("session_id"))
            return "The " + interaction + " session timed out.";
        if (status == "expired")
            return "The invitation for " + interaction + " expired.";
        if (status == "ended")
            return "The " + interaction + " session has ended.";
        return "Avatar interaction update: " + status + ".";
    }

    void avatar_interaction_invite_callback(const LLSD& notification, const LLSD& response)
    {
        LLViewerRegion* region = gAgent.getRegion();
        if (!region || !region->isCapabilityAvailable("AvatarInteraction"))
            return;

        LLSD request;
        request["action"] = LLNotificationsUtil::getSelectedOption(notification, response) == 0
            ? "accept" : "reject";
        request["request_id"] = notification["payload"]["request_id"];
        region->requestPostCapability("AvatarInteraction", request);
    }

    class LLAvatarInteractionEventNode : public LLHTTPNode
    {
    public:
        LLSD simplePost(const LLSD& input) const override
        {
            const LLSD body = input["body"];
            if (!body.isMap())
                return LLSD();

            if (body.has("requester_name") && body.has("request_id"))
            {
                LLSD substitutions;
                substitutions["REQUESTER"] = body["requester_name"].asString();
                substitutions["INTERACTION"] = avatar_interaction_label(body["interaction"].asString());
                LLNotificationsUtil::add("AvatarInteractionInvite", substitutions, body,
                    avatar_interaction_invite_callback);
            }
            else if (body.has("status"))
            {
                const std::string status = body["status"].asString();
                if (body.has("session_id"))
                {
                    const LLUUID session_id = body["session_id"].asUUID();
                    if (status == "accepted")
                    {
                        const LLUUID requester = body["requester"].asUUID();
                        const LLUUID target = body["target"].asUUID();
                        const LLUUID partner = requester == gAgent.getID() ? target : requester;
                        LLAvatarActions::setActiveAvatarInteraction(session_id, partner);
                        avatar_interaction_start_paired_motions(body, requester, target, session_id);
                        LLViewerObject* partner_object = gObjectList.findObject(partner);
                        if (partner_object && partner_object->isAvatar())
                            gAgentCamera.setInteractionLookAt(partner_object);
                    }
                    else if (status == "ended" || status == "expired")
                    {
                        avatar_interaction_stop_paired_motions(session_id);
                        LLAvatarActions::clearActiveAvatarInteraction(session_id);
                        gAgentCamera.clearInteractionLookAt();
                    }
                }

                LLSD substitutions;
                substitutions["MESSAGE"] = avatar_interaction_status_message(body);
                LLNotificationsUtil::add("GenericAlert", substitutions);
            }

            return LLSD();
        }
    };

    LLHTTPRegistration<LLAvatarInteractionEventNode> gAvatarInteractionInvite(
        "/message/AvatarInteractionInvite");
    LLHTTPRegistration<LLAvatarInteractionEventNode> gAvatarInteractionAccepted(
        "/message/AvatarInteractionAccepted");
    LLHTTPRegistration<LLAvatarInteractionEventNode> gAvatarInteractionEnded(
        "/message/AvatarInteractionEnded");
}

// Called from LLAgentCamera::updateCamera() so late avatar creation and asset
// loading can complete without another timer or background thread.
void update_avatar_interaction_motions()
{
    const F64 now = LLTimer::getTotalSeconds();
    auto it = sPendingAvatarInteractionMotions.begin();
    while (it != sPendingAvatarInteractionMotions.end())
    {
        const LLUUID session_id = it->first;
        const LLAvatarInteractionMotions motions = it->second;

        if (LLAvatarActions::getActiveAvatarInteractionSession() != session_id ||
            now - motions.queued_at > 10.0)
        {
            it = sPendingAvatarInteractionMotions.erase(it);
            continue;
        }

        if (avatar_interaction_try_start_paired_motions(session_id, motions))
            it = sPendingAvatarInteractionMotions.erase(it);
        else
            ++it;
    }
}


void send_generic_message(const std::string& method,
                          const std::vector<std::string>& strings,
                          const LLUUID& invoice)
{
    LLMessageSystem* msg = gMessageSystem;
    msg->newMessage("GenericMessage");
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
    msg->nextBlock("MethodData");
    msg->addString("Method", method);
    msg->addUUID("Invoice", invoice);
    if(strings.empty())
    {
        msg->nextBlock("ParamList");
        msg->addString("Parameter", NULL);
    }
    else
    {
        std::vector<std::string>::const_iterator it = strings.begin();
        std::vector<std::string>::const_iterator end = strings.end();
        for(; it != end; ++it)
        {
            msg->nextBlock("ParamList");
            msg->addString("Parameter", *it);
        }
    }
    gAgent.sendReliableMessage();
}

void process_generic_message(LLMessageSystem* msg, void**)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_NETWORK;
    LLUUID agent_id;
    msg->getUUID("AgentData", "AgentID", agent_id);
    if (agent_id != gAgent.getID())
    {
        LL_WARNS() << "GenericMessage for wrong agent" << LL_ENDL;
        return;
    }

    std::string request;
    LLUUID invoice;
    LLDispatcher::sparam_t strings;
    LLDispatcher::unpackMessage(msg, request, invoice, strings);

    if(!gGenericDispatcher.dispatch(request, invoice, strings))
    {
        LL_WARNS() << "GenericMessage " << request << " failed to dispatch"
            << LL_ENDL;
    }
}

void process_generic_streaming_message(LLMessageSystem* msg, void**)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_NETWORK;
    LLGenericStreamingMessage data;
    data.unpack(msg);
    switch (data.mMethod)
    {
    case LLGenericStreamingMessage::METHOD_GLTF_MATERIAL_OVERRIDE:
        gGLTFMaterialList.applyOverrideMessage(msg, data.mData);
        break;
    default:
        LL_WARNS_ONCE() << "Received unknown method" << LL_ENDL;
        break;
    }
}

void process_large_generic_message(LLMessageSystem* msg, void**)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_NETWORK;
    LLUUID agent_id;
    msg->getUUID("AgentData", "AgentID", agent_id);
    if (agent_id != gAgent.getID())
    {
        LL_WARNS() << "GenericMessage for wrong agent" << LL_ENDL;
        return;
    }

    std::string request;
    LLUUID invoice;
    LLDispatcher::sparam_t strings;
    LLDispatcher::unpackLargeMessage(msg, request, invoice, strings);

    if (!gGenericDispatcher.dispatch(request, invoice, strings))
    {
        LL_WARNS() << "GenericMessage " << request << " failed to dispatch"
            << LL_ENDL;
    }
}
