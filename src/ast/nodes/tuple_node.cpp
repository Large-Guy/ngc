//
// Created by Ravi Lebgue on 3/18/26.
//

#include "tuple_node.h"

#include "../../memory_utils.h"

TupleNode::TupleNode(std::vector<std::unique_ptr<ExpressionNode> > elements) : elements(std::move(elements)) {
}

std::unique_ptr<AstNode> TupleNode::Clone() const {
    std::vector<std::unique_ptr<ExpressionNode> > items;
    for (const auto& item: elements) {
        items.push_back(UniqueCast<ExpressionNode>(item->Clone()));
    }
    return std::make_unique<TupleNode>(std::move(items));
}
