/**
 * @file llavatarskinmaterial.cpp
 * @brief Defaults and validation for native Avatar skin material/layer data.
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarskinmaterial.h"

#include <algorithm>
#include <cmath>

namespace
{
    F32 clampUnitValue(F32 value)
    {
        // The ordered comparisons also turn NaN into a safe zero value.
        if (!(value >= 0.0f))
        {
            return 0.0f;
        }
        return value > 1.0f ? 1.0f : value;
    }

    bool isValidLayerType(LLAvatarSkin::LayerType type)
    {
        switch (type)
        {
        case LLAvatarSkin::LayerType::COLOR_OVERLAY:
        case LLAvatarSkin::LayerType::MICRO_DETAIL:
        case LLAvatarSkin::LayerType::MAKEUP:
        case LLAvatarSkin::LayerType::TATTOO_OVERLAY:
        case LLAvatarSkin::LayerType::SURFACE_NORMAL:
        case LLAvatarSkin::LayerType::SURFACE_ROUGHNESS:
        case LLAvatarSkin::LayerType::SURFACE_SPECULAR:
        case LLAvatarSkin::LayerType::SUBSURFACE_MASK:
            return true;
        default:
            return false;
        }
    }

    bool isValidBlendMode(LLAvatarSkin::BlendMode mode)
    {
        switch (mode)
        {
        case LLAvatarSkin::BlendMode::NORMAL:
        case LLAvatarSkin::BlendMode::MULTIPLY:
        case LLAvatarSkin::BlendMode::ADD:
        case LLAvatarSkin::BlendMode::SOFT_LIGHT:
            return true;
        default:
            return false;
        }
    }
}

namespace LLAvatarSkin
{
    LLColor4 compositeLayer(const LLColor4& base, const LLColor4& layer,
                            F32 opacity, BlendMode mode)
    {
        opacity = clampUnitValue(opacity);
        F32 base_alpha = clampUnitValue(base.mV[3]);
        F32 source_alpha = clampUnitValue(layer.mV[3]) * opacity;
        F32 out_alpha = source_alpha + base_alpha * (1.0f - source_alpha);

        LLColor4 result(0.0f, 0.0f, 0.0f, out_alpha);
        if (out_alpha <= 0.0f)
        {
            return result;
        }

        for (U32 channel = 0; channel < 3; ++channel)
        {
            F32 cb = clampUnitValue(base.mV[channel]);
            F32 cs = clampUnitValue(layer.mV[channel]);
            F32 blend = cs;

            switch (mode)
            {
            case BlendMode::MULTIPLY:
                blend = cb * cs;
                break;
            case BlendMode::ADD:
                blend = std::min(1.0f, cb + cs);
                break;
            case BlendMode::SOFT_LIGHT:
                if (cs <= 0.5f)
                {
                    blend = cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
                }
                else
                {
                    F32 curve = cb <= 0.25f
                        ? ((16.0f * cb - 12.0f) * cb + 4.0f) * cb
                        : std::sqrt(cb);
                    blend = cb + (2.0f * cs - 1.0f) * (curve - cb);
                }
                break;
            case BlendMode::NORMAL:
            default:
                blend = cs;
                break;
            }

            // W3C source-over blend compositing in premultiplied-alpha space.
            F32 premultiplied =
                (1.0f - source_alpha) * cb * base_alpha +
                (1.0f - base_alpha) * cs * source_alpha +
                base_alpha * source_alpha * blend;
            result.mV[channel] = clampUnitValue(premultiplied / out_alpha);
        }
        return result;
    }

    LayerDescriptor::LayerDescriptor()
    :   type(LayerType::COLOR_OVERLAY),
        blend_mode(BlendMode::NORMAL),
        opacity(1.0f),
        order(0),
        enabled(false)
    {
    }

    void LayerDescriptor::sanitize()
    {
        opacity = clampUnitValue(opacity);
        if (!isValidLayerType(type) || !isValidBlendMode(blend_mode) ||
            layer_id.isNull() || texture_id.isNull())
        {
            enabled = false;
        }
    }

    bool LayerDescriptor::isValid() const
    {
        return isValidLayerType(type) && isValidBlendMode(blend_mode) &&
               !layer_id.isNull() && !texture_id.isNull();
    }

    void LayerDescriptor::collectTextureIds(std::set<LLUUID>& ids) const
    {
        if (!texture_id.isNull()) ids.insert(texture_id);
        if (!mask_id.isNull()) ids.insert(mask_id);
    }

    LayerStack::LayerStack()
    :   schema_version(LAYER_STACK_SCHEMA_VERSION)
    {
    }

    void LayerStack::sanitize()
    {
        if (!isSupportedVersion())
        {
            for (std::vector<LayerDescriptor>::iterator it = layers.begin();
                 it != layers.end(); ++it)
            {
                it->enabled = false;
            }
            return;
        }

        std::stable_sort(layers.begin(), layers.end(),
            [](const LayerDescriptor& lhs, const LayerDescriptor& rhs)
            {
                return lhs.order < rhs.order;
            });

        std::set<LLUUID> seen_layer_ids;
        U32 enabled_count = 0;
        for (std::vector<LayerDescriptor>::iterator it = layers.begin();
             it != layers.end(); ++it)
        {
            it->sanitize();
            if (!it->enabled)
            {
                continue;
            }

            if (!seen_layer_ids.insert(it->layer_id).second ||
                enabled_count >= MAX_ENABLED_SKIN_LAYERS)
            {
                it->enabled = false;
                continue;
            }
            ++enabled_count;
        }
    }

    bool LayerStack::isSupportedVersion() const
    {
        return schema_version == LAYER_STACK_SCHEMA_VERSION;
    }

    U32 LayerStack::getEnabledLayerCount() const
    {
        U32 count = 0;
        for (std::vector<LayerDescriptor>::const_iterator it = layers.begin();
             it != layers.end(); ++it)
        {
            if (it->enabled && it->isValid())
            {
                ++count;
            }
        }
        return count;
    }

    void LayerStack::collectTextureIds(std::set<LLUUID>& ids) const
    {
        for (std::vector<LayerDescriptor>::const_iterator it = layers.begin();
             it != layers.end(); ++it)
        {
            it->collectTextureIds(ids);
        }
    }

    MaterialData::MaterialData()
    :   schema_version(MATERIAL_SCHEMA_VERSION),
        roughness_factor(0.65f),
        specular_factor(0.15f),
        subsurface_strength(0.0f),
        subsurface_color(0.82f, 0.58f, 0.48f, 1.0f),
        detail_strength(0.0f),
        enabled(false)
    {
    }

    void MaterialData::sanitize()
    {
        roughness_factor = clampUnitValue(roughness_factor);
        specular_factor = clampUnitValue(specular_factor);
        subsurface_strength = clampUnitValue(subsurface_strength);
        detail_strength = clampUnitValue(detail_strength);

        for (U32 channel = 0; channel < 3; ++channel)
        {
            subsurface_color.mV[channel] = clampUnitValue(subsurface_color.mV[channel]);
        }
        subsurface_color.mV[3] = clampUnitValue(subsurface_color.mV[3]);

        if (!isSupportedVersion())
        {
            enabled = false;
        }
    }

    bool MaterialData::isSupportedVersion() const
    {
        return schema_version == MATERIAL_SCHEMA_VERSION;
    }

    void MaterialData::collectTextureIds(std::set<LLUUID>& ids) const
    {
        if (!normal_map_id.isNull()) ids.insert(normal_map_id);
        if (!roughness_map_id.isNull()) ids.insert(roughness_map_id);
        if (!specular_map_id.isNull()) ids.insert(specular_map_id);
        if (!detail_mask_id.isNull()) ids.insert(detail_mask_id);
    }
}
