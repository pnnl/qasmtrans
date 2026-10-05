#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "QASMTransPrimitives.hpp"

namespace QASMTrans::cli
{
    enum class CliCriticalPathMode
    {
        LogProduct,
        Hybrid
    };

    enum class CliRoutingMode
    {
        Sabre,
        ExecWindow
    };

    struct CliConfig
    {
        bool run_with_limit = false;
        IdxType mode = 0;
        std::string mode_name = "ibmq";
        IdxType debug_level = 0;
        std::string output_path = "../data/output/";
        std::string pulse_template_path;
        bool allow_parameterized_merge_candidates = true;
        std::string backendpath;
        std::vector<std::string> input_files;
        bool disable_mapomatic = true;
        bool use_full_fidelity = false;
        CliCriticalPathMode cp_mode = CliCriticalPathMode::LogProduct;
        std::size_t mapomatic_limit = 1000;
        bool enable_1q_opt = false;
        CliRoutingMode routing_mode = CliRoutingMode::Sabre;
        bool force_identity_layout = false;
    };

    enum class ExecutionPipeline
    {
        SingleBaseline,
        SingleEnhanced,
        BatchEnhanced
    };

    ExecutionPipeline select_execution_pipeline(const CliConfig &config);
    int run_single_circuit_pipeline(const CliConfig &config,
                                    ExecutionPipeline pipeline,
                                    double backend_metadata_ms);
    int run_batch_pipeline(const CliConfig &config);
} // namespace QASMTrans::cli
