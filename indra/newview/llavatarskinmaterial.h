/**
 * @file llavatarskinmaterial.h
 * @brief Viewer-local material and editable layer descriptions for native Avatar skin.
 *
 * This data contract is deliberately separate from LLMaterial and GLTF PBR.
 */

#ifndef LL_LLAVATARSKINMATERIAL_H
#define LL_LLAVATARSKINMATERIAL_H

#include "stdtypes.h"
#include "lluuid.h"
#include <set>
#include <vector>
#include "v4color.h"

namespace LLAvatarSkin
{
    static constexpr U32 MATERIAL_SCHEMA_VERSION = 1;
    static constexpr U32 LAYER_STACK_SCHEMA_VERSION = 1;
    static constexpr U32 MAX_ENABLED_SKIN_LAYERS = 16;

    enum class LayerType : U8
    {
        COLOR_OVERLAY = 0,
        MICRO_DETAIL,
        MAKEUP,
        TATTOO_OVERLAY,
        SURFACE_NORMAL,
        SURFACE_ROUGHNESS,
        SURFACE_SPECULAR,
        SUBSURFACE_MASK
    };

    enum class BlendMode : U8
    {
        NORMAL = 0,
        MULTIPLY,
        ADD,
        SOFT_LIGHT
    };

    // CPU-side logical layer descriptor. It owns UUID metadata only, never GPU resources.
    struct LayerDescriptor
    {
        LLUUID      layer_id;
        LLUUID      texture_id;
        LLUUID      mask_id;
        LayerType   type;
        BlendMode   blend_mode;
        F32         opacity;
        U32         order;
        bool        enabled;

        LayerDescriptor();

        // Disable invalid descriptors and clamp all user/content supplied values.
        void sanitize();
        bool isValid() const;
        void collectTextureIds(std::set<LLUUID>& ids) const;
    };

    // Ordered logical stack. Runtime rendering must flatten this into a bounded
    // set of composite textures rather than binding one sampler per descriptor.
    struct LayerStack
    {
        U32 schema_version;
        std::vector<LayerDescriptor> layers;

        LayerStack();

        void sanitize();
        bool isSupportedVersion() const;
        U32 getEnabledLayerCount() const;
        void collectTextureIds(std::set<LLUUID>& ids) const;
    };

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

        void sanitize();
        bool isSupportedVersion() const;
        void collectTextureIds(std::set<LLUUID>& ids) const;
    };
}

#endif // LL_LLAVATARSKINMATERIAL_H
