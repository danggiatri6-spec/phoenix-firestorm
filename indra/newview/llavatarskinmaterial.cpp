/**
 * @file llavatarskinmaterial.cpp
 * @brief Defaults and validation for viewer-local Avatar skin materials.
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarskinmaterial.h"

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
}

namespace LLAvatarSkin
{
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
