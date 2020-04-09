#include "luxutil.h"
#include "../utilities/log.h"

void scene_parse(Scene *scene, std::string str)
{
    Properties props;
    props.SetFromString(str);
    try
    {
        scene->Parse(props);
    }
    catch (const std::exception &e)
    {
        log_error(e.what());
    }
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

std::pair<unsigned char *, unsigned int> read_file_content(std::string filepath)
{
    log("...reading: " + filepath);
    struct stat statbuf;
    stat(filepath.c_str(), &statbuf);
    log("...size: " + std::to_string(statbuf.st_size));
    unsigned char *mem = (unsigned char *)malloc(statbuf.st_size);
    // int file = fopen(filepath.c_str(), O_RDWR , 0600);
    FILE *f = fopen(filepath.c_str(), "rb");
    if( f ){
        fread(mem, 1, statbuf.st_size, f);
        fclose(f);
    } else {
        log_error("failed to open file" + std::string(strerror(errno)));
    }
    return std::make_pair(mem, statbuf.st_size);
}
