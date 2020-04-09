#pragma once

#include <luxcore/luxcore.h>
using namespace luxrays;
using namespace luxcore;

#include "../utilities/luxutil.h"

enum class BinaryDataType : unsigned int
{
    UNKNOWN = 0,
    CLEAR_ALL = 1,
    OPTIONS = 2,
    GEOMETRY = 11,
    TEXTURE = 12,
    MATERIAL = 13,
    MESH_OBJECT = 14,
    ENVIRONMENT = 15,
    CAMERA = 16,
    LIGHT = 17,

    BAKE_LIGHTMAP_OBJECTS = 18,
    BAKE_COMBINED_OBJECTS = 19,
    BAKE_REQUEST_IMAGE = 20,

    RENDER_START = 101,
    RENDER_END = 102,
    RENDER_REQUEST_IMAGE = 103,
    CLEAR_OBJECTS = 104,
    END_SCENE_EDIT = 105,

    HANDSHAKE = 1001
};

enum class PTEngine : unsigned int
{
    PATHCPU = 0,
    BIDIR = 1,
    RTPATHCPU = 2,
    BAKECPU = 3
};

enum class PTFilmFilter : unsigned int
{
    DEFAULT = 0,
    BLACKMANHARRIS = 1,
    BOX = 2,
    NONE = 3,
};

enum class PTSampler : unsigned int
{
    SOBOL = 0,
    METROPOLIS = 1
};

enum class TextureDataType : unsigned int
{
    UNKNOWN = 0,
    UINT8 = 1,
    FLOAT32 = 2
};

enum class TextureWrapping : unsigned int
{
    REPEAT = 0,
    MIRRORED_REPEAT = 1,
    CLAMP = 2
};

enum class PTLightType : unsigned int
{
    UNKNOWN = 0,
    SPHERE = 1,
    TUBE = 2,
    RECTANGLE = 3,
    MESH = 4,
    SPOT = 5,
    DIRECTIONAL = 6
};

struct DecoderData
{
    unsigned int image_id;
    unsigned int texture_id;
    int material_id;
    unsigned int geometry_id;
    unsigned int object_id;
    unsigned int light_id;

    unsigned int bakemap_id;

    void reset()
    {
        image_id = 0;
        texture_id = 0;
        material_id = 0;
        geometry_id = 0;
        object_id = 0;
        light_id = 0;

        bakemap_id = 0;
    }

    DecoderData()
    {
        reset();
    }
};

struct BinaryDataHeader
{
    unsigned int client_id;
    BinaryDataType type;
    unsigned int message_index;
    int update_data_index;
};

BinaryDataHeader read_binary_data_header(void *data, unsigned int *byte_offset_after_read, unsigned int *remaining_length);
void set_camera(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length);
void clear_objects(DecoderData *decoder_data, Scene *scene);
std::string add_texture(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length, bool is_environment);
std::string add_material(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length, int update_index);
std::string add_geometry(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length);
void add_object(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length);
void add_bake_objects(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length, Properties *props, bool combined, std::string filepath);
void add_light_dummy(DecoderData *decoder_data, Scene *scene, int index);
std::string add_light(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length);
std::pair<unsigned char *, unsigned int> get_baked_image(void *data, unsigned int length, std::string filepath);
std::string get_bake_image_filepath(std::string filepath, unsigned int bakemap_id);