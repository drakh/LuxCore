#include "decoder.h"
#include "../utilities/log.h"
#include <algorithm>
#include <array>

using namespace luxrays;
using namespace luxcore;

BinaryDataHeader read_binary_data_header(void *data, unsigned int *byte_offset_after_read, unsigned int *remaining_length)
{
    *byte_offset_after_read += 16;
    *remaining_length -= 16;
    BinaryDataHeader header;
    header.client_id = ((unsigned int *)data)[0];
    header.type = BinaryDataType(((unsigned int *)data)[1]);
    header.message_index = ((unsigned int *)data)[2];
    header.update_data_index = ((int *)data)[3];
    return header;
}

void set_camera(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length)
{
    constexpr unsigned int CAMERA_EXPECTED_BYTES = 4 * (1 + 2 + 1 + (3 + 3 + 3) + 2); // focus_plane_distance + focus_plane_half_size + dof_size + position + lookat + up + fov + fov_h
    if (length != CAMERA_EXPECTED_BYTES)
    {
        log_error("Camera: invalid data length");
        return;
    }

    float *fdata = (float *)data;
    float focus_plane_distance = fdata[0];
    float focus_plane_half_size_x = fdata[1];
    float focus_plane_half_size_y = fdata[2];
    float dof_size = fdata[3];

    float cp_x = fdata[4];
    float cp_y = fdata[5];
    float cp_z = fdata[6];
    float ct_x = fdata[7];
    float ct_y = fdata[8];
    float ct_z = fdata[9];
    float cu_x = fdata[10];
    float cu_y = fdata[11];
    float cu_z = fdata[12];
    float fov = fdata[13];
    float fov_h = fdata[14];

    bool is_perspective = fov > 0.0001;

    Properties props = Properties();
    if (is_perspective)
    {
        props << Property("scene.camera.type")("perspective");
        props << Property("scene.camera.fieldofview")(std::max(fov, fov_h));
    }
    else
    {
        props << Property("scene.camera.type")("orthographic");
        props << Property("scene.camera.screenwindow")(-focus_plane_half_size_x, focus_plane_half_size_x, -focus_plane_half_size_y, focus_plane_half_size_y);
    }
    props << Property("scene.camera.lookat.orig")(cp_x, cp_y, cp_z);
    props << Property("scene.camera.lookat.target")(ct_x, ct_y, ct_z);
    props << Property("scene.camera.up")(cu_x, cu_y, cu_z);
    if (dof_size > 1e-3)
    {
        props << Property("scene.camera.lensradius")(dof_size);
        props << Property("scene.camera.focaldistance")(focus_plane_distance);
    }
    scene->Parse(props);
}

constexpr unsigned int GEOMETRY_DATA_POSITION_OFFSET = 0;
constexpr unsigned int GEOMETRY_DATA_NORMAL_OFFSET = (3);
constexpr unsigned int GEOMETRY_DATA_UV0_OFFSET = (3 + 3);
constexpr unsigned int GEOMETRY_DATA_UV1_OFFSET = (3 + 3 + 2);
constexpr unsigned int GEOMETRY_DATA_VERTEX_SIZE = (3 + 3 + 2 + 2);                     // (position + normal + uv0 + uv1)
constexpr unsigned int GEOMETRY_DATA_VERTEX_SIZE_BYTES = GEOMETRY_DATA_VERTEX_SIZE * 4; //  * float32_bytes

const std::string image_str = "image_";
const std::string image_str_clamp = "_clamp";
const std::string image_str_normal_flip = "_flipped";
const std::string texture_str = "texture_";
const std::string material_str = "material_";
const std::string geometry_str = "geometry_";
const std::string object_str = "object_";
const std::string light_str = "light_";

void clear_objects(DecoderData *decoder_data, Scene *scene)
{
    for (unsigned int i = 0; i < decoder_data->object_id; ++i)
    {
        scene->DeleteObject(object_str + std::to_string(i));
    }
    decoder_data->object_id = 0;
    for (unsigned int i = 0; i < decoder_data->light_id; ++i)
    {
        add_light_dummy(decoder_data, scene, i); // NOTE (danielis): for reasons unknown, delete light doesn't work on. so i re-define it with dummy light (and attempt to delete it anyway, in case it will get fixed)
        scene->DeleteLight(light_str + std::to_string(i));
    }
    decoder_data->light_id = 0;
}

std::string add_texture(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length, bool is_environment)
{
    constexpr unsigned int HEADER_LENGTH_BYTES = sizeof(unsigned int) * 5 + sizeof(float) * 2;
    unsigned int *header = (unsigned int *)data;
    unsigned int texture_id = header[0];
    unsigned int width = header[1];
    unsigned int height = header[2];
    TextureDataType type = TextureDataType(header[3]);
    unsigned int channels = header[4];
    float *headerf = (float *)data;
    float hdrmultiplier = headerf[5]; // for environment
    float hdrrotation = headerf[6];
    float gamma = headerf[5]; // for regular ass images
    bool is_normal_map = headerf[6] > 0.5f;

    unsigned int item_bytes = type == TextureDataType::UINT8 ? 1 : 4;
    unsigned int expected_length = HEADER_LENGTH_BYTES + width * height * channels * item_bytes;
    if (expected_length != length)
    {
        log_error("TextureData: invalid data length");
        return "";
    }

    void *buffer = nullptr;
    void *buffer_normal_flipped = nullptr;
    if (type == TextureDataType::UINT8)
    {
        buffer = malloc(width * height * channels);
        if (is_normal_map && !is_environment)
        {
            buffer_normal_flipped = malloc(width * height * channels);
        }
    }
    else if (type == TextureDataType::FLOAT32)
    {
        buffer = malloc(width * height * channels * sizeof(float));
    }
    else
    {
        log_error("TextureData: unknown data type");
        return "";
    }

    unsigned int line_bytes = width * channels * item_bytes;
    unsigned int pixel_bytes = channels * item_bytes;
    for (unsigned int i = 0; i < height; ++i)
    {
        if (is_environment)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                memcpy((char *)buffer + i * line_bytes + x * pixel_bytes, (char *)data + HEADER_LENGTH_BYTES + i * line_bytes + (width - 1 - x) * pixel_bytes, pixel_bytes);
            }
        }
        else
        {
            // flip y so we don't have to deal with it
            memcpy((char *)buffer + i * line_bytes, (char *)data + HEADER_LENGTH_BYTES + (height - 1 - i) * line_bytes, line_bytes);
        }
    }
    if (is_normal_map && !is_environment)
    {
        memcpy(buffer_normal_flipped, buffer, width * height * pixel_bytes);
        for (unsigned int x = 0; x < width; ++x)
        {
            for (unsigned int y = 0; y < height; ++y)
            {
                unsigned char *ny = (unsigned char *)buffer_normal_flipped + (((x + y * width) * pixel_bytes) + 1);
                *ny = 255 - *ny;
            }
        }
    }

    std::string imagename = image_str + std::to_string(decoder_data->image_id);
    if (is_environment)
    {
        imagename = "__environment_image";
    }

    if (type == TextureDataType::UINT8)
    {
        // FIXME (danielis): this is dumb. also flip is dumb. javascript should determine what it will actually need and only that should be defined
        scene->DefineImageMap(imagename, (unsigned char *)buffer, gamma, channels, width, height);
        scene->DefineImageMap(imagename + image_str_clamp, (unsigned char *)buffer, gamma, channels, width, height, luxcore::Scene::DEFAULT, luxcore::Scene::CLAMP);
        if (is_normal_map && !is_environment)
        {
            scene->DefineImageMap(imagename + image_str_normal_flip, (unsigned char *)buffer_normal_flipped, gamma, channels, width, height);
            scene->DefineImageMap(imagename + image_str_clamp + image_str_normal_flip, (unsigned char *)buffer_normal_flipped, gamma, channels, width, height, luxcore::Scene::DEFAULT, luxcore::Scene::CLAMP);
        }
    }
    else if (type == TextureDataType::FLOAT32)
    {
        scene->DefineImageMap(imagename, (float *)buffer, 1.0f, channels, width, height);
    }
    else
    {
        log_error("TextureData: unknown data type");
        return "";
    }

    if (is_environment)
    {
        std::string propstring = "scene.lights.__hdr.type = infinite \n";
        propstring += "scene.lights.__hdr.file = " + imagename + "\n";
        propstring += "scene.lights.__hdr.gain = " + std::to_string(hdrmultiplier) + " " + std::to_string(hdrmultiplier) + " " + std::to_string(hdrmultiplier) + "\n";
        propstring += "scene.lights.__hdr.gamma = 1.0 \n";
        float transform[16] = {
            cos(-hdrrotation), -sin(-hdrrotation), 0.0f, 0.0f,
            sin(-hdrrotation), cos(-hdrrotation), 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f};
        propstring += "scene.lights.__hdr.transformation = ";
        for (unsigned int i = 0; i < 16; ++i)
        {
            propstring += std::to_string(transform[i]) + " ";
        }
        propstring += "\n";
        scene_parse(scene, propstring);
    }
    else
    {
        ++decoder_data->image_id;
    }

    free(buffer);
    free(buffer_normal_flipped);

    return imagename;
}

std::string add_material(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length, int update_index)
{
    int use_material_index = update_index;
    if (update_index < 0)
    {
        use_material_index = decoder_data->material_id;
        ++decoder_data->material_id;
    }

    constexpr unsigned int MATERIAL_FLOAT_BYTES = 4 * 39;
    constexpr unsigned int TEXTURE_CONFIG_BYTES = 4 * (1 + 1 + 1 + 2 + 2 + 1); // id + uv + wrap + repeat + offset + rotation
    constexpr unsigned int TCONFIGS = (1 + 1 + 1 + 1 + 1 + 1);                 // albedo + opacity + roughness + metalness + emission_color + normal
    constexpr unsigned int MATERIAL_TEXTURE_CONFIG_BYTES = TEXTURE_CONFIG_BYTES * TCONFIGS;
    constexpr unsigned int MATERIAL_DATA_EXPECTED_BYTES = MATERIAL_FLOAT_BYTES + MATERIAL_TEXTURE_CONFIG_BYTES;
    if (length != MATERIAL_DATA_EXPECTED_BYTES)
    {
        log_error("MaterialData: invalid data length: " + std::to_string(MATERIAL_DATA_EXPECTED_BYTES) + " - " + std::to_string(length));
        return "";
    }

    float *fdata = (float *)data;

    float basecolor_x = fdata[0] * fdata[0];
    float basecolor_y = fdata[1] * fdata[1];
    float basecolor_z = fdata[2] * fdata[2];
    float opacity = fdata[3];
    float roughness = fdata[4];
    float metallic = fdata[5];
    float reflectivity = fdata[6];
    float emission_x = fdata[7] * fdata[7];
    float emission_y = fdata[8] * fdata[8];
    float emission_z = fdata[9] * fdata[9];
    float emission_p = fdata[10];
    float normal_scale = fdata[11];
    bool normal_flip = fdata[12] > 0.5f;
    float refractive = fdata[13];
    float ior = fdata[14];
    float refractive_absorption_depth = fdata[15];
    float refractive_absorption_color_x = fdata[16];
    float refractive_absorption_color_y = fdata[17];
    float refractive_absorption_color_z = fdata[18];
    float subsurface = fdata[19];
    float subsurface_radius = fdata[20];
    float subsurface_color_x = fdata[21];
    float subsurface_color_y = fdata[22];
    float subsurface_color_z = fdata[23];
    float subsurface_scale = fdata[24];
    bool is_shadowcatcher = fdata[25] > 0.5f;
    float speculartint_x = fdata[26];
    float speculartint_y = fdata[27];
    float speculartint_z = fdata[28];
    bool clearcoat_enabled = fdata[29] > 0.5f;
    float clearcoat_reflectivity = fdata[30];
    float clearcoat_roughness = fdata[31];
    float anisotropic = fdata[32];
    float sheen = fdata[33];
    float sheentint_x = fdata[34];
    float sheentint_y = fdata[35];
    float sheentint_z = fdata[36];

    float shadow_transparency = fdata[37];
    float cauchy_c = fdata[38];

    std::vector<std::string> pbr_lines;
    std::vector<std::string> glass_lines;
    std::vector<std::string> sss_lines;

    std::string propstring;

    bool is_glass = refractive > 1e-3;
    bool is_subsurface = !is_glass && subsurface > 1e-3;
    bool is_pbr = !(refractive >= 0.999 || (!is_glass && subsurface >= 0.999));
    bool is_light = is_pbr && emission_p > 1e-3;

    if (is_glass)
    {
        if (refractive > 1.5)
        {
            glass_lines.push_back(".type = archglass \n");
        }
        else if (roughness < 1e-3)
        {
            glass_lines.push_back(".type = glass \n");
        }
        else
        {
            glass_lines.push_back(".type = roughglass \n");
        }
        glass_lines.push_back(".kt = " + std::to_string(basecolor_x) + " " + std::to_string(basecolor_y) + " " + std::to_string(basecolor_z) + " \n");
        float gr = roughness * roughness;
        if (roughness >= 1e-3 && refractive <= 1.5)
        {
            glass_lines.push_back(".uroughness = " + std::to_string(gr) + " \n");
            glass_lines.push_back(".vroughness = " + std::to_string(gr) + " \n");
        }
        if (roughness < 1e-3)
        {
            glass_lines.push_back(".cauchyc = " + std::to_string(cauchy_c) + " \n");
        }
        glass_lines.push_back(".interiorior = " + std::to_string(ior) + " \n");

        // TODO: this could use its own settings
        float abs[3] = {refractive_absorption_color_x, refractive_absorption_color_y, refractive_absorption_color_z};
        for (unsigned int i = 0; i < 3; ++i)
        {
            abs[i] = (-log(std::max(abs[i], 1e-30f)) / refractive_absorption_depth); // * scale; // * (abs[i] == 1.0 and -1 or 1)
        }

        const std::string volume_str = "volume_glass_" + std::to_string(use_material_index);
        std::string vs = "scene.volumes." + volume_str;
        propstring += vs + ".type = clear \n";
        propstring += vs + ".absorption = " + std::to_string(abs[0]) + " " + std::to_string(abs[1]) + " " + std::to_string(abs[2]) + "\n";

        glass_lines.push_back(".volume.interior = " + volume_str + "\n");

        glass_lines.push_back(".transparency.shadow = " + std::to_string(shadow_transparency) + " " + std::to_string(shadow_transparency) + " " + std::to_string(shadow_transparency) + " \n");
    }
    if (is_subsurface)
    {
        float abs[3] = {subsurface_color_x, subsurface_color_y, subsurface_color_z};
        for (unsigned int i = 0; i < 3; ++i)
        {
            abs[i] = (-log(std::max(abs[i], 1e-30f)) / subsurface_radius); // * scale; // * (abs[i] == 1.0 and -1 or 1)
        }
        // float scat[3] = ...;

        sss_lines.push_back(".type = mattetranslucent \n");
        sss_lines.push_back(".kr = 0 0 0 \n");
        sss_lines.push_back(".kt = 1 1 1 \n");

        const std::string volume_str = "volume_sss_" + std::to_string(use_material_index);
        std::string vs = "scene.volumes." + volume_str;
        propstring += vs + ".type = homogeneous \n";
        propstring += vs + ".scattering = " + std::to_string(subsurface_scale) + " " + std::to_string(subsurface_scale) + " " + std::to_string(subsurface_scale) + "\n";
        propstring += vs + ".absorption = " + std::to_string(abs[0]) + " " + std::to_string(abs[1]) + " " + std::to_string(abs[2]) + "\n";
        propstring += vs + ".asymmetry = 0.0 0.0 0.0 \n";
        propstring += vs + ".multiscattering = 1 \n";

        sss_lines.push_back(".volume.interior = " + volume_str + "\n");
    }

    if (is_pbr)
    {
        pbr_lines.push_back(".type = disney \n");
        pbr_lines.push_back(".basecolor = " + std::to_string(basecolor_x) + " " + std::to_string(basecolor_y) + " " + std::to_string(basecolor_z) + "\n");
        pbr_lines.push_back(".roughness = " + std::to_string(roughness) + "\n");
        pbr_lines.push_back(".metallic = " + std::to_string(metallic) + "\n");
        pbr_lines.push_back(".specular = " + std::to_string(std::min(reflectivity * 25.0f, 1.0f)) + "\n");
        if (is_light)
        {
            pbr_lines.push_back(".emission = " + std::to_string(emission_x * emission_p) + " " + std::to_string(emission_y * emission_p) + " " + std::to_string(emission_z * emission_p) + "\n");
        }
        pbr_lines.push_back(".speculartint = " + std::to_string(speculartint_x) + " " + std::to_string(speculartint_y) + " " + std::to_string(speculartint_z) + "\n");
        // pbr_lines.push_back(".clearcoat = " + std::to_string(clearcoat) + "\n");
        // pbr_lines.push_back(".clearcoatgloss = " + std::to_string(clearcoatgloss) + "\n");
        pbr_lines.push_back(".anisotropic = " + std::to_string(anisotropic) + "\n");
        pbr_lines.push_back(".sheen = " + std::to_string(sheen) + "\n");
        pbr_lines.push_back(".sheentint = " + std::to_string(sheentint_x) + " " + std::to_string(sheentint_y) + " " + std::to_string(sheentint_z) + "\n");
    }

    std::string transparency_texture_name;
    // TextureConfig *configs[TCONFIGS] = {&albedo_texture, &opacity_texture, &roughness_texture, &metalness_texture, &emission_texture, &normal_texture};
    for (unsigned int i = 0; i < TCONFIGS; ++i)
    {
        void *tcdata = (void *)((char *)data + (MATERIAL_FLOAT_BYTES + TEXTURE_CONFIG_BYTES * i));
        int id = (int)((char *)tcdata)[0];
        int uv_channel = (int)((char *)tcdata)[4];
        TextureWrapping wrapping = (TextureWrapping)((char *)tcdata)[8];
        float repeat_x = ((float *)tcdata)[3];
        float repeat_y = ((float *)tcdata)[4];
        float offset_x = ((float *)tcdata)[5];
        float offset_y = ((float *)tcdata)[6];
        float rotation = ((float *)tcdata)[7];
        if (id != -1)
        {
            std::string texture_name = texture_str + std::to_string(decoder_data->texture_id) + (i == 0 ? "_base" : ""); // base color will use scale texture
            std::string ts = "scene.textures." + texture_name;
            std::string tpropstring;
            tpropstring += ts + ".type = imagemap \n";
            tpropstring += ts + ".file = " + image_str + std::to_string(id) + (wrapping == TextureWrapping::CLAMP ? image_str_clamp : "") + (i == 5 && normal_flip ? image_str_normal_flip : "") + "\n";
            tpropstring += ts + ".mapping.type = uvmapping2d \n";
            tpropstring += ts + ".mapping.uvscale = " + std::to_string(repeat_x) + " " + std::to_string(repeat_y) + "\n";
            tpropstring += ts + ".mapping.uvdelta = " + std::to_string(offset_x) + " " + std::to_string(offset_y) + "\n";
            tpropstring += ts + ".mapping.rotation = " + std::to_string(rotation) + "\n";

            if (i == 0) // add scale texture for base color
            {
                tpropstring += "scene.textures." + texture_str + std::to_string(decoder_data->texture_id) + ".type = scale \n";
                tpropstring += "scene.textures." + texture_str + std::to_string(decoder_data->texture_id) + ".texture1 = " + texture_name + " \n";
                tpropstring += "scene.textures." + texture_str + std::to_string(decoder_data->texture_id) + ".texture2 = " + std::to_string(basecolor_x) + " " + std::to_string(basecolor_y) + " " + std::to_string(basecolor_z) + "\n";

                texture_name = texture_str + std::to_string(decoder_data->texture_id);
            }
            else if (i == 1)
            {
                tpropstring += ts + ".gain = " + std::to_string(opacity) + "\n";
            }
            else if (i == 2)
            {
                tpropstring += ts + ".gain = " + std::to_string(roughness) + "\n";
            }
            else if (i == 3)
            {
                tpropstring += ts + ".gain = " + std::to_string(metallic) + "\n";
            }
            else if (i == 5)
            { // FIXME: this is only for bump, remove when using normal maps
                // tpropstring += ts + ".gain = " + std::to_string(normal_scale) + "\n";
            }

            scene_parse(scene, tpropstring);
            ++decoder_data->texture_id;

            if (i == 0)
            {
                if (is_pbr)
                {
                    pbr_lines.push_back(".basecolor = " + texture_name + "\n");
                }
            }
            else if (i == 1)
            {
                transparency_texture_name = texture_name;
            }
            else if (i == 2)
            {
                if (is_glass)
                {
                    glass_lines.push_back(".uroughness = " + texture_name + "\n");
                    glass_lines.push_back(".vroughness = " + texture_name + "\n");
                }
                if (is_pbr)
                {
                    pbr_lines.push_back(".roughness = " + texture_name + "\n");
                }
            }
            else if (i == 3)
            {
                if (is_pbr)
                {
                    pbr_lines.push_back(".metallic = " + texture_name + "\n");
                }
            }
            else if (i == 4)
            {
                if (is_pbr && is_light)
                {
                    pbr_lines.push_back(".emission = " + texture_name + "\n");
                    pbr_lines.push_back(".emission.gain = " + std::to_string(emission_x * emission_p) + " " + std::to_string(emission_y * emission_p) + " " + std::to_string(emission_z * emission_p) + "\n");
                }
            }
            else if (i == 5)
            {
                if (is_pbr)
                {
                    pbr_lines.push_back(".normaltex = " + texture_name + "\n");
                    pbr_lines.push_back(".normaltex.scale = " + std::to_string(normal_scale) + "\n");
                }
                if (is_glass)
                {
                    glass_lines.push_back(".normaltex = " + texture_name + "\n");
                    glass_lines.push_back(".normaltex.scale = " + std::to_string(normal_scale) + "\n");
                }
                if (is_subsurface)
                {
                    sss_lines.push_back(".normaltex = " + texture_name + "\n");
                    sss_lines.push_back(".normaltex.scale = " + std::to_string(normal_scale) + "\n");
                }
            }
            else
            {
                log_error("texture config: out of bounds");
            }
        }
    }

    std::string name = material_str + (clearcoat_enabled ? "base_" : "") + std::to_string(use_material_index);
    std::string mm = "scene.materials." + name;
    if (is_pbr)
    {
        std::string pbr_name = material_str + "pbr_" + std::to_string(use_material_index);
        std::string glass_name = material_str + "glass_" + std::to_string(use_material_index);
        std::string sss_name = material_str + "sss_" + std::to_string(use_material_index);
        if (is_glass)
        {
            for (size_t i = 0, I = pbr_lines.size(); i < I; ++i)
            {
                propstring += "scene.materials." + pbr_name + pbr_lines[i];
            }
            for (size_t i = 0, I = glass_lines.size(); i < I; ++i)
            {
                propstring += "scene.materials." + glass_name + glass_lines[i];
            }
            propstring += mm + ".type = mix \n";
            propstring += mm + ".material1 = " + pbr_name + "\n";
            propstring += mm + ".material2 = " + glass_name + "\n";
            propstring += mm + ".amount = " + std::to_string(refractive) + "\n";
        }
        else if (is_subsurface)
        {
            for (size_t i = 0, I = pbr_lines.size(); i < I; ++i)
            {
                propstring += "scene.materials." + pbr_name + pbr_lines[i];
            }
            for (size_t i = 0, I = sss_lines.size(); i < I; ++i)
            {
                propstring += "scene.materials." + sss_name + sss_lines[i];
            }
            propstring += mm + ".type = mix \n";
            propstring += mm + ".material1 = " + pbr_name + "\n";
            propstring += mm + ".material2 = " + sss_name + "\n";
            propstring += mm + ".amount = " + std::to_string(subsurface) + "\n";
        }
        else
        {
            for (size_t i = 0, I = pbr_lines.size(); i < I; ++i)
            {
                propstring += mm + pbr_lines[i];
            }
        }
    }
    else
    {
        if (is_glass)
        {
            for (size_t i = 0, I = glass_lines.size(); i < I; ++i)
            {
                propstring += mm + glass_lines[i];
            }
        }
        else if (is_subsurface)
        {
            for (size_t i = 0, I = sss_lines.size(); i < I; ++i)
            {
                propstring += mm + sss_lines[i];
            }
        }
    }

    if (is_shadowcatcher)
    {
        propstring += mm + ".shadowcatcher.enable = 1 \n";
    }
    propstring += mm + ".transparency = " + std::to_string(opacity) + "\n";
    if (transparency_texture_name != "")
    {
        propstring += mm + ".transparency = " + transparency_texture_name + "\n";
    }

    if (clearcoat_enabled)
    {
        std::string name_base = name;
        name = material_str + std::to_string(use_material_index);
        mm = "scene.materials." + name;
        propstring += mm + ".type = glossycoating \n";
        propstring += mm + ".base = " + name_base + "\n";
        propstring += mm + ".ks = " + std::to_string(clearcoat_reflectivity) + " " + std::to_string(clearcoat_reflectivity) + " " + std::to_string(clearcoat_reflectivity) + "\n";
        propstring += mm + ".uroughness = " + std::to_string(clearcoat_roughness) + "\n";
        propstring += mm + ".vroughness = " + std::to_string(clearcoat_roughness) + "\n";
    }

    scene_parse(scene, propstring);

    return name;
}

std::string add_geometry(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length)
{
    if (length % GEOMETRY_DATA_VERTEX_SIZE_BYTES != 0)
    {
        log_error("Geometry: invalid data length (vertex)");
        return "";
    }
    if (length % (GEOMETRY_DATA_VERTEX_SIZE_BYTES * 3) != 0)
    {
        log_error("Geometry: invalid data length (triangle)");
        return "";
    }

    float *float_data = (float *)data;
    unsigned int vertex_count = length / GEOMETRY_DATA_VERTEX_SIZE_BYTES;

    float *verts = scene->AllocVerticesBuffer(vertex_count);
    unsigned int *triangles = scene->AllocTrianglesBuffer(vertex_count / 3);
    std::array<float *, 8> *uvs = new std::array<float *, 8>();
    std::get<0>(*uvs) = new float[vertex_count * 2];
    std::get<1>(*uvs) = new float[vertex_count * 2];
    std::get<2>(*uvs) = nullptr;
    std::get<3>(*uvs) = nullptr;
    std::get<4>(*uvs) = nullptr;
    std::get<5>(*uvs) = nullptr;
    std::get<6>(*uvs) = nullptr;
    std::get<7>(*uvs) = nullptr;
    float *normals = new float[vertex_count * 3];

    for (unsigned int i = 0; i < vertex_count; ++i)
    {
        verts[i * 3 + 0] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 0 + GEOMETRY_DATA_POSITION_OFFSET];
        verts[i * 3 + 1] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 1 + GEOMETRY_DATA_POSITION_OFFSET];
        verts[i * 3 + 2] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 2 + GEOMETRY_DATA_POSITION_OFFSET];
        normals[i * 3 + 0] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 0 + GEOMETRY_DATA_NORMAL_OFFSET];
        normals[i * 3 + 1] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 1 + GEOMETRY_DATA_NORMAL_OFFSET];
        normals[i * 3 + 2] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 2 + GEOMETRY_DATA_NORMAL_OFFSET];
        std::get<0>(*uvs)[i * 2 + 0] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 0 + GEOMETRY_DATA_UV0_OFFSET];
        std::get<0>(*uvs)[i * 2 + 1] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 1 + GEOMETRY_DATA_UV0_OFFSET];
        std::get<1>(*uvs)[i * 2 + 0] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 0 + GEOMETRY_DATA_UV1_OFFSET];
        std::get<1>(*uvs)[i * 2 + 1] = float_data[i * GEOMETRY_DATA_VERTEX_SIZE + 1 + GEOMETRY_DATA_UV1_OFFSET];

        triangles[i] = i;
    }

    std::string name = geometry_str + std::to_string(decoder_data->geometry_id);
    // scene->DefineMesh(name, vertex_count, vertex_count / 3, verts, triangles, normals, (*uvs)[0], nullptr, nullptr);
    scene->DefineMeshExt(name, vertex_count, vertex_count / 3, verts, triangles, normals, uvs, nullptr, nullptr);

    ++decoder_data->geometry_id;
    return name;
}

void add_object(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length)
{
    constexpr unsigned int MESH_OBJECT_EXPECTED_BYTES = 4 * (4 + 16); // type + geometry_id + material_id + transform matrix
    if (length % MESH_OBJECT_EXPECTED_BYTES != 0)
    {
        log_error("MeshObject: invalid data length");
        return;
    }
    unsigned int num_objects = length / MESH_OBJECT_EXPECTED_BYTES;
    for (unsigned int index = 0; index < num_objects; ++index)
    {
        unsigned int *udata = (unsigned int *)data;
        // type = (MeshObjectType)udata[0];
        unsigned int geom_id = udata[1];
        unsigned int mat_id = udata[2];
        bool camera_invisible = udata[3] != 0;

        float *fdata = (float *)((char *)data + (4 * sizeof(unsigned int)));

        std::string name = object_str + std::to_string(decoder_data->object_id);
        std::string geometry_name = geometry_str + std::to_string(geom_id);
        std::string material_name = material_str + std::to_string(mat_id);

        std::string propstring = "scene.objects." + name + ".shape = " + geometry_name + "\n" + "scene.objects." + name + ".material = " + material_name + "\n";
        if (camera_invisible)
        {
            propstring += "scene.objects." + name + ".camerainvisible = 1 \n";
        }
        propstring += "scene.objects." + name + ".transformation = ";
        for (unsigned int i = 0; i < 16; ++i)
        {
            propstring += std::to_string(fdata[i]) + " ";
        }

        scene_parse(scene, propstring);

        ++decoder_data->object_id;

        data = (void *)((char *)data + MESH_OBJECT_EXPECTED_BYTES);
    }
}

void add_bake_objects(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length, Properties *props, bool combined, std::string filepath)
{
    constexpr unsigned int BAKE_OBJECT_EXPECTED_BYTES = 4; // object_id
    if (length % BAKE_OBJECT_EXPECTED_BYTES != 0)
    {
        log_error("BakeObject: invalid data length");
        return;
    }
    unsigned int num_objects = length / BAKE_OBJECT_EXPECTED_BYTES;

    std::string bakes = "";
    for (unsigned int index = 0; index < num_objects; ++index)
    {
        unsigned int *udata = (unsigned int *)data;

        bakes += "\"" + object_str + std::to_string(udata[0]) + "\" ";

        data = (void *)((char *)data + BAKE_OBJECT_EXPECTED_BYTES);
    }

    ////
    std::string mapstr = "bake.maps." + std::to_string(decoder_data->bakemap_id);
    (*props) << Property(mapstr + ".type")(combined ? "COMBINED" : "LIGHTMAP");
    // # Name of the bake map file
    (*props) << Property(mapstr + ".filename")(get_bake_image_filepath(filepath, decoder_data->bakemap_id));
    // # Index of the image pipeline to use for the output
    (*props) << Property(mapstr + ".imagepipelineindex")(0);
    // # If to use "autosize" map width/height
    // props << Property(mapstr + ".autosize.enabled")(1);
    // # Explicit selection of map width/height if "autosize" is disabled
    auto w = (*props).Get("film.width");
    (*props) << Property(mapstr + ".width")(w.GetValuesString());
    auto h = (*props).Get("film.height");
    (*props) << Property(mapstr + ".height")(h.GetValuesString());
    // # The index of the mesh UV coordinates to use for the baking process
    (*props) << Property(mapstr + ".uvindex")(1);
    // # A space separated list of scene objects names to bake (see the .scn file)
    (*props).SetFromString(mapstr + ".objectnames = " + bakes);

    ////

    decoder_data->bakemap_id++;
}

void add_light_dummy(DecoderData *decoder_data, Scene *scene, int index)
{
    std::string name = light_str + std::to_string(index);

    std::string prefix = "scene.lights. " + name;

    std::string propstring = prefix + ".type = distant \n";
    propstring += prefix + ".direction = 0 0 1 \n";
    propstring += prefix + ".color = 0 0 0 \n";
    propstring += prefix + ".theta = 0 \n";

    scene_parse(scene, propstring);
}

std::string add_light(DecoderData *decoder_data, Scene *scene, void *data, unsigned int length)
{
    constexpr unsigned int LIGHT_EXPECTED_BYTES = 4 * (3 + 29); // light type + object index + 30*random data
    if (length != LIGHT_EXPECTED_BYTES)
    {
        log_error("Light: invalid data length");
        return "";
    }

    std::string name = light_str + std::to_string(decoder_data->light_id);

    unsigned int *udata = (unsigned int *)data;
    PTLightType light_type = PTLightType(udata[0]);
    unsigned int object_index = udata[1];
    unsigned int visible_to_camera = udata[2] != 0;
    if (light_type == PTLightType::DIRECTIONAL)
    {
        float *fdata = (float *)((char *)data + (3 * sizeof(unsigned int)));

        std::string prefix = "scene.lights. " + name;

        std::string propstring = prefix + ".type = distant \n";
        propstring += prefix + ".direction = " + std::to_string(-fdata[0]) + " " + std::to_string(-fdata[1]) + " " + std::to_string(-fdata[2]) + "\n";
        propstring += prefix + ".color = " + std::to_string(fdata[3]) + " " + std::to_string(fdata[4]) + " " + std::to_string(fdata[5]) + "\n";
        propstring += prefix + ".theta = " + std::to_string(fdata[6]) + "\n";

        scene_parse(scene, propstring);
    }
    else if (light_type == PTLightType::SPHERE)
    {
        float *fdata = (float *)((char *)data + (3 * sizeof(unsigned int)));

        std::string prefix = "scene.lights. " + name;

        std::string propstring = prefix + ".type = sphere \n";
        propstring += prefix + ".position = " + std::to_string(fdata[0]) + " " + std::to_string(fdata[1]) + " " + std::to_string(fdata[2]) + "\n";
        propstring += prefix + ".radius = " + std::to_string(fdata[3]) + "\n";
        propstring += prefix + ".color = " + std::to_string(fdata[4]) + " " + std::to_string(fdata[5]) + " " + std::to_string(fdata[6]) + "\n";
        propstring += prefix + ".power = " + std::to_string(fdata[7]) + " \n";
        propstring += prefix + ".efficency = 1 \n";

        scene_parse(scene, propstring);
    }
    else if (light_type == PTLightType::SPOT)
    {
        float *fdata = (float *)((char *)data + (3 * sizeof(unsigned int)));

        std::string prefix = "scene.lights. " + name;

        std::string propstring = prefix + ".type = spot \n";
        propstring += prefix + ".position = " + std::to_string(fdata[0]) + " " + std::to_string(fdata[1]) + " " + std::to_string(fdata[2]) + "\n";
        propstring += prefix + ".target = " + std::to_string(fdata[3]) + " " + std::to_string(fdata[4]) + " " + std::to_string(fdata[5]) + "\n";

        propstring += prefix + ".color = " + std::to_string(fdata[6]) + " " + std::to_string(fdata[7]) + " " + std::to_string(fdata[8]) + "\n";
        propstring += prefix + ".power = " + std::to_string(fdata[9]) + "\n";
        propstring += prefix + ".efficency = 1 \n";

        propstring += prefix + ".coneangle = " + std::to_string(fdata[10]) + "\n";
        propstring += prefix + ".conedeltaangle = " + std::to_string(fdata[10] - fdata[11]) + "\n";

        scene_parse(scene, propstring);
    }
    else if (light_type == PTLightType::RECTANGLE || light_type == PTLightType::TUBE)
    {
        return ""; // don't report errors for this, this is handled as mesh light
    }
    else
    {
        log_error("unsupported light type: " + std::to_string(udata[0]));
        return "";
    }

    ++decoder_data->light_id;
    return name;
}


#include <thread>
#include <chrono>

std::pair<unsigned char *, unsigned int> get_baked_image(void *data, unsigned int length, std::string filepath)
{
    constexpr unsigned int EXPECTED_BYTES = 4;
    if (length != EXPECTED_BYTES)
    {
        log_error("Baked image: invalid data length");
        return std::make_pair(nullptr, 0);
    }

    // this is retarded, but sometimes file cannot be accessed, so try this
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    unsigned int id = ((unsigned int *)data)[0];
    return read_file_content(get_bake_image_filepath(filepath, id));
}

std::string get_bake_image_filepath(std::string filepath, unsigned int bakemap_id)
{
    return filepath + std::to_string(bakemap_id) + ".exr";
}
