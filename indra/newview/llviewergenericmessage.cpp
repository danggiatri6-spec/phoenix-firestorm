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

#include "llagent.h"
#include "llavataractions.h"
#include "llgltfmateriallist.h"

LLDispatcher gGenericDispatcher;


namespace
{
    std::string avatar_interaction_label(const std::string& interaction)
    {
        if (interaction == "handshake") return "a handshake";
        if (interaction == "high_five") return "a high-five";
        if (interaction == "shoulder_tap") return "a shoulder tap";
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
        if (status == "expired")
            return "The invitation for " + interaction + " expired.";
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
                        LLAvatarActions::setActiveAvatarInteraction(session_id);
                    else if (status == "ended" || status == "expired")
                        LLAvatarActions::clearActiveAvatarInteraction(session_id);
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
