#pragma once

#include "goliath/assembly/instruction.h"
#include "goliath/definition/generate.h"
#include "goliath/memory/loader.h"
#include "goliath/memory/unloader.h"

#include <functional>

namespace graph
{
/**
 *  graph type defines a assembly instruction graph that
 *  can read and write to a location of memory
 *  and can append multiple instruction
 */
struct graph : public virtual memory::reader,
               public virtual memory::writer,
               public virtual definition::generator {
    virtual ~graph() = default;
    /**
     *  append adds an assembly instruction into the graph representation
     */
    virtual void
    append(assembly::instruction instruction, size_t iteration) = 0;
};

/**
 *  creator type for defining a custom callback
 *  that will be called to customise the creation process
 *  of the internal graph concrete implementation
 */
using creator = std::function<graph *()>;

/**
 * is_initialised returns true if the internal implementation is initialised
 */
bool is_initialised() noexcept;

/**
 *  instance returns the current instance of the graph
 */
graph *instance() noexcept;

/**
 * clean will clean the internal graph implementation
 */
void clean() noexcept;

/**
 *  custom_creation assign a custom behaviour for the
 *  process of creating an internal graph
 */
void custom_creation(creator create) noexcept;

}; // namespace graph