#ifndef TEST_H
#define TEST_H

#include <vector>
#include <mutex>

extern "C" {
#include "wiredtiger.h"
}

#include "api_const.h"
#include "component.h"
#include "configuration_settings.h"
#include "conn_api.h"
#include "runtime_monitor.h"
#include "timestamp_manager.h"
#include "thread_manager.h"
#include "workload_generator.h"

namespace test_harness {
/*
 * The base class for a test, the standard usage pattern is to just call run().
 */
class test {
    public:
    test(const std::string &config, bool enable_tracking)
    {
        _configuration = new configuration(name, config);
        _workload_generator = new workload_generator(_configuration, enable_tracking);
        _runtime_monitor = new runtime_monitor();
        _timestamp_manager = new timestamp_manager();
        _thread_manager = new thread_manager();
        /*
         * Ordering is not important here, any dependencies between components should be resolved
         * internally by the components.
         */
        _components = {_workload_generator, _timestamp_manager, _runtime_monitor};
    }

    ~test()
    {
        delete _configuration;
        delete _runtime_monitor;
        delete _timestamp_manager;
        delete _thread_manager;
        delete _workload_generator;
        _configuration = nullptr;
        _runtime_monitor = nullptr;
        _timestamp_manager = nullptr;
        _thread_manager = nullptr;
        _workload_generator = nullptr;

        _components.clear();
    }

    /*
     * The primary run function that most tests will be able to utilise without much other code.
     */
    void
    run()
    {
        int64_t duration_seconds;

        duration_seconds = 0;

        /* Set up the test environement. */
        conn_api_open();

        /* Initiate the load stage of each component. */
        for (const auto &it : _components) {
            it->load();
        }

        /* Spawn threads for all component::run() functions. */
        for (const auto &it : _components) {
            _thread_manager->add_thread(&component::run, it);
        }

        /* Sleep duration seconds. */
        testutil_check(_configuration->get_int(DURATION_SECONDS, duration_seconds));
        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

        /* End the test. */
        for (const auto &it : _components) {
            it->finish();
        }
        _thread_manager->join();
        conn_api_close();
    }

    /*
     * Getters for all the major components, used if a test wants more control over the test
     * program.
     */
    workload_generator *
    get_workload_generator()
    {
        return _workload_generator;
    }

    runtime_monitor *
    get_runtime_monitor()
    {
        return _runtime_monitor;
    }

    timestamp_manager *
    get_timestamp_manager()
    {
        return _timestamp_manager;
    }

    thread_manager *
    get_thread_manager()
    {
        return _thread_manager;
    }

    static const std::string name;
    static const std::string default_config;

    private:
    std::vector<component *> _components;
    configuration *_configuration;
    std::mutex mtx;
    runtime_monitor *_runtime_monitor;
    timestamp_manager *_timestamp_manager;
    thread_manager *_thread_manager;
    workload_generator *_workload_generator;
};
} // namespace test_harness

#endif
