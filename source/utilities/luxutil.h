#pragma once

#include <luxcore/luxcore.h>
using namespace luxrays;
using namespace luxcore;

void scene_parse(Scene *scene, std::string str);
std::pair<unsigned char *, unsigned int> read_file_content(std::string filepath);