#include "test/custom_params.h"
#include "test/fake_definition.h"
#include "test/fake_graph.h"
#include "test/fake_output_streamer.h"
#include "test/plugin_layer.h"
#include "test/plugin_report.h"
#include "test/virtual_memory.h"

#include <goliath.h>
#include <goliath/assembly/instruction.h>
#include <goliath/engine/engine.h>
#include <goliath/graph/graph.h>
#include <goliath/logger/logger.h>

#include <catch2/catch.hpp>

#include <memory>

TEST_CASE("The plugin is assumed to be run in layer 2", "[GetLayer]")
{
    auto layer = GetLayer();
    REQUIRE(layer == PLUGIN_LAYER);
}

TEST_CASE("Test different states of initialisation", "[DBTInit]")
{
    SECTION("When the virtual memory is not initiliased")
    {
        BOOL state = DBTInit();
        REQUIRE_FALSE(state);
    }

    SECTION("When the virtual memory is set but the engine fails to initialise")
    {
        auto vm = virtual_memory();
        engine::custom_creation([](HANDLE file_mapping) -> engine::engine * {
            REQUIRE(file_mapping != nullptr);
            return nullptr;
        });
        BOOL state = DBTInit();
        REQUIRE_FALSE(state);

        engine::custom_creation(nullptr);
    }

    SECTION("When the engine is set but the logger fails to initialise")
    {
        auto vm = virtual_memory();
        vm.enable_log_name();
        std::string logger_name;
        auto fail = [&logger_name](const char *name) -> std::ostream * {
            REQUIRE(name != nullptr);
            logger_name = name;
            return nullptr;
        };
        logger::custom_creation(fail);

        BOOL state = DBTInit();
        REQUIRE_FALSE(state);
        REQUIRE(logger_name == vm.logger_name());
        engine::clean();
        logger::custom_creation(nullptr);
    }

    SECTION("When the engine, logger is set but the control flow graph fails "
            "to initialise")
    {
        auto vm = virtual_memory();
        vm.enable_log_name();
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        graph::custom_creation([]() -> graph::graph * { return nullptr; });

        BOOL state = DBTInit();
        REQUIRE_FALSE(state);

        std::string got = fos.name();
        auto expected = vm.logger_name();
        REQUIRE(got == expected);

        fos.contains(
          "[CFGTrace] DBTInit engine and logger state are initiliased");

        engine::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("When the engine, logger and control flow graph is initialised")
    {
        auto vm = virtual_memory();
        vm.enable_log_name();
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));

        BOOL state = DBTInit();
        REQUIRE(state == TRUE);

        fos.contains(
          "[CFGTrace] DBTInit engine and logger state are initiliased");
        fos.contains("[CFGTrace] Init is called for iteration [1]");

        auto it = vm.iteration_count();
        REQUIRE(it == 1);

        engine::clean();
        logger::clean();
        graph::clean();
        logger::custom_creation(nullptr);
    }

    SECTION("When the state is initialised and the logger is at iteration >0")
    {
        auto vm = virtual_memory();
        vm.enable_log_name();
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));

        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            fk->_read = [](const std::byte *from) { REQUIRE(from != nullptr); };
            return fk;
        });

        // set iteration to be > 0
        vm.set_iteration_count(1);

        BOOL state = DBTInit();
        REQUIRE(state == TRUE);

        fos.contains(
          "[CFGTrace] DBTInit engine and logger state are initiliased");
        fos.contains("[CFGTrace] Init is called for iteration [2]");

        auto it = vm.iteration_count();
        REQUIRE(it == 2);

        // check instance graph
        auto g = graph::instance();
        auto is_fake_graph = dynamic_cast<fake_graph *>(g);
        REQUIRE(is_fake_graph != nullptr);

        engine::clean();
        logger::clean();
        graph::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }
}

TEST_CASE("Test adding instruction in before ", "[DBTBeforeExecute]")
{
    auto vm = virtual_memory();
    vm.enable_log_name();

    SECTION("Call with a simple instruction")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "MOV EAX, EBX", 0XFFAA, 0, 0);
            };

            return fk;
        });
        auto params = std::make_unique<custom_params>(
          0x55232288, "MOV EAX, EBX", NO_BRANCH, 4, 0XFFAA, 0);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBeforeExecute(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with fail graph append")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t iteration) {
                throw std::exception("here be dragons");
            };

            return fk;
        });
        auto params = std::make_unique<custom_params>(
          0x55232288, "MOV EAX, EBX", NO_BRANCH, 4, 0XFFAA, 0);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBeforeExecute(params->get(), layers.get());
        REQUIRE(report == nullptr);

        // check if the returned exception is logged
        fos.contains("here be dragons");

        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with branch instruction")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "CALL 0x5521323", 0x5521323,
                                    0x55232288 + 4, CallType);
            };
            return fk;
        });

        auto params = std::make_unique<custom_params>(
          0x55232288, "CALL 0x5521323", CallType, 4, 0x5521323, 0x55232288 + 4);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBeforeExecute(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with branch instruction that is not call")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "JB 0x5521323", 0x5521323,
                                    0x55232288 + 4, JB);
            };
            return fk;
        });

        auto params = std::make_unique<custom_params>(
          0x55232288, "JB 0x5521323", JB, 4, 0x5521323, 0x55232288 + 4);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBeforeExecute(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with APIReporter layer present")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "CALL 0x5521323 External windows api ",
                                    0, 0x55232288 + 4, 0);
                REQUIRE(i.api_reporter == "External windows api ");
            };
            return fk;
        });

        auto params = std::make_unique<custom_params>(
          0x55232288, "CALL 0x5521323", CallType, 4, 0x5521323, 0x55232288 + 4);
        auto layers =
          plugin_layer({{2, "PluginTwo", nullptr, nullptr},
                        {1, "APIReporter", nullptr, "External windows api "}});

        auto report = DBTBeforeExecute(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    // teardown the state
    engine::clean();
}

TEST_CASE("Test adding instruction in branching", "[DBTBranching]")
{
    auto vm = virtual_memory();
    vm.enable_log_name();

    SECTION("Call with a CALL instruction")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "CALL 0x5521323", 0x5521323,
                                    0x55232288 + 4, CallType);
            };
            return fk;
        });

        auto params = std::make_unique<custom_params>(
          0x55232288, "CALL 0x5521323", CallType, 4, 0x5521323, 0x55232288 + 4);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBranching(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with a LEAVE instruction")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "LEAVE 0x5521323", 0x5521323, 0,
                                    NO_BRANCH);
            };
            return fk;
        });

        auto params = std::make_unique<custom_params>(
          0x55232288, "LEAVE 0x5521323", NO_BRANCH, 2, 0x5521323, 0);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBranching(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with a branch instruction")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                REQUIRE_INSTRUCTION(i, "je 0x5521323", 0x5521323,
                                    0x55232288 + 4, JE);
            };
            return fk;
        });

        auto params = std::make_unique<custom_params>(
          0x55232288, "je 0x5521323", JE, 4, 0x5521323, 0x55232288 + 4);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBranching(params->get(), layers.get());
        REQUIRE(report != nullptr);

        FREE_REPORT(report);
        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    SECTION("Call with fail graph append")
    {
        auto fos = fake_output_streamer();
        logger::custom_creation(std::bind(&fake_output_streamer::writer, &fos,
                                          std::placeholders::_1));
        auto state = logger::initialise("random_logger");
        REQUIRE(state == true);
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            // test if the instruction has been passed correctly
            fk->_append = [](assembly::instruction i, size_t it) {
                throw std::exception("here be dragons");
            };

            return fk;
        });
        auto params = std::make_unique<custom_params>(
          0x55232288, "MOV EAX, EBX", NO_BRANCH, 4, 0XFFAA, 0);
        auto layers = plugin_layer({{1, "PluginOne", nullptr, nullptr},
                                    {2, "PluginTwo", nullptr, nullptr}});

        auto report = DBTBranching(params->get(), layers.get());
        REQUIRE(report == nullptr);

        // check if the returned exception is logged
        fos.contains("here be dragons");

        graph::clean();
        logger::clean();
        logger::custom_creation(nullptr);
        graph::custom_creation(nullptr);
    }

    // teardown the state
    engine::clean();
}

TEST_CASE("Test all states", "[DBTFinish]")
{
    // setup all the state
    auto vm = virtual_memory();
    vm.enable_log_name();
    auto fos = fake_output_streamer();
    logger::custom_creation(
      std::bind(&fake_output_streamer::writer, &fos, std::placeholders::_1));
    auto state = logger::initialise("test_log_finish");
    REQUIRE(state == true);

    SECTION("Test standard cleanup when definitions are not null")
    {
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            fk->_write = [](std::byte *from) { REQUIRE(from != nullptr); };
            fk->_generate = [](definition::FORMAT format) {
                return new fake_definition();
            };
            return fk;
        });

        auto report = DBTFinish();
        REQUIRE(report != nullptr);
        FREE_REPORT(report);

        fos.contains("[CFGTrace] Finish is called at iteration [0]");
    }

    SECTION("Test standard cleanup when definitions are null")
    {
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            fk->_write = [](std::byte *from) { REQUIRE(from != nullptr); };
            return fk;
        });

        auto report = DBTFinish();
        REQUIRE(report != nullptr);
        FREE_REPORT(report);

        fos.contains("[CFGTrace] Finish is called at iteration [0]");
    }

    SECTION("Test standard cleanup when definitions returns an exception")
    {
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            fk->_write = [](std::byte *from) { REQUIRE(from != nullptr); };
            fk->_generate = [](definition::FORMAT format) {
                throw std::logic_error("logic error");
                return nullptr;
            };
            return fk;
        });

        auto report = DBTFinish();
        REQUIRE(report == nullptr);
        fos.contains("[CFGTrace] Finish is called at iteration [0]");
        fos.contains("logic error");
    }

#ifndef NDEBUG
    SECTION("Test standard cleanup when the execution returns an exception")
    {
        graph::custom_creation([]() -> graph::graph * {
            auto fk = new fake_graph();
            fk->_write = [](std::byte *from) { REQUIRE(from != nullptr); };
            fk->_generate = [](definition::FORMAT format) {
                auto def = new fake_definition();
                def->_execute = []() { throw std::logic_error("logic error"); };
                return def;
            };
            return fk;
        });

        auto report = DBTFinish();
        REQUIRE(report == nullptr);
        fos.contains("[CFGTrace] Finish is called at iteration [0]");
        fos.contains("logic error");
    }
#endif

    logger::custom_creation(nullptr);
    graph::custom_creation(nullptr);
}