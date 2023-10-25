#pragma once

#include "base_solver.h"
#include "hex_knowledge_handler.h"
#include "hex_rzone_handler.h"
#include <memory>

namespace gamesolver {

class HexSolver : public BaseSolver {
public:
    HexSolver(uint64_t tree_node_size)
        : BaseSolver(tree_node_size) {}

private:
    inline std::shared_ptr<RZoneHandler> createRZoneHandler() override { return std::make_shared<HexRZoneHandler>(getKnowledgeHandler()); }
    inline std::shared_ptr<KnowledgeHandler> createKnowledgeHandler() override { return std::make_shared<HexKnowledgeHandler>(); }

    inline std::shared_ptr<HexKnowledgeHandler> getKnowledgeHandler() { return std::static_pointer_cast<HexKnowledgeHandler>(knowledge_handler_); }
};

} // namespace gamesolver
