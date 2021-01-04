#include "sh_args.h"
#include <cstring>
#include <common/utils/path.h>
#include <common/utils/process.h>
#include <common/log.h>
#include <wordexp.h>

CShareArgs::CShareArgs(int pid, const std::string &command, const std::string &env) {
    mPid = pid;

    wordExp(command, mArg);
    wordExp(env, mEnv);
}

bool CShareArgs::getLoaderArgs(CLoaderArgs &loaderArgs) {
    if (mArg.empty()) {
        LOG_ERROR("argument empty");
        return false;
    }

    loaderArgs.arg_count = mArg.size();
    loaderArgs.env_count = mEnv.size();

    char *arg = loaderArgs.arg;
    char *env = loaderArgs.env;

    for (const auto& a: mArg) {
        strcpy(arg, a.c_str());
        arg += a.length() + 1;
    }

    for (const auto& e: mEnv) {
        strcpy(env, e.c_str());
        env += e.length() + 1;
    }

    if (!loaderArgs.base_address && !getBaseAddress(loaderArgs.base_address)) {
        LOG_ERROR("find base address failed");
        return false;
    }

    if (!setAux(loaderArgs)) {
        LOG_ERROR("read auxv failed");
        return false;
    }

    return true;
}

bool CShareArgs::getBaseAddress(unsigned long& baseAddress) const {
    std::list<CProcessMap> processMaps;

    if (!CProcess::getProcessMaps(mPid, processMaps)) {
        LOG_ERROR("get process maps failed");
        return -1;
    }

    baseAddress = 0;

    for (const auto& m: processMaps) {
        if (m.start > 0x7f0000000000)
            break;

        baseAddress = m.end;
    }

    baseAddress += 0x1000000 - (baseAddress % 0x1000000);

    return true;
}

bool CShareArgs::setAux(CLoaderArgs &loaderArgs) const {
    std::string path = CPath::join("/proc", std::to_string(mPid), "auxv");
    std::ifstream ifs(path, std::ifstream::binary);

    if (!ifs.is_open())
        return false;

    ifs.read((char *)loaderArgs.auxv, sizeof(loaderArgs.auxv));

    return true;
}

bool CShareArgs::wordExp(const std::string &str, std::list<std::string>& words) {
    wordexp_t p = {};

    if (wordexp(str.c_str(), &p, WRDE_NOCMD) != 0) {
        LOG_ERROR("word exp '%s' failed", str.c_str());
        return false;
    }

    for (int i = 0; i < p.we_wordc; i++)
        words.emplace_back(p.we_wordv[i]);

    wordfree(&p);

    return true;
}
