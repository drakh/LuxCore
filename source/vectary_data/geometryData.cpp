// #include "geometryData.h"
// #include "../utilities/log.h"

// GeometryData::GeometryData(const void *data, uint length)
// {
//     buffer = nullptr;
//     vertex_count = 0;
//     index_buffer = nullptr;

//     if (length % GEOMETRY_DATA_VERTEX_SIZE_BYTES != 0)
//     {
//         log_error("Geometry: invalid data length (vertex)");
//         return;
//     }
//     if (length % (GEOMETRY_DATA_VERTEX_SIZE_BYTES * 3) != 0)
//     {
//         log_error("Geometry: invalid data length (triangle)");
//         return;
//     }

//     float *float_data = (float *)data;
//     uint num_vertices = length / GEOMETRY_DATA_VERTEX_SIZE_BYTES;

//     vertex_count = num_vertices;
//     buffer = new float[num_vertices * GEOMETRY_DATA_VERTEX_SIZE + 1]; // + 1 is for safety for embree wide memory reads
//     buffer[num_vertices * GEOMETRY_DATA_VERTEX_SIZE] = 0.0f;
//     memcpy(buffer, data, length);
//     index_buffer = new uint[num_vertices];
//     for (uint i = 0; i < num_vertices; ++i)
//     {
//         index_buffer[i] = i; // TODO (danielis): this looks retarded, but embree requires indices, and maybe we could eventually send them...
//     }
// }

// GeometryAttributes GeometryData::get_vertex_attributes(uint vertex)
// {
//     uint idx = GEOMETRY_DATA_VERTEX_SIZE * vertex;
//     GeometryAttributes r;
//     constexpr uint P = GEOMETRY_DATA_POSITION_OFFSET;
//     r.position[0] = buffer[idx + P + 0];
//     r.position[1] = buffer[idx + P + 1];
//     r.position[2] = buffer[idx + P + 2];
//     constexpr uint N = GEOMETRY_DATA_NORMAL_OFFSET;
//     r.normal[0] = buffer[idx + N + 0];
//     r.normal[1] = buffer[idx + N + 1];
//     r.normal[2] = buffer[idx + N + 2];
//     constexpr uint U = GEOMETRY_DATA_UV_OFFSET;
//     r.uv[0] = buffer[idx + U + 0];
//     r.uv[1] = buffer[idx + U + 1];
//     constexpr uint T = GEOMETRY_DATA_TANGENT_OFFSET;
//     r.tangent[0] = buffer[idx + T + 0];
//     r.tangent[1] = buffer[idx + T + 1];
//     r.tangent[2] = buffer[idx + T + 2];
//     r.tangent[3] = buffer[idx + T + 3];
//     return r;
// }

// GeometryAttributes GeometryData::get_attributes(uint triangle, float u, float v, mat<4> *transform_matrix, mat<3> *normal_matrix)
// {
//     uint idx0 = GEOMETRY_DATA_VERTEX_SIZE * (triangle * 3 + 0);
//     uint idx1 = GEOMETRY_DATA_VERTEX_SIZE * (triangle * 3 + 1);
//     uint idx2 = GEOMETRY_DATA_VERTEX_SIZE * (triangle * 3 + 2);

//     float w = 1.0f - u - v;
//     // result.position = p0 * w0 + p1 * coords.x + p2 * coords.y;
//     GeometryAttributes r;
//     constexpr uint P = GEOMETRY_DATA_POSITION_OFFSET;
//     r.position[0] = buffer[idx0 + P + 0] * w + buffer[idx1 + P + 0] * u + buffer[idx2 + P + 0] * v;
//     r.position[1] = buffer[idx0 + P + 1] * w + buffer[idx1 + P + 1] * u + buffer[idx2 + P + 1] * v;
//     r.position[2] = buffer[idx0 + P + 2] * w + buffer[idx1 + P + 2] * u + buffer[idx2 + P + 2] * v;
//     constexpr uint N = GEOMETRY_DATA_NORMAL_OFFSET;
//     r.normal[0] = buffer[idx0 + N + 0] * w + buffer[idx1 + N + 0] * u + buffer[idx2 + N + 0] * v;
//     r.normal[1] = buffer[idx0 + N + 1] * w + buffer[idx1 + N + 1] * u + buffer[idx2 + N + 1] * v;
//     r.normal[2] = buffer[idx0 + N + 2] * w + buffer[idx1 + N + 2] * u + buffer[idx2 + N + 2] * v;
//     constexpr uint U = GEOMETRY_DATA_UV_OFFSET;
//     r.uv[0] = buffer[idx0 + U + 0] * w + buffer[idx1 + U + 0] * u + buffer[idx2 + U + 0] * v;
//     r.uv[1] = buffer[idx0 + U + 1] * w + buffer[idx1 + U + 1] * u + buffer[idx2 + U + 1] * v;
//     constexpr uint T = GEOMETRY_DATA_TANGENT_OFFSET;
//     r.tangent[0] = buffer[idx0 + T + 0] * w + buffer[idx1 + T + 0] * u + buffer[idx2 + T + 0] * v;
//     r.tangent[1] = buffer[idx0 + T + 1] * w + buffer[idx1 + T + 1] * u + buffer[idx2 + T + 1] * v;
//     r.tangent[2] = buffer[idx0 + T + 2] * w + buffer[idx1 + T + 2] * u + buffer[idx2 + T + 2] * v;
//     r.tangent[3] = buffer[idx0 + T + 3] * w + buffer[idx1 + T + 3] * u + buffer[idx2 + T + 3] * v;

//     // TODO: object matrix transformation
//     vec<4> p4 = *transform_matrix * vec<4>(r.position, fp(1));
//     r.position = vec<3>(p4[0], p4[1], p4[2]);
//     r.normal = normalize(*normal_matrix * r.normal);
//     vec<4> t4 = *transform_matrix * vec<4>(r.tangent[0], r.tangent[1], r.tangent[2], fp(0));
//     vec<3> t3 = normalize(vec<3>(t4[0], t4[1], t4[2]));
//     r.tangent = vec<4>(t3[0], t3[1], t3[2], r.tangent[3]);

//     return r;
// }

// GeometryData::~GeometryData()
// {
//     if (buffer)
//     {
//         delete[] buffer;
//     }
//     if (index_buffer)
//     {
//         delete[] index_buffer;
//     }
// }