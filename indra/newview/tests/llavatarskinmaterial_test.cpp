/**
 * @file llavatarskinmaterial_test.cpp
 * @brief Tests for native Avatar skin material and layer data.
 */

#include "../llviewerprecompiledheaders.h"
#include "../test/lltut.h"

#include "../llavatarskinmaterial.h"
#include "llimage.h"

#include <limits>
#include <set>

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
        ensure_equals("schema version", material.schema_version, static_cast<U32>(1));
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
        material.sanitize();
        ensure("null optional maps are valid", material.enabled);
        ensure("normal map remains null", material.normal_map_id.isNull());
        ensure("roughness map remains null", material.roughness_map_id.isNull());
        ensure("specular map remains null", material.specular_map_id.isNull());
        ensure("detail mask remains null", material.detail_mask_id.isNull());
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<5>()
    {
        LLAvatarSkin::MaterialData material;
        material.roughness_factor = std::numeric_limits<F32>::quiet_NaN();
        material.specular_factor = std::numeric_limits<F32>::infinity();
        material.subsurface_strength = -std::numeric_limits<F32>::infinity();
        material.detail_strength = std::numeric_limits<F32>::quiet_NaN();
        material.subsurface_color.mV[1] = std::numeric_limits<F32>::quiet_NaN();
        material.sanitize();
        ensure_equals("NaN roughness becomes zero", material.roughness_factor, 0.0f);
        ensure_equals("infinite specular clamps to one", material.specular_factor, 1.0f);
        ensure_equals("negative infinite subsurface becomes zero", material.subsurface_strength, 0.0f);
        ensure_equals("NaN detail becomes zero", material.detail_strength, 0.0f);
        ensure_equals("NaN color channel becomes zero", material.subsurface_color.mV[1], 0.0f);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<6>()
    {
        LLAvatarSkin::MaterialData material;
        LLUUID shared_id;
        shared_id.generate();
        LLUUID other_id;
        other_id.generate();
        material.normal_map_id = shared_id;
        material.roughness_map_id = shared_id;
        material.specular_map_id = other_id;
        std::set<LLUUID> ids;
        material.collectTextureIds(ids);
        ensure_equals("shared slot UUID collected once", ids.size(), static_cast<std::set<LLUUID>::size_type>(2));
        ensure("shared UUID present", ids.find(shared_id) != ids.end());
        ensure("second UUID present", ids.find(other_id) != ids.end());
        ensure("null UUID omitted", ids.find(LLUUID::null) == ids.end());
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<7>()
    {
        LLAvatarSkin::LayerDescriptor layer;
        ensure("layer disabled by default", !layer.enabled);
        ensure_equals("default opacity", layer.opacity, 1.0f);
        ensure("missing identifiers make layer invalid", !layer.isValid());

        layer.layer_id.generate();
        layer.texture_id.generate();
        layer.enabled = true;
        layer.opacity = 1.5f;
        layer.sanitize();
        ensure("complete layer is valid", layer.isValid());
        ensure_equals("opacity is clamped", layer.opacity, 1.0f);
        ensure("valid layer remains enabled", layer.enabled);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<8>()
    {
        LLAvatarSkin::LayerStack stack;
        LLAvatarSkin::LayerDescriptor first;
        first.layer_id.generate();
        first.texture_id.generate();
        first.enabled = true;
        first.order = 20;

        LLAvatarSkin::LayerDescriptor second;
        second.layer_id.generate();
        second.texture_id.generate();
        second.enabled = true;
        second.order = 10;

        stack.layers.push_back(first);
        stack.layers.push_back(second);
        stack.sanitize();

        ensure_equals("enabled layer count", stack.getEnabledLayerCount(), static_cast<U32>(2));
        ensure_equals("layers sorted by order", stack.layers[0].order, static_cast<U32>(10));
        ensure_equals("second ordered layer", stack.layers[1].order, static_cast<U32>(20));
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<9>()
    {
        LLAvatarSkin::LayerStack stack;
        LLAvatarSkin::LayerDescriptor first;
        first.layer_id.generate();
        first.texture_id.generate();
        first.enabled = true;
        LLAvatarSkin::LayerDescriptor duplicate = first;
        duplicate.order = 1;
        stack.layers.push_back(first);
        stack.layers.push_back(duplicate);
        stack.sanitize();
        ensure_equals("duplicate layer id disabled", stack.getEnabledLayerCount(), static_cast<U32>(1));
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<10>()
    {
        LLAvatarSkin::LayerStack stack;
        for (U32 i = 0; i < LLAvatarSkin::MAX_ENABLED_SKIN_LAYERS + 2; ++i)
        {
            LLAvatarSkin::LayerDescriptor layer;
            layer.layer_id.generate();
            layer.texture_id.generate();
            layer.enabled = true;
            layer.order = i;
            stack.layers.push_back(layer);
        }
        stack.sanitize();
        ensure_equals("enabled layer cap enforced", stack.getEnabledLayerCount(),
                      LLAvatarSkin::MAX_ENABLED_SKIN_LAYERS);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<11>()
    {
        LLAvatarSkin::LayerStack stack;
        LLAvatarSkin::LayerDescriptor layer;
        layer.layer_id.generate();
        layer.texture_id.generate();
        layer.mask_id.generate();
        layer.enabled = true;
        stack.layers.push_back(layer);
        std::set<LLUUID> ids;
        stack.collectTextureIds(ids);
        ensure_equals("texture and mask collected", ids.size(),
                      static_cast<std::set<LLUUID>::size_type>(2));
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<12>()
    {
        LLAvatarSkin::LayerStack stack;
        stack.schema_version = LLAvatarSkin::LAYER_STACK_SCHEMA_VERSION + 1;
        LLAvatarSkin::LayerDescriptor layer;
        layer.layer_id.generate();
        layer.texture_id.generate();
        layer.enabled = true;
        stack.layers.push_back(layer);
        stack.sanitize();
        ensure("unknown layer schema disables content", stack.getEnabledLayerCount() == 0);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<13>()
    {
        LLColor4 base(1.0f, 0.0f, 0.0f, 1.0f);
        LLColor4 layer(0.0f, 0.0f, 1.0f, 1.0f);
        LLColor4 result = LLAvatarSkin::compositeLayer(
            base, layer, 0.5f, LLAvatarSkin::BlendMode::NORMAL);
        ensure_equals("normal blend red", result.mV[0], 0.5f);
        ensure_equals("normal blend blue", result.mV[2], 0.5f);
        ensure_equals("opaque base remains opaque", result.mV[3], 1.0f);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<14>()
    {
        LLColor4 base(0.8f, 0.4f, 0.2f, 1.0f);
        LLColor4 layer(0.5f, 0.5f, 0.5f, 1.0f);
        LLColor4 result = LLAvatarSkin::compositeLayer(
            base, layer, 1.0f, LLAvatarSkin::BlendMode::MULTIPLY);
        ensure_equals("multiply red", result.mV[0], 0.4f);
        ensure_equals("multiply green", result.mV[1], 0.2f);
        ensure_equals("multiply blue", result.mV[2], 0.1f);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<15>()
    {
        LLColor4 base(0.2f, 0.3f, 0.4f, 1.0f);
        LLColor4 transparent_layer(1.0f, 0.0f, 0.0f, 0.0f);
        LLColor4 result = LLAvatarSkin::compositeLayer(
            base, transparent_layer, 1.0f, LLAvatarSkin::BlendMode::ADD);
        ensure_equals("transparent layer leaves red", result.mV[0], base.mV[0]);
        ensure_equals("transparent layer leaves green", result.mV[1], base.mV[1]);
        ensure_equals("transparent layer leaves blue", result.mV[2], base.mV[2]);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<16>()
    {
        LLColor4 base(0.0f, 0.0f, 0.0f, 0.0f);
        LLColor4 layer(0.4f, 0.6f, 0.8f, 1.0f);
        LLColor4 result = LLAvatarSkin::compositeLayer(
            base, layer, 2.0f, LLAvatarSkin::BlendMode::NORMAL);
        ensure_equals("transparent base receives layer red", result.mV[0], 0.4f);
        ensure_equals("transparent base receives layer green", result.mV[1], 0.6f);
        ensure_equals("transparent base receives layer blue", result.mV[2], 0.8f);
        ensure_equals("opacity clamps to one", result.mV[3], 1.0f);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<17>()
    {
        LLImageRaw base;
        LLImageRaw detail;
        LLImageRaw output;
        ensure("allocate base normal", base.resize(1, 1, 3));
        ensure("allocate detail normal", detail.resize(1, 1, 3));
        U8* b = base.getData();
        U8* d = detail.getData();
        b[0] = 128; b[1] = 128; b[2] = 255;
        d[0] = 255; d[1] = 128; d[2] = 128;
        ensure("normal composite succeeds",
               LLAvatarSkin::compositeNormalMap(base, detail, 1.0f, output));
        const U8* out = output.getData();
        ensure_equals("normal output has RGB components", output.getComponents(), static_cast<S8>(3));
        ensure("detail normal points toward positive X", out[0] > 240);
        ensure("detail normal is normalized toward tangent plane", out[2] < 150);
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<18>()
    {
        LLImageRaw base;
        LLImageRaw detail;
        LLImageRaw output;
        ensure("allocate base normal", base.resize(1, 1, 3));
        ensure("allocate RGBA detail normal", detail.resize(1, 1, 4));
        U8* b = base.getData();
        U8* d = detail.getData();
        b[0] = 128; b[1] = 128; b[2] = 255;
        d[0] = 255; d[1] = 128; d[2] = 128; d[3] = 0;
        ensure("transparent detail normal composite succeeds",
               LLAvatarSkin::compositeNormalMap(base, detail, 1.0f, output));
        const U8* out = output.getData();
        ensure_equals("zero alpha preserves base X", out[0], static_cast<U8>(128));
        ensure_equals("zero alpha preserves base Y", out[1], static_cast<U8>(128));
        ensure_equals("zero alpha preserves base Z", out[2], static_cast<U8>(255));
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<19>()
    {
        LLImageRaw specular;
        LLImageRaw output;
        ensure("allocate specular scalar", specular.resize(1, 1, 1));
        specular.getData()[0] = 64;
        ensure("pack specular with missing roughness",
               LLAvatarSkin::packSpecularRoughness(&specular, nullptr, 0.2f, 0.7f, output));
        const U8* out = output.getData();
        ensure_equals("packed output has RGBA components", output.getComponents(), static_cast<S8>(4));
        ensure_equals("specular stored in red", out[0], static_cast<U8>(64));
        ensure("missing roughness uses configured default", out[1] >= 178 && out[1] <= 180);
        ensure_equals("subsurface channel is neutral", out[2], static_cast<U8>(0));
        ensure_equals("detail channel is neutral", out[3], static_cast<U8>(255));
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<20>()
    {
        LLImageRaw specular;
        LLImageRaw roughness;
        LLImageRaw output;
        ensure("allocate specular", specular.resize(1, 1, 1));
        ensure("allocate mismatched roughness", roughness.resize(2, 1, 1));
        ensure("mismatched material maps are rejected",
               !LLAvatarSkin::packSpecularRoughness(&specular, &roughness, 0.2f, 0.7f, output));
    }

    template<> template<>
    void avatarskinmaterial_object_t::test<21>()
    {
        LLImageRaw base;
        LLImageRaw detail;
        LLImageRaw output;
        ensure("allocate base normal", base.resize(1, 1, 3));
        ensure("allocate invalid two-channel detail", detail.resize(1, 1, 2));
        ensure("unsupported normal component count is rejected",
               !LLAvatarSkin::compositeNormalMap(base, detail, 1.0f, output));
    }

}
