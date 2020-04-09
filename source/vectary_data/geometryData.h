// #pragma once

// #include "../math/vector.h"
// #include "../math/matrix.h"

// constexpr uint GEOMETRY_DATA_POSITION_OFFSET = 0;
// constexpr uint GEOMETRY_DATA_POSITION_OFFSET_BYTES = 0;
// constexpr uint GEOMETRY_DATA_NORMAL_OFFSET = (3);
// constexpr uint GEOMETRY_DATA_NORMAL_OFFSET_BYTES = (3) * 4;
// constexpr uint GEOMETRY_DATA_UV_OFFSET = (3 + 3);
// constexpr uint GEOMETRY_DATA_UV_OFFSET_BYTES = (3 + 3) * 4;
// constexpr uint GEOMETRY_DATA_TANGENT_OFFSET = (3 + 3 + 2);
// constexpr uint GEOMETRY_DATA_TANGENT_OFFSET_BYTES = (3 + 3 + 2) * 4;
// constexpr uint GEOMETRY_DATA_VERTEX_SIZE = (3 + 3 + 2 + 4);                     // (position + normal + uv + tangent)
// constexpr uint GEOMETRY_DATA_VERTEX_SIZE_BYTES = GEOMETRY_DATA_VERTEX_SIZE * 4; //  * float32_bytes

// struct GeometryAttributes 
// {
//     vec<3> position;
//     vec<3> normal;
//     vec<2> uv;
//     vec<4> tangent;
// };

// class GeometryData
// {
//   public:
//     float *buffer;
//     uint vertex_count;
//     uint *index_buffer;

//   public:
//     GeometryData(const void *data, uint length);

//     GeometryAttributes get_vertex_attributes(uint vertex);
//     GeometryAttributes get_attributes(uint triangle, float u, float v, mat<4> *transform_matrix, mat<3> *normal_matrix);

//     ~GeometryData();
// };