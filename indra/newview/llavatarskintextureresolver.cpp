/**
 * @file llavatarskintextureresolver.cpp
 * @brief UUID-to-Viewer-texture resolution for optional Avatar skin maps.
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarskintextureresolver.h"

namespace LLAvatarSkin
{
    LLViewerFetchedTexture* TextureResolver::resolve(const LLUUID& texture_id)
    {
        if (texture_id.isNull())
        {
            return nullptr;
        }

        auto found = mTextures.find(texture_id);
        if (found != mTextures.end())
        {
            if (!found->second->isDeleted())
            {
                return found->second.get();
            }

            mTextures.erase(found);
        }

        LLViewerFetchedTexture* texture =
            LLViewerTextureManager::getFetchedTexture(
                texture_id,
                FTT_DEFAULT,
                true,
                LLViewerTexture::BOOST_NONE,
                LLViewerTexture::FETCHED_TEXTURE);

        if (!texture)
        {
            return nullptr;
        }

        mTextures.emplace(texture_id, texture);
        return texture;
    }

    TextureResolver::State TextureResolver::getState(const LLUUID& texture_id) const
    {
        if (texture_id.isNull())
        {
            return State::UNASSIGNED;
        }

        const auto found = mTextures.find(texture_id);
        if (found == mTextures.end())
        {
            return State::UNASSIGNED;
        }

        LLViewerFetchedTexture* texture = found->second.get();
        if (!texture || texture->isDeleted())
        {
            return State::DELETED;
        }

        if (texture->isMissingAsset())
        {
            return State::MISSING;
        }

        return texture->isFullyLoaded() ? State::READY : State::LOADING;
    }

    void TextureResolver::clear()
    {
        mTextures.clear();
    }
}
