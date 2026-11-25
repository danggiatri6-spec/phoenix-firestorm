/**
 * @file llavatarskinmaterial.cpp
 * @brief Defaults and validation for viewer-local Avatar skin materials.
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarskinmaterial.h"

#include <algorithm>

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
        roughness_factor = std::max(0.0f, std::min(1.0f, roughness_factor));
        specular_factor = std::max(0.0f, std::min(1.0f, specular_factor));
        subsurface_strength = std::max(0.0f, std::min(1.0f, subsurface_strength));
        detail_strength = std::max(0.0f, std::min(1.0f, detail_strength));

        for (U32 channel = 0; channel < 3; ++channel)
        {
            subsurface_color.mV[channel] =
                std::max(0.0f, std::min(1.0f, subsurface_color.mV[channel]));
        }
        subsurface_color.mV[3] =
            std::max(0.0f, std::min(1.0f, subsurface_color.mV[3]));

        if (!isSupportedVersion())
        {
            enabled = false;
        }
    }

    bool MaterialData::isSupportedVersion() const
    {
        return schema_version == MATERIAL_SCHEMA_VERSION;
    }
}
