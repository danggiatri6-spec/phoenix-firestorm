/**
 * @file llavatarskintextureresolver.h
 * @brief Viewer texture-manager adapter for optional Avatar skin maps.
 */

#ifndef LL_LLAVATARSKINTEXTURERESOLVER_H
#define LL_LLAVATARSKINTEXTURERESOLVER_H

#include <map>

#include "llviewertexture.h"

namespace LLAvatarSkin
{
    // Per-Avatar cache of references to textures owned by the Viewer texture
    // manager. MaterialData remains UUID-only and never owns these handles.
    class TextureResolver
    {
    public:
        enum class State
        {
            UNASSIGNED,
            LOADING,
            READY,
            MISSING,
            DELETED
        };

        TextureResolver() = default;
        ~TextureResolver() = default;

        // Returns a borrowed pointer. The resolver retains an LLPointer while
        // the entry is cached; the texture manager remains the resource owner.
        LLViewerFetchedTexture* resolve(const LLUUID& texture_id);

        State getState(const LLUUID& texture_id) const;

        // Release this Avatar's references. The global texture manager/cache
        // continues to manage the underlying fetched texture.
        void clear();

    private:
        std::map<LLUUID, LLPointer<LLViewerFetchedTexture>> mTextures;
    };
}

#endif // LL_LLAVATARSKINTEXTURERESOLVER_H
