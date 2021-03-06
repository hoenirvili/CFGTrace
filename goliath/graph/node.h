#pragma once

#include "goliath/assembly/instruction.h"
#include <string>
#include <vector>

class Node
{
private:
	size_t _start_address = 0;
	size_t _iteration = 0;

	std::vector<assembly::instruction> block;
	bool is_done = false;

	bool it_fits(size_t size) const noexcept;
	bool no_branching() const noexcept;
	std::string graphviz_color() const noexcept;
	std::string gdl_color() const noexcept;
	std::string name() const noexcept;
	std::string graphviz_label() const noexcept;
	std::string gdl_label() const noexcept;
	void already_visited(size_t eip) noexcept;

public:
	size_t max_occurrences = 1;
	size_t true_branch_address = 0;
	size_t false_branch_address = 0;
	unsigned int occurrences = 1;

	void mark_done() noexcept;
	size_t start_address() const noexcept;
	bool done() const noexcept;
	void append_instruction(assembly::instruction instruction,
		size_t iteration) noexcept;
	void append_branch_instruction(assembly::instruction instruction,
		size_t iteration) noexcept;
	std::string graphviz_definition() const;
	std::string gdl_definition() const;
	std::string graphviz_relation() const;
	std::string gdl_relation() const;
	void load_to_memory(std::byte *mem) const noexcept;
	void load_from_memory(const std::byte *mem) noexcept;
	size_t mem_size() const noexcept;
	bool contains_address(size_t eip) const noexcept;
	size_t true_neighbour() const noexcept;
	size_t false_neighbour() const noexcept;
	Node(size_t start_address, size_t iteration)
		: _start_address(start_address), _iteration(iteration)
	{
	}
	Node() = default;
	~Node() = default;
};
