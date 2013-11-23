#ifndef _NTFA_HANDLERD_LOG_H
#define _NTFA_HANDLERD_LOG_H

#include <stdint.h>

#include <map>
#include <string>

void InitGenerationLog(std::map<std::string,uint32_t>& gen_map);
void LogGeneration(const std::string& handle);

#endif // _NTFA_HANDLERD_LOG_H
