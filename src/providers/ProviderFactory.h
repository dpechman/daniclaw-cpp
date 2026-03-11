#pragma once
#include "ILlmProvider.h"
#include <memory>
#include <string>

/**
 * Factory para instanciar provedores de LLM.
 */
class ProviderFactory {
public:
    static std::shared_ptr<ILlmProvider> create(const std::string& providerName = "");
};
