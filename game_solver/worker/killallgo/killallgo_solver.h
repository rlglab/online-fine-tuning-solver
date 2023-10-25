#pragma once

#include "base_solver.h"
#include "killallgo_knowledge_handler.h"
#include "killallgo_rzone_handler.h"
#include <memory>

namespace gamesolver {

class KillallGoSolver : public BaseSolver {
public:
    KillallGoSolver(uint64_t tree_node_size)
        : BaseSolver(tree_node_size) {}

private:
    inline std::shared_ptr<RZoneHandler> createRZoneHandler() override { return std::make_shared<KillallGoRZoneHandler>(getKnowledgeHandler()); }
    inline std::shared_ptr<KnowledgeHandler> createKnowledgeHandler() override { return std::make_shared<KillallGoKnowledgeHandler>(); }

    inline std::shared_ptr<KillallGoKnowledgeHandler> getKnowledgeHandler() { return std::static_pointer_cast<KillallGoKnowledgeHandler>(knowledge_handler_); }
};

} // namespace gamesolver
