/**
 * @file llavatarskinmaterial.h
 * @brief Viewer-local material description for traditional Avatar skin.
 *
 * This data contract is deliberately separate from LLMaterial and GLTF PBR.
 */

#ifndef LL_LLAVATARSKINMATERIAL_H
#define LL_LLAVATARSKINMATERIAL_H

#include "stdtypes.h"
#include "lluuid.h"
#include "v4color.h"

namespace LLAvatarSkin
{
    static constexpr U32 MATERIAL_SCHEMA_VERSION = 1;

    // CPU-side description only. Texture UUIDs are resolved by the viewer's
    // texture manager; this structure must never own GPU texture pointers.
    struct MaterialData
    {
        U32     schema_version;
        LLUUID  normal_map_id;
        LLUUID  roughness_map_id;
        LLUUID  specular_map_id;
        LLUUID  detail_mask_id;

        F32     roughness_factor;
        F32     specular_factor;
        F32     subsurface_strength;
        LLColor4 subsurface_color;
        F32     detail_strength;

        bool    enabled;

        MaterialData();

        // Clamp user/content supplied values to safe ranges.
        void sanitize();

        // Unknown schema versions must use the legacy Avatar material path.
        bool isSupportedVersion() const;
    };
}

#endif // LL_LLAVATARSKINMATERIAL_H
