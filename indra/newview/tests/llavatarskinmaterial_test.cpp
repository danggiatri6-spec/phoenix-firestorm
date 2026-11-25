/**
 * @file llavatarskinmaterial_test.cpp
 * @brief Tests for viewer-local Avatar skin material data.
 */

#include "../llviewerprecompiledheaders.h"
#include "../test/lltut.h"

#include "../llavatarskinmaterial.h"

namespace tut
{
    struct avatarskinmaterial
    {
    };

    typedef test_group<avatarskinmaterial> avatarskinmaterial_t;
    typedef avatarskinmaterial_t::object avatarskinmaterial_object_t;
    tut::avatarskinmaterial_t tut_avatarskinmaterial("LLAvatarSkinMaterial");

    template<> template<>
    void avatarskinmaterial_object_t::test<1>()
    {
        LLAvatarSkin::MaterialData material;

        ensure_equals("schema version", material.schema_version,
                      static_cast<U32>(1));
        ensure("disabled by default", !material.enabled);
        ensure("normal map absent by default", material.normal_map_id.isNull());
        ensure("roughness map absent by default", material.roughness_map_id.isNull());
        ensure("specular map absent by default", material.specular_map_id.isNull());
        ensure("detail mask absent by default", material.detail_mask_id.isNull());
        ensure_equals("default roughness", material.roughness_factor, 0.65f);
        ensure_equals("default specular", material.specular_factor, 0.15f);
        ensure_equals("subsurface disabled", material.subsurface_strength, 0.0f);
        ensure_equals("detail disabled", material.detail_strength, 0.0f);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<2>()
    {
        LLAvatarSkin::MaterialData material;
        material.enabled = true;
        material.roughness_factor = -0.5f;
        material.specular_factor = 1.5f;
        material.subsurface_strength = 2.0f;
        material.detail_strength = -1.0f;
        material.subsurface_color.mV[0] = -0.2f;
        material.subsurface_color.mV[1] = 1.2f;
        material.subsurface_color.mV[2] = 0.5f;
        material.subsurface_color.mV[3] = 2.0f;

        material.sanitize();

        ensure_equals("roughness lower clamp", material.roughness_factor, 0.0f);
        ensure_equals("specular upper clamp", material.specular_factor, 1.0f);
        ensure_equals("subsurface upper clamp", material.subsurface_strength, 1.0f);
        ensure_equals("detail lower clamp", material.detail_strength, 0.0f);
        ensure_equals("color red lower clamp", material.subsurface_color.mV[0], 0.0f);
        ensure_equals("color green upper clamp", material.subsurface_color.mV[1], 1.0f);
        ensure_equals("color blue unchanged", material.subsurface_color.mV[2], 0.5f);
        ensure_equals("color alpha upper clamp", material.subsurface_color.mV[3], 1.0f);
        ensure("supported material remains enabled", material.enabled);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<3>()
    {
        LLAvatarSkin::MaterialData material;
        material.schema_version = LLAvatarSkin::MATERIAL_SCHEMA_VERSION + 1;
        material.enabled = true;

        ensure("unknown version rejected", !material.isSupportedVersion());
        material.sanitize();
        ensure("unknown version disables material", !material.enabled);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<4>()
    {
        LLAvatarSkin::MaterialData material;
        material.enabled = true;
        material.normal_map_id.setNull();
        material.roughness_map_id.setNull();
        material.specular_map_id.setNull();
        material.detail_mask_id.setNull();

        material.sanitize();

        ensure("null optional maps are valid", material.enabled);
        ensure("normal map remains null", material.normal_map_id.isNull());
        ensure("roughness map remains null", material.roughness_map_id.isNull());
        ensure("specular map remains null", material.specular_map_id.isNull());
        ensure("detail mask remains null", material.detail_mask_id.isNull());
    }
}
