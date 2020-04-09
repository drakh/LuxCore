#include "log.h"
#include <fstream>
#include <mutex>

std::mutex lock;
std::string log_path;

void log_start(std::string path)
{
    log_path = path;

    lock.lock();
    std::ofstream outFile(log_path + "path_tracer.log");
    outFile << "# Starting log" << std::endl;
    outFile.close();
    std::cout << "# Starting log" << std::endl;
    lock.unlock();
}

void log(std::string message)
{
    lock.lock();
    std::ofstream outFile(log_path + "path_tracer.log", std::ofstream::app);
    outFile <<  message << std::endl;
    outFile.close();
    std::cout << message << std::endl;
    lock.unlock();
}

void log_error(std::string message)
{
    log("# ERROR: " + message);
}
