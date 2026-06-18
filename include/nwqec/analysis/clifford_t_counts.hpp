#pragma once

#include "nwqec/core/circuit.hpp"
#include "nwqec/core/constants.hpp"
#include "nwqec/core/transpiler.hpp"
#include "nwqec/gridsynth/gridsynth.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NWQEC
{

    namespace detail
    {
        struct CliffordTCountState
        {
            std::unordered_map<std::string, size_t> gates;
            size_t rz_gates = 0;
        };

        inline std::string angle_to_count_key(double angle, int sig_digits = 4)
        {
            if (angle == 0.0)
            {
                std::string result = "0.";
                result.append(std::max(0, sig_digits - 1), '0');
                return result;
            }

            double abs_angle = std::abs(angle);
            int order = static_cast<int>(std::floor(std::log10(abs_angle)));
            double scaled = abs_angle / std::pow(10, order - (sig_digits - 1));
            int scaled_int = static_cast<int>(std::round(scaled));

            double rounded = scaled_int * std::pow(10, order - (sig_digits - 1));
            if (angle < 0)
                rounded = -rounded;

            std::stringstream ss;
            ss << std::fixed << std::setprecision(std::max(0, (sig_digits - 1) - order)) << rounded;
            return ss.str();
        }

        inline double resolve_rz_count_epsilon(double angle, RzErrorPolicy policy, double epsilon, size_t rz_occurrence_count)
        {
            switch (policy)
            {
            case RzErrorPolicy::PER_GATE:
                return epsilon;
            case RzErrorPolicy::TOTAL:
                if (rz_occurrence_count == 0)
                    return epsilon;
                return epsilon / static_cast<double>(rz_occurrence_count);
            case RzErrorPolicy::RELATIVE:
                return std::abs(angle) * epsilon;
            default:
                throw std::runtime_error("Unknown RZ error policy");
            }
        }

        inline std::string synthesize_rz_count_sequence(double angle, RzErrorPolicy policy, double epsilon, size_t rz_occurrence_count)
        {
            try
            {
                std::string theta = std::to_string(angle);
                double epsilon_abs = resolve_rz_count_epsilon(angle, policy, epsilon, rz_occurrence_count);

                std::ostringstream eps_ss;
                eps_ss.setf(std::ios::scientific);
                eps_ss << std::setprecision(16) << epsilon_abs;
                return gridsynth::gridsynth_gates(theta, eps_ss.str());
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error("Failed to synthesize RZ angle " + std::to_string(angle) + ": " + e.what());
            }
        }

        inline void add_gridsynth_gate_counts(const std::string &gate_sequence,
                                              size_t multiplier,
                                              CliffordTCountState &counts)
        {
            for (char gate : gate_sequence)
            {
                switch (gate)
                {
                case 'X':
                    counts.gates["x"] += multiplier;
                    break;
                case 'Y':
                    counts.gates["y"] += multiplier;
                    break;
                case 'Z':
                    counts.gates["z"] += multiplier;
                    break;
                case 'H':
                    counts.gates["h"] += multiplier;
                    break;
                case 'S':
                    counts.gates["s"] += multiplier;
                    break;
                case 'T':
                    counts.gates["t"] += multiplier;
                    break;
                case 'W':
                    break;
                default:
                    throw std::runtime_error("Unknown gate type in gridsynth sequence: " + std::string(1, gate));
                }
            }
        }
    } // namespace detail

    inline std::unordered_map<std::string, size_t> get_clifford_t_counts(
        const Circuit &input,
        RzErrorPolicy rz_error_policy = DEFAULT_RZ_ERROR_POLICY,
        std::optional<double> rz_error_epsilon = std::nullopt,
        bool keep_ccx = false)
    {
        double epsilon = rz_error_epsilon.value_or(default_rz_error_epsilon(rz_error_policy));
        if (!(epsilon > 0.0) || !std::isfinite(epsilon))
        {
            throw std::invalid_argument("epsilon must be a positive finite number");
        }

        Transpiler transpiler;
        PassConfig config;
        config.keep_ccx = keep_ccx;
        config.rz_error_policy = rz_error_policy;
        config.rz_error_epsilon = epsilon;
        config.silent = true;

        auto circuit = transpiler.execute_passes(
            std::make_unique<Circuit>(input),
            PassSequences::BASIC_PREPROCESSING,
            config);

        detail::CliffordTCountState counts;
        std::vector<double> distinct_angles;
        std::vector<std::string> distinct_angle_keys;
        std::vector<size_t> angle_occurrences;

        for (const auto &operation : circuit->get_operations())
        {
            if (operation.get_type() != Operation::Type::RZ)
            {
                counts.gates[operation.get_type_name()]++;
                continue;
            }

            counts.rz_gates++;
            const auto &params = operation.get_parameters();
            if (params.empty())
            {
                throw std::runtime_error("RZ gate found without an angle parameter");
            }

            double angle = params[0];
            std::string angle_key = detail::angle_to_count_key(angle);
            auto it = std::find(distinct_angle_keys.begin(), distinct_angle_keys.end(), angle_key);
            if (it == distinct_angle_keys.end())
            {
                distinct_angle_keys.push_back(angle_key);
                distinct_angles.push_back(angle);
                angle_occurrences.push_back(1);
            }
            else
            {
                angle_occurrences[static_cast<size_t>(std::distance(distinct_angle_keys.begin(), it))]++;
            }
        }

        for (size_t i = 0; i < distinct_angles.size(); ++i)
        {
            std::string gate_sequence = detail::synthesize_rz_count_sequence(
                distinct_angles[i],
                rz_error_policy,
                epsilon,
                counts.rz_gates);
            detail::add_gridsynth_gate_counts(gate_sequence, angle_occurrences[i], counts);
        }

        return counts.gates;
    }

} // namespace NWQEC
