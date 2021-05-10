//
// Created by Javier Cabrera on 2021-05-10.
//

#ifndef CROWMERGE_COMMON_H
#define CROWMERGE_COMMON_H

#endif //CROWMERGE_COMMON_H

#include <map>
#include <llvm/IR/GlobalValue.h>
#include <sys/stat.h>

using namespace llvm;

namespace crow_linker{

    static std::map<std::string, GlobalValue::LinkageTypes> backupLinkage4Functions;
    static std::map<std::string, GlobalValue::LinkageTypes> backupLinkage4Globals;
    static std::map<std::string, std::vector<std::string>> origingalVariantsMap;

    // Check if file exists
    bool exists (const std::string& name);
}