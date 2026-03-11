#include "BaseTool.h"
#include <sstream>

std::string BaseTool::toSchemaJson() const {
    std::ostringstream oss;
    oss << R"({"name":")" << name()
        << R"(","description":")" << description()
        << R"("})";
    return oss.str();
}
