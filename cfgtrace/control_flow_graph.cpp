#include "cfgtrace/control_flow_graph.h"
#include "cfgtrace/command/execute.h"
#include "cfgtrace/engine/engine.h"
#include "cfgtrace/error/error.h"
#include "cfgtrace/instruction.h"
#include "cfgtrace/random/random.h"
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;

// digraph_prefix template
constexpr const char *digraph_prefix = R"(
digraph control_flow_graph {
	node [
		shape = box 
		color = black
		arrowhead = diamond
		style = filled
		fontname = "Source Code Pro"
		arrowtail = normal
	]	
)";

string control_flow_graph::graphviz()
{
    this->set_nodes_max_occurrences();

    string definitions = "";
    for (const auto &item : this->nodes)
        definitions += item.second->graphviz_definition();

    auto digraph = digraph_prefix + definitions;

    for (const auto &item : this->nodes)
        digraph += item.second->graphviz_relation();

    return digraph + "\n}";
}

void control_flow_graph::load_to_memory(uint8_t *mem) const noexcept
{
    memcpy(mem, &this->start_address_first_node, sizeof(this->start_address_first_node));
    mem += sizeof(this->start_address_first_node);

    const size_t n = this->nodes.size();
    memcpy(mem, &n, sizeof(n));
    mem += sizeof(n);

    for (const auto &item : this->nodes) {
        memcpy(mem, &item.first, sizeof(item.first));
        mem += sizeof(item.first);
        item.second->load_to_memory(mem);
        mem += item.second->mem_size();
    }
}

void control_flow_graph::load_from_memory(const uint8_t *mem) noexcept
{
    memcpy(&this->start_address_first_node, mem, sizeof(this->start_address_first_node));
    mem += sizeof(start_address_first_node);

    size_t n = 0;
    memcpy(&n, mem, sizeof(n));
    mem += sizeof(n);

    for (auto i = 0u; i < n; i++) {
        size_t key = 0;
        memcpy(&key, mem, sizeof(key));
        mem += sizeof(key);

        auto node = make_unique<Node>();
        node->load_from_memory(mem);
        mem += node->mem_size();

        this->nodes[key] = move(node);
    }
}

void control_flow_graph::generate(const string content, ostream *out, int it) const
{
    (*out) << content << endl;

    auto name = to_string(it) + "_" + random::string();

    const string cmd = "dot -Tpng partiaflowgraph.dot -o" + name + ".png";
    string process_stderr, process_exit;

    command::execute(cmd, &process_stderr, &process_exit);

    string exception_message = "";
    if (!process_stderr.empty())
        exception_message += process_exit;

    if (!process_exit.empty())
        exception_message += " " + process_exit;

    if (!exception_message.empty())
        throw ex(runtime_error, exception_message);
}

bool control_flow_graph::node_exists(size_t address) const noexcept
{
    return (this->nodes.find(address) != this->nodes.end());
}

unique_ptr<Node> control_flow_graph::get_current_node(size_t start_address) noexcept
{
    if (this->node_exists(start_address))
        return move(this->nodes[start_address]);
    return make_unique<Node>(start_address);
}

bool control_flow_graph::node_contains_address(size_t address) const noexcept
{
    for (const auto &item : this->nodes)
        if (item.second->contains_address(address))
            return true;

    return false;
}

size_t control_flow_graph::set_and_get_current_address(size_t eip) noexcept
{
    if (this->current_node_start_addr == 0)
        this->current_node_start_addr = eip;

    if (this->current_pointer == 0)
        this->current_pointer = eip;

    return this->current_pointer;
}

void control_flow_graph::append_instruction(instruction instruction)
{
    if (!instruction.validate())
        throw ex(invalid_argument, "invalid instruction passed");

    if (instruction.is_branch())
        throw ex(invalid_argument, "cannot append instruction that is branch");

    size_t current = this->set_and_get_current_address(instruction.pointer_address());
    auto node = this->get_current_node(current);
    node->append_instruction(instruction);
    this->nodes[current] = move(node);
}

void control_flow_graph::append_node_neighbors(const unique_ptr<Node> &node) noexcept
{
    size_t true_address = node->true_neighbour();
    size_t false_address = node->false_neighbour();

    if (true_address != 0) {
        auto true_node = get_current_node(true_address);
        this->nodes[true_address] = move(true_node);
    }

    if (false_address != 0) {
        auto false_node = get_current_node(false_address);
        this->nodes[false_address] = move(false_node);
    }
}

void control_flow_graph::append_branch_instruction(instruction instruction)
{
    if (!instruction.validate())
        throw ex(invalid_argument, "invalid instruction passed");

    if (!instruction.is_branch())
        throw ex(invalid_argument, "cannot append non branch instruction");

    size_t current = this->set_and_get_current_address(instruction.pointer_address());
    auto node = this->get_current_node(current);
    node->append_branch_instruction(instruction);
    this->append_node_neighbors(node);
    this->unset_current_address(node);
    this->nodes[current] = move(node);
}

void control_flow_graph::unset_current_address(const unique_ptr<Node> &node) noexcept
{
    if (node->done())
        this->current_pointer = 0;
}

size_t control_flow_graph::mem_size() const noexcept
{
    size_t size = sizeof(this->start_address_first_node);
    size += sizeof(size_t); // how many nodes we have
    for (const auto &item : this->nodes) {
        size += sizeof(item.first);
        size += item.second->mem_size();
    }
    return size;
}

void control_flow_graph::set_nodes_max_occurrences() noexcept
{
    unsigned int max = 0;
    for (const auto &item : this->nodes)
        if (item.second->occurrences > max)
            max = item.second->occurrences;

    for (auto &item : this->nodes)
        item.second->max_occurrences = max;
}

bool control_flow_graph::it_fits(const size_t size) const noexcept
{
    return (this->mem_size() <= size);
}
