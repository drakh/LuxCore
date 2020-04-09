//// windows shit
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
//// end of windows shit

#include <ctime>
#include <mutex>
#include <chrono>
#include <thread>

#include "vectary_data/decoder.h"
#include "utilities/log.h"

#include <stdlib.h>
#include <cstdlib>
#include <thread>
#include <algorithm>

#include <map>

#include <luxcore/luxcore.h>
using namespace luxrays;
using namespace luxcore;
using namespace std;

typedef unsigned int uint;

std::string version_string = "1.0.0";

constexpr uint CLIENT_INACTIVE_PAUSE_SEC = 40;
constexpr uint CLIENT_INACTIVE_DELETE_SEC = 240;

constexpr bool EXPORT_ON_START = false;
constexpr bool SAVE_IMAGE_ON_CANCEL = false;
constexpr bool SAVE_IMAGE_ON_DONE = false;

std::mutex client_data_cleaner_lock;

std::string filepath = "./";

struct ClientData
{
    char *version_return;

    Properties render_config_props;
    bool is_bake;
    RenderSession *render_session;
    RenderConfig *render_config;
    Scene *render_scene;

    void *image_response_data;
    uint image_response_data_length;
    float *image_response_color;

    DecoderData decoder_data;

    std::time_t last_message_time;

    ClientData()
    {
        version_return = (char *)malloc(5);

        is_bake = false;
        render_session = nullptr;
        render_config = nullptr;
        render_scene = nullptr;

        image_response_data = NULL;
        image_response_data_length = 0;
        image_response_color = NULL;

        decoder_data = DecoderData();

        last_message_time = std::time(nullptr);
    }

    ~ClientData()
    {
        free(version_return);
        delete render_scene;
        delete render_config;
        delete render_session;

        free(image_response_data);
        free(image_response_color);
    }
};

std::map<uint, ClientData *> client_data_map;

void clear_all(ClientData *client_data)
{
    if (client_data->render_session)
    {
        if (client_data->render_session->IsInSceneEdit())
        {
            client_data->render_session->EndSceneEdit();
        }
        if (client_data->render_session->IsStarted())
        {
            client_data->render_session->Stop();
            log("-- render stopped");
        }
    }

    delete client_data->render_session;
    delete client_data->render_config;
    delete client_data->render_scene;

    client_data->render_session = nullptr;
    client_data->render_config = nullptr;
    client_data->render_scene = nullptr;
    client_data->decoder_data.reset();
}

void pause(ClientData *client_data)
{
    if (client_data->render_session && client_data->render_session->IsStarted() && !client_data->render_session->IsInPause())
    {
        log("CLEANER: inactive session: pausing");
        client_data->render_session->Pause();
    }
}

void set_options(ClientData *client_data, const void *data, uint length)
{
    constexpr uint UINTs = 27;
    constexpr uint FLOATs = 14;
    constexpr uint OPTIONS_LENGTH_BYTES = (UINTs + FLOATs) * 4;
    if (length != OPTIONS_LENGTH_BYTES)
    {
        log_error("set_options: invalid data length");
        return;
    }

    uint *uidata = (uint *)data;

    uint width = uidata[0];
    uint height = uidata[1];
    uint halt_samples = uidata[2];
    uint halt_seconds = uidata[3];
    uint engine = uidata[4];
    uint sampler = uidata[5];
    uint dls_cache = uidata[6];

    uint pathdepth_total = uidata[7];
    uint pathdepth_diffuse = uidata[8];
    uint pathdepth_glossy = uidata[9];
    uint pathdepth_specular = uidata[10];

    uint denoise_numpixels = uidata[11];

    uint photon_cache_maxcount = uidata[12];
    uint photon_cache_maxdepth = uidata[13];
    bool photon_cache_indirect = uidata[14] != 0;
    uint photon_cache_indirect_maxsize = uidata[15];
    bool photon_cache_caustic = uidata[16] != 0;
    uint photon_cache_caustic_maxsize = uidata[17];
    uint photon_cache_caustic_lookupmaxcount = uidata[18];

    uint rt_zoomphase_size = uidata[19];

    bool subregion_on = uidata[20] != 0;
    uint subregion_x_start = uidata[21];
    uint subregion_x_stop = uidata[22];
    uint subregion_y_start = uidata[23];
    uint subregion_y_stop = uidata[24];

    PTFilmFilter filmfilter_type = PTFilmFilter(uidata[25]);
    uint filmfilter_width = uidata[26];

    float *fdata = (float *)data;
    float photon_cache_indirect_lookupradius = fdata[UINTs + 0];
    float photon_cache_indirect_normalangle = fdata[UINTs + 1];
    float photon_cache_indirect_usagethresholdscale = fdata[UINTs + 2];
    float photon_cache_indirect_glossinessthreshold = fdata[UINTs + 3];
    float photon_cache_caustic_lookupradius = fdata[UINTs + 4];
    float photon_cache_caustic_lookupnormalangle = fdata[UINTs + 5];

    float variance_clamping = fdata[UINTs + 6];

    bool bloom = fdata[UINTs + 7] > 0.5f;
    float bloom_size = fdata[UINTs + 8];
    float bloom_strength = fdata[UINTs + 9];

    float rt_zoomphase_weight = fdata[UINTs + 10];

    bool hybrid_enabled = fdata[UINTs + 11] > 0.5f;
    float hybrid_partition = fdata[UINTs + 12];
    float hybrid_glossinessthreshold = fdata[UINTs + 13];

    ////

    client_data->is_bake = false;

    Properties props = Properties();
    if (PTEngine(engine) == PTEngine::RTPATHCPU)
    {
        props << Property("renderengine.type")("RTPATHCPU");
        props << Property("sampler.type")("RTPATHCPUSAMPLER");
        props << Property("rtpathcpu.zoomphase.size")(rt_zoomphase_size);
        props << Property("rtpathcpu.zoomphase.weight")(rt_zoomphase_weight);
    }
    else if (PTEngine(engine) == PTEngine::BAKECPU)
    {
        client_data->is_bake = true;

        props << Property("renderengine.type")("BAKECPU");
        props << Property("sampler.type")(sampler ? "METROPOLIS" : "SOBOL");

        // props << Property("film.filter.type")("NONE");

        // props << Property("film.filter.type")("BOX");
        // props << Property("film.filter.width")(1);

        // props << Property("film.filter.type")("BLACKMANHARRIS");
        // props << Property("film.filter.width")(4);
    }
    else
    {
        props << Property("renderengine.type")(engine ? "BIDIRCPU" : "PATHCPU");
        props << Property("sampler.type")(sampler ? "METROPOLIS" : "SOBOL");
    }

    props << Property("film.width")(width);
    props << Property("film.height")(height);
    if (filmfilter_type != PTFilmFilter::DEFAULT)
    {
        std::string filter = "NONE";
        switch (filmfilter_type)
        {
        case PTFilmFilter::NONE:
            filter = "NONE";
            break;
        case PTFilmFilter::BOX:
            filter = "BOX";
            break;
        case PTFilmFilter::BLACKMANHARRIS:
            filter = "BLACKMANHARRIS";
            break;
        default:
            break;
        }
        props << Property("film.filter.type")(filter);
        props << Property("film.filter.width")(filmfilter_width);
    }
    props << Property("batch.halttime")(halt_seconds);
    props << Property("batch.haltspp")(halt_samples);

    if (subregion_on && PTEngine(engine) != PTEngine::BAKECPU)
    {
        props << Property("film.subregion")(subregion_x_start, subregion_x_stop, subregion_y_start, subregion_y_stop);
    }

    if (PTEngine(engine) == PTEngine::BAKECPU)
    {
        // #
        // # Image pipeline used to output the baked map (combined/lightmap/shadowcatcher) just use the same pipeline for everything and finish it in engine
        // #
        // props << Property("film.imagepipelines.0.0.type")("NOP");
        props << Property("film.imagepipelines.0.0.type")("TONEMAP_LINEAR");
        props << Property("film.imagepipelines.0.0.scale")(1.0);
        // props << Property("film.imagepipelines.0.2.type")("GAMMA_CORRECTION");
        // props << Property("film.imagepipelines.0.2.value")(2.2);
        // props << Property("film.imagepipelines.1.3.type")("INTEL_OIDN");

        props << Property("film.outputs.0.type")("RGBA_IMAGEPIPELINE");
        props << Property("film.outputs.0.index")(0);
        props << Property("film.outputs.0.filename")("./RGBA_IMAGEPIPELINE.png");
        // props << Property("film.outputs.1.type")("RGBA_IMAGEPIPELINE"); // OIDN still doesn't seem to work...
        // props << Property("film.outputs.1.index")(1);
        // props << Property("film.outputs.1.filename")("./RGBA_IMAGEPIPELINE.png");

        // ################################################################################
        // #
        // # General baking options
        // #
        // # Note: "autosize" is an option to automatically set the baked map width/height
        // # based on the mesh area. Small objects will have lower resolution maps and big
        // # will have high resolution maps.
        // #
        // # Set the minimum size of the baked map when "autosize" option is enabled
        props << Property("bake.minmapautosize")(512);
        // # Set the maximum size of the baked map when "autosize" option is enabled
        props << Property("bake.maxmapautosize")(1024);
        // # Set if I have to skip baked map rendering if the map file already
        // # exists. Useful to restart interrupted bake renderings from.
        props << Property("bake.skipexistingmapfiles")(0);
        // ################################################################################
        // props << Property("bake.margin")(4);
        // props << Property("bake.samplesthreshold")(halt_samples < 100000 ? halt_samples / 16 : 16);

        // # List of bake maps to render
        // #
        // // # Type of bake map to render: COMBINED or LIGHTMAP
        // props << Property("bake.maps.0.type")("LIGHTMAP");
        // // props << Property("bake.maps.0.type")("COMBINED");
        // // # Name of the bake map file
        // // props << Property("bake.maps.0.filename")("./lightmap.png");
        // props << Property("bake.maps.0.filename")("./combined.png");
        // // # Index of the image pipeline to use for the output
        // props << Property("bake.maps.0.imagepipelineindex")(1);
        // // # If to use "autosize" map width/height
        // // props << Property("bake.maps.0.autosize.enabled")(1);
        // // # Explicit selection of map width/height if "autosize" is disabled
        // props << Property("bake.maps.0.width")(width);
        // props << Property("bake.maps.0.height")(height);
        // // # The index of the mesh UV coordinates to use for the baking process
        // props << Property("bake.maps.0.uvindex")(1);
        // // # A space separated list of scene objects names to bake (see the .scn file)
    }
    else
    {
        props << Property("film.imagepipelines.0.0.type")("GAMMA_CORRECTION");
        props << Property("film.imagepipelines.0.0.value")(2.2);
        props << Property("film.imagepipelines.1.0.type")("INTEL_OIDN");
        props << Property("film.imagepipelines.1.0.numpixels")(denoise_numpixels);
        if (bloom)
        {
            props << Property("film.imagepipelines.1.1.type")("BLOOM");
            props << Property("film.imagepipelines.1.1.radius")(bloom_size);
            props << Property("film.imagepipelines.1.1.weight")(bloom_strength);
        }
        props << Property("film.imagepipelines.1.2.type")("GAMMA_CORRECTION");
        props << Property("film.imagepipelines.1.2.value")(2.2);

        props << Property("film.outputs.1.type")("RGBA_IMAGEPIPELINE");
        props << Property("film.outputs.1.filename")(filepath + "debug_output/_image.png");
    }

    if (dls_cache)
    {
        props << Property("lightstrategy.type")("DLS_CACHE");
    }
    props << Property("path.pathdepth.total")(pathdepth_total);
    props << Property("path.pathdepth.diffuse")(pathdepth_diffuse);
    props << Property("path.pathdepth.glossy")(pathdepth_glossy);
    props << Property("path.pathdepth.specular")(pathdepth_specular);

    props << Property("path.clamping.variance.maxvalue")(variance_clamping);

    props << Property("path.forceblackbackground.enable")(1);

    if (photon_cache_indirect || photon_cache_caustic)
    {
        props << Property("path.photongi.maxcount")(photon_cache_maxcount);
        props << Property("path.photongi.maxdepth")(photon_cache_maxdepth);
        if (photon_cache_indirect)
        {
            props << Property("path.photongi.indirect.enabled")(photon_cache_indirect);
            props << Property("path.photongi.indirect.maxsize")(photon_cache_indirect_maxsize);
            props << Property("path.photongi.indirect.lookup.radius")(photon_cache_indirect_lookupradius);
            props << Property("path.photongi.indirect.lookup.normalangle")(photon_cache_indirect_normalangle);
            props << Property("path.photongi.indirect.usagethresholdscale")(photon_cache_indirect_usagethresholdscale);
            props << Property("path.photongi.indirect.glossinessusagethreshold")(photon_cache_indirect_glossinessthreshold);
        }
        if (photon_cache_caustic)
        {
            props << Property("path.photongi.caustic.enabled")(photon_cache_caustic);
            props << Property("path.photongi.caustic.maxsize")(photon_cache_caustic_maxsize);
            props << Property("path.photongi.caustic.lookup.radius")(photon_cache_caustic_lookupradius);
            props << Property("path.photongi.caustic.lookup.maxcount")(photon_cache_caustic_lookupmaxcount);
            props << Property("path.photongi.caustic.lookup.normalangle")(photon_cache_caustic_lookupnormalangle);
        }
    }

    if (hybrid_enabled && PTEngine(engine) != PTEngine::BAKECPU) // BAKECPU doesn't work very well with hybrid (2020-02-17)
    {
        props << Property("path.hybridbackforward.enable")(1);
        props << Property("path.hybridbackforward.partition")(hybrid_partition); // TODO (danielis): set this to 0.0 if using OpenCL (PATHOCL). this will split forward/backward to CPU/GPU
        props << Property("path.hybridbackforward.glossinessthreshold")(hybrid_glossinessthreshold);
    }

    client_data->render_scene = Scene::Create();
    client_data->render_scene->SetDeleteMeshData(true);

    client_data->render_config_props = props;
}

void render_start(ClientData *client_data)
{
    // if (!client_data->render_config)
    // {
    //     log_error("render start requested without render config");
    //     return;
    // }
    if (client_data->render_session)
    {
        log_error("render start requested with already existing render session");
    }

    // FIXME (danielis): add baking props

    client_data->render_config = RenderConfig::Create(client_data->render_config_props, client_data->render_scene);

    client_data->render_session = RenderSession::Create(client_data->render_config);

    client_data->render_scene->RemoveUnusedMeshes();
    client_data->render_scene->RemoveUnusedMaterials();
    client_data->render_scene->RemoveUnusedTextures();
    client_data->render_scene->RemoveUnusedImageMaps();

    if (EXPORT_ON_START)
    {
        client_data->render_config->Export("./debug_output/");
        // client_data->render_scene->Save("scene");
    }

    client_data->render_session->Start();
    log("-- render started");
}

void render_cancel(ClientData *client_data)
{
    if (client_data->render_session)
    {
        if (client_data->render_session->IsStarted())
        {
            client_data->render_session->Stop();
            log("-- render stopped");
        }

        if (SAVE_IMAGE_ON_CANCEL)
        {
            const std::string renderEngine = client_data->render_config->GetProperty("renderengine.type").Get<std::string>();
            if (renderEngine != "FILESAVER")
            {
                client_data->render_session->GetFilm().SaveOutputs();
            }
        }

        clear_all(client_data);
    }
}

struct MessageResponse
{
    char *response;
    uint response_length;

    MessageResponse()
    {
        response = nullptr;
        response_length = 0;
    }
};

#define PASS_DATA_BODY_ONLY (char *)body + data_offset, remaining_length
#define PASS_DATA_BODY &client_data->decoder_data, client_data->render_scene, (char *)body + data_offset, remaining_length
#define BEGIN_SCENE_EDIT                                                                                                    \
    if (client_data->render_session && !client_data->render_session->IsInSceneEdit() && data_header.update_data_index >= 0) \
        client_data->render_session->BeginSceneEdit();
void process_message(void *body, uint length, MessageResponse *response)
{
    client_data_cleaner_lock.lock();

    uint data_offset = 0;
    uint remaining_length = length;
    BinaryDataHeader data_header = read_binary_data_header(body, &data_offset, &remaining_length);

    if (client_data_map.find(data_header.client_id) == client_data_map.end())
    {
        client_data_map[data_header.client_id] = new ClientData();
    }
    ClientData *client_data = client_data_map[data_header.client_id];

    // do not record handshakes as that might be another tab (or restarted tab) pinging it
    if (data_header.type != BinaryDataType::HANDSHAKE)
    {
        client_data->last_message_time = std::time(nullptr);
    }

    if (data_header.type == BinaryDataType::HANDSHAKE)
    {
        response->response_length = uint(version_string.size());

        free(client_data->version_return);
        client_data->version_return = (char *)malloc(version_string.size());
        memcpy(client_data->version_return, version_string.c_str(), version_string.size());

        response->response = client_data->version_return;
    }
    else if (data_header.type == BinaryDataType::RENDER_START)
    {
        try
        {
            render_start(client_data);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::RENDER_END)
    {
        try
        {
            render_cancel(client_data);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::RENDER_REQUEST_IMAGE)
    {
        if (client_data->render_session && client_data->render_session->IsStarted() && !client_data->render_session->IsInSceneEdit())
        {
            if (client_data->render_session->IsInPause())
            {
                client_data->render_session->Resume();
            }

            client_data->render_session->WaitNewFrame();

            const Properties &stats = client_data->render_session->GetStats();
            client_data->render_session->UpdateStats();
            const double elapsedTime = stats.Get("stats.renderengine.time").Get<double>();
            const unsigned int pass = stats.Get("stats.renderengine.pass").Get<unsigned int>();

            bool done = client_data->render_session->HasDone();
            uint total_size = 0;

            if (client_data->is_bake)
            {
                const uint HEADER_SIZE = 5 * 4; // is_bake, uint elapsed_time(ms), uint samples_done
                total_size = HEADER_SIZE;       // +imagedata

                if (client_data->image_response_data_length != total_size)
                {
                    free(client_data->image_response_data);
                    client_data->image_response_data = malloc(total_size);
                    client_data->image_response_data_length = total_size;
                    // delete[] client_data->image_response_color;
                    // client_data->image_response_color = new float[image_size];
                }

                ((uint *)client_data->image_response_data)[0] = client_data->is_bake ? 1 : 0;
                ((uint *)client_data->image_response_data)[1] = done ? 1 : 0;
                ((uint *)client_data->image_response_data)[2] = uint(elapsedTime * 1000);
                ((uint *)client_data->image_response_data)[3] = pass;

                if (done)
                {
                    log("-- baking done");
                }
            }
            else
            {
                unsigned int width = client_data->render_session->GetFilm().GetWidth();
                unsigned int height = client_data->render_session->GetFilm().GetHeight();

                // (char *)body + data_offset, remaining_length
                unsigned int image_type = (unsigned int)((char *)body + data_offset)[0];

                uint channels = 4;
                uint image_size = width * height * channels;
                const uint HEADER_SIZE = 7 * 4; // is_bake, is_done, uint elapsed_time(ms), uint samples_done, uint width, uint height, uint bytes per channel
                uint bytes_per_channel = 1;
                total_size = HEADER_SIZE + image_size * bytes_per_channel; // +imagedata

                if (client_data->image_response_data_length != total_size)
                {
                    free(client_data->image_response_data);
                    client_data->image_response_data = malloc(total_size);
                    client_data->image_response_data_length = total_size;
                    delete[] client_data->image_response_color;
                    client_data->image_response_color = new float[image_size];
                }

                if (done && SAVE_IMAGE_ON_DONE)
                {
                    const std::string renderEngine = client_data->render_config->GetProperty("renderengine.type").Get<std::string>();
                    if (renderEngine != "FILESAVER")
                    {
                        client_data->render_session->GetFilm().SaveOutputs();
                    }
                }

                ((uint *)client_data->image_response_data)[0] = client_data->is_bake ? 1 : 0;
                ((uint *)client_data->image_response_data)[1] = done ? 1 : 0;
                ((uint *)client_data->image_response_data)[2] = uint(elapsedTime * 1000);
                ((uint *)client_data->image_response_data)[3] = pass;
                ((uint *)client_data->image_response_data)[4] = width;
                ((uint *)client_data->image_response_data)[5] = height;
                ((uint *)client_data->image_response_data)[6] = bytes_per_channel;

                try
                {
                    client_data->render_session->GetFilm().GetOutput(Film::OUTPUT_RGBA_IMAGEPIPELINE, client_data->image_response_color, image_type);
                }
                catch (const std::exception &e)
                {
                    log_error(e.what());
                }
                for (unsigned int i = 0; i < image_size; ++i)
                {
                    unsigned int w = (i / channels) % width;
                    unsigned int h = height - 1 - ((i / channels) / width);
                    float v = client_data->image_response_color[(w + h * width) * channels + (i % channels)];
                    ((unsigned char *)(client_data->image_response_data))[HEADER_SIZE + i] = (unsigned char)(round(v * 255.0f));
                    // ((float *)(client_data->image_response_data))[HEADER_SIZE / 4 + i] = v;
                }
            }
            response->response_length = total_size;
            response->response = (char *)client_data->image_response_data;
            ////

            // update time also here because denoiser can take a long time on huge resolutions
            client_data->last_message_time = std::time(nullptr);
        }
    }
    else if (data_header.type == BinaryDataType::BAKE_REQUEST_IMAGE)
    {
        log("-- requested baked image");
        std::pair<unsigned char *, unsigned int> s = get_baked_image(PASS_DATA_BODY_ONLY, filepath);
        if (s.first)
        {
            free(client_data->image_response_data);
            client_data->image_response_data = s.first;

            response->response_length = s.second;
            response->response = (char *)client_data->image_response_data;
        }
    }
    else if (data_header.type == BinaryDataType::CLEAR_ALL)
    {
        clear_all(client_data);
    }
    else if (data_header.type == BinaryDataType::CLEAR_OBJECTS)
    {
        if (client_data->render_session && !client_data->render_session->IsInSceneEdit())
        {
            client_data->render_session->BeginSceneEdit();
        }
        clear_objects(&client_data->decoder_data, client_data->render_scene);
    }
    else if (data_header.type == BinaryDataType::END_SCENE_EDIT)
    {
        if (client_data->render_session && client_data->render_session->IsInSceneEdit())
        {
            client_data->render_session->EndSceneEdit();
        }
    }
    else if (data_header.type == BinaryDataType::OPTIONS)
    {
        try
        {
            set_options(client_data, PASS_DATA_BODY_ONLY);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::GEOMETRY)
    {
        try
        {
            add_geometry(PASS_DATA_BODY);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::TEXTURE || data_header.type == BinaryDataType::ENVIRONMENT)
    {
        try
        {
            BEGIN_SCENE_EDIT
            add_texture(PASS_DATA_BODY, data_header.type == BinaryDataType::ENVIRONMENT);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::MATERIAL)
    {
        try
        {
            BEGIN_SCENE_EDIT
            add_material(PASS_DATA_BODY, data_header.update_data_index);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::MESH_OBJECT)
    {
        try
        {
            add_object(PASS_DATA_BODY);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::LIGHT)
    {
        try
        {
            add_light(PASS_DATA_BODY);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::BAKE_LIGHTMAP_OBJECTS || data_header.type == BinaryDataType::BAKE_COMBINED_OBJECTS)
    {
        try
        {
            add_bake_objects(PASS_DATA_BODY, &client_data->render_config_props, data_header.type == BinaryDataType::BAKE_COMBINED_OBJECTS, filepath);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else if (data_header.type == BinaryDataType::CAMERA)
    {
        try
        {
            BEGIN_SCENE_EDIT
            set_camera(PASS_DATA_BODY);
        }
        catch (const std::exception &e)
        {
            log_error(e.what());
        }
    }
    else
    {
        log_error("unhandled binary data type: " + std::to_string(uint(data_header.type)));
    }

    client_data_cleaner_lock.unlock();
}

// // +-------------------------------------------------+
// // |                                                 |
// // |     HTTP_SERVER_SYNC ::: BOOST:BEAST            |
// // |                                                 |
// // +-------------------------------------------------+
// //
// // Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// //
// // Distributed under the Boost Software License, Version 1.0. (See accompanying
// // file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
// //
// // Official repository: https://github.com/boostorg/beast
// //

// //------------------------------------------------------------------------------
// //
// // Example: HTTP server, synchronous
// //
// //------------------------------------------------------------------------------

void client_data_cleaner_function()
{
    while (true)
    {
        client_data_cleaner_lock.lock();

        std::time_t now = std::time(nullptr);

        for (auto it = client_data_map.begin(); it != client_data_map.end(); it++)
        {
            ClientData *client_data = it->second;
            if (now - client_data->last_message_time > CLIENT_INACTIVE_DELETE_SEC)
            {
                log("CLEANER: inactive session: deleting");
                clear_all(client_data);
                client_data_map.erase(it);
                delete client_data;
                break;
            }
            else if (now - client_data->last_message_time > CLIENT_INACTIVE_PAUSE_SEC)
            {
                pause(client_data);
            }
        }

        client_data_cleaner_lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
}

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <boost/optional/optional_io.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template <class Stream>
struct send_lambda
{
    Stream &stream_;
    bool &close_;
    beast::error_code &ec_;

    explicit send_lambda(
        Stream &stream,
        bool &close,
        beast::error_code &ec)
        : stream_(stream), close_(close), ec_(ec)
    {
    }

    template <bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields> &&msg) const
    {
        // Determine if we should close the connection after
        close_ = msg.need_eof();

        // We need the serializer here because the serializer requires
        // a non-const file_body, and the message oriented version of
        // http::write only works with const messages.
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::write(stream_, sr, ec_);
    }
};

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
void handle_request(
    beast::string_view doc_root,
    http::request<http::string_body> &req,
    send_lambda<tcp::socket> &send)
{
    // process post request, that's what we want for PT
    if (req.method() == http::verb::post)
    {
        auto body = req.body();
        MessageResponse response;
        process_message((void *)body.c_str(), (uint)(body.size()), &response);

        std::string restr;
        for (uint i = 0; i < response.response_length; ++i)
        {
            restr.push_back(response.response[i]);
        }

        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::access_control_allow_credentials, "true");
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "POST");

        // res.set(http::field::content_type, mime_type(path));
        res.body() = restr;
        res.prepare_payload();
        // res.keep_alive(req.keep_alive());
        send(std::move(res));
    }
}

//------------------------------------------------------------------------------

// Report a failure
void fail(beast::error_code ec, char const *what)
{
    std::stringstream ss;
    ss << what << ": " << ec.message();
    log_error(ss.str());
}

// Handles an HTTP server connection
void do_session(
    tcp::socket &socket,
    std::shared_ptr<std::string const> const &doc_root)
{
    bool close = false;
    beast::error_code ec;

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    send_lambda<tcp::socket> lambda{socket, close, ec};

    for (;;)
    {
        // Read a request
        http::request_parser<http::string_body> parser;
        // Allow for an unlimited body size
        parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
        http::read(socket, buffer, parser, ec);
        http::message<true, http::string_body> msg = parser.get();

        if (ec == http::error::end_of_stream)
            break;
        if (ec)
            return fail(ec, "read");

        // Send the response
        handle_request(*doc_root, msg, lambda);
        if (ec)
            return fail(ec, "write");
        if (close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
        }
    }

    // Send a TCP shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if (version_string != "1.0.0")
    {
        std::cout << "initial version string should be \"1.0.0\"" << std::endl;
    }

    luxcore::Init();

    std::thread cleaner_thread = std::thread(client_data_cleaner_function);

    auto port = static_cast<unsigned short>(14024);
    for (int i = 0; i < argc; ++i)
    {
        if ((!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")))
        {
            std::cout << "Use --version or -v to specify version to be communicated to connecting application" << std::endl;
            std::cout << "Use --port or -p to specify port on which to listen" << std::endl;
            std::cout << "Use --filepath or -f to specify path to folder where path tracer can write temporary files" << std::endl;
        }
        if ((!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) && i + 1 < argc)
        {
            version_string = argv[i + 1];
            std::cout << "Using version: " + version_string << std::endl;
        }
        if ((!strcmp(argv[i], "--port") || !strcmp(argv[i], "-p")) && i + 1 < argc)
        {
            port = static_cast<unsigned short>(std::atoi(argv[i + 1]));
            std::cout << "listening on port: " + std::to_string(port) << std::endl;
        }
        if ((!strcmp(argv[i], "--filepath") || !strcmp(argv[i], "-f")) && i + 1 < argc)
        {
            filepath = argv[i + 1];
            if (filepath[filepath.size() - 1] != '/' && filepath[filepath.size() - 1] != '\\')
            {
                filepath += "/";
            }
            std::cout << "using path to write files: " + filepath << std::endl;
        }
    }

    log_start(filepath);

    try
    {
        auto const address = net::ip::make_address("127.0.0.1");
        auto const doc_root = std::make_shared<std::string>(".");

        // The io_context is required for all I/O
        net::io_context ioc{1};

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};
        for (;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{std::bind(
                            &do_session,
                            std::move(socket),
                            doc_root)}
                .detach();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
