/**
 * @file llavatarskintextureresolver_test.cpp
 * @brief Tests for safe empty-state behavior of the Avatar skin texture resolver.
 */

#include "../llviewerprecompiledheaders.h"
#include "../test/lltut.h"

#include "../llavatarskintextureresolver.h"

namespace tut
{
    struct avatarskintextureresolver
    {
    };

    typedef test_group<avatarskintextureresolver> avatarskintextureresolver_t;
    typedef avatarskintextureresolver_t::object avatarskintextureresolver_object_t;
    tut::avatarskintextureresolver_t tut_avatarskintextureresolver("LLAvatarSkinTextureResolver");

    template<> template<>
    void avatarskintextureresolver_object_t::test<1>()
    {
        LLAvatarSkin::TextureResolver resolver;
        const LLUUID null_id;

        ensure("null UUID does not resolve", resolver.resolve(null_id) == nullptr);
        ensure("null UUID is unassigned",
               resolver.getState(null_id) == LLAvatarSkin::TextureResolver::State::UNASSIGNED);
    }

    template<> template<>
    void avatarskintextureresolver_object_t::test<2>()
    {
        LLAvatarSkin::TextureResolver resolver;
        LLUUID texture_id;
        texture_id.generate();

        ensure("unrequested UUID is unassigned",
               resolver.getState(texture_id) == LLAvatarSkin::TextureResolver::State::UNASSIGNED);

        resolver.clear();

        ensure("clear is safe for empty resolver",
               resolver.getState(texture_id) == LLAvatarSkin::TextureResolver::State::UNASSIGNED);
    }
}
