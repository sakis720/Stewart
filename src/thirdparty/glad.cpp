#include <GLFW/glfw3.h>
#include <map>
#include <string>

static std::map<std::string, void*> g_Cache;

extern "C" void* glP_Get(const char* name) {
    auto it = g_Cache.find(name);
    if (it != g_Cache.end()) return it->second;
    void* ptr = (void*)glfwGetProcAddress(name);
    g_Cache[name] = ptr;
    return ptr;
}
