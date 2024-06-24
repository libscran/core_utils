#ifndef AVERAGE_VECTORS_HPP
#define AVERAGE_VECTORS_HPP

#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>
#include <numeric>

/**
 * @file average_vectors.hpp
 * @brief Average parallel elements across vectors.
 */

namespace scran {

/**
 * @namespace scran::average_vectors
 * @brief Average parallel elements across vectors.
 */
namespace average_vectors {

/**
 * @cond
 */
namespace internal {

template<bool weighted_, typename Stat_, typename Weight_, typename Output_>
void compute(size_t n, std::vector<Stat_*> in, const Weight_* w, Output_* out, bool skip_nan) {
    if (in.empty()) {
        std::fill_n(out, n, std::numeric_limits<Output_>::quiet_NaN());
        return;
    } else if (in.size() == 1) {
        if constexpr(weighted_) {
            if (w[0] == 0) {
                std::fill_n(out, n, std::numeric_limits<Output_>::quiet_NaN());
                return;
            }
        } 
        std::copy(in[0], in[0] + n, out);
        return;
    }

    std::fill_n(out, n, 0);
    std::vector<Weight_> accumulated(skip_nan ? n : 0);

    auto wcopy = w;
    for (auto current : in) {
        auto copy = out;

        if constexpr(weighted_) {
            Weight_ weight = *(wcopy++);
            if (weight == 0) {
                continue;
            }

            // Use the other loop and skip an unnecessary multiplication when the weigth is 1.
            if (weight != 1) { 
                if (skip_nan) {
                    for (size_t i = 0; i < n; ++i, ++current, ++copy) {
                        auto x = *current * weight;
                         if (!std::isnan(x)) {
                            *copy += x; 
                            accumulated[i] += weight;
                         }
                    }
                } else {
                    for (size_t i = 0; i < n; ++i, ++current, ++copy) {
                        *copy += *current * weight;
                    }
                }
                continue;
            }
        }

        if (skip_nan) {
            for (size_t i = 0; i < n; ++i, ++current, ++copy) {
                auto x = *current;
                if (!std::isnan(x)) {
                    *copy += x; 
                    ++accumulated[i];
                }
            }
        } else {
            for (size_t i = 0; i < n; ++i, ++current, ++copy) {
                *copy += *current;
            }
        }
    }

    if (skip_nan) {
        for (size_t i = 0; i < n; ++i, ++out) {
            *out /= accumulated[i];
        }
    } else {
        double denom = 1;
        if constexpr(weighted_) {
            denom /= std::accumulate(w, w + in.size(), 0.0);
        } else {
            denom /= in.size();
        }
        for (size_t i = 0; i < n; ++i, ++out) {
            *out *= denom;
        }
    }
}

}
/**
 * @endcond
 */

/**
 * Average parallel elements across multiple arrays.
 *
 * @tparam Stat_ Type of the input statistic, typically floating point.
 * @tparam Output_ Floating-point output type.
 *
 * @param n Length of each array.
 * @param[in] in Vector of pointers to input arrays of length `n`.
 * @param[out] out Pointer to an output array of length `n`.
 * On completion, `out` is filled with the average of all arrays in `in`.
 * Specifically, each element of `out` is set to the average of the corresponding elements across all `in` arrays.
 * @param skip_nan Whether to check for NaNs.
 * If `true`, NaNs are ignored in the average calculations for each element, at the cost of some efficiency.
 */
template<typename Stat_, typename Output_>
void compute(size_t n, std::vector<Stat_*> in, Output_* out, bool skip_nan) {
    internal::compute<false>(n, std::move(in), static_cast<int*>(NULL), out, skip_nan);
    return;
}

/**
 * Overload of `compute()` that allocates an output vector of averaged values.
 *
 * @tparam Output Floating-point output type.
 * @tparam Stat Type of the input statistic, typically floating point.
 *
 * @param n Length of each array.
 * @param[in] in Vector of pointers to input arrays of length `n`.
 * @param skip_nan Whether to check for NaNs.
 *
 * @return A vector of length `n` is returned, containing the average of all arrays in `in`.
 */
template<typename Output_ = double, typename Stat_>
std::vector<Output_> compute(size_t n, std::vector<Stat_*> in, bool skip_nan) {
    std::vector<Output_> out(n);
    compute(n, std::move(in), out.data(), skip_nan);
    return out;
}

/**
 * Compute a weighted average of parallel elements across multiple arrays.
 *
 * @tparam Stat_ Type of the input statistic, typically floating point.
 * @tparam Weight_ Type of the weight, typically floating point.
 * @tparam Output_ Floating-point output type.
 *
 * @param n Length of each array.
 * @param[in] in Vector of pointers to input arrays of length `n`.
 * @param[in] w Pointer to an array of length equal to `in.size()`, containing the weight to use for each input array.
 * Weights should be non-negative and finite.
 * @param[out] out Pointer to an output array of length `n`.
 * On output, `out` is filled with the weighted average of all arrays in `in`.
 * Specifically, each element of `out` is set to the weighted average of the corresponding elements across all `in` arrays.
 * @param skip_nan Whether to check for NaNs.
 * If `true`, NaNs are ignored in the average calculations for each element, at the cost of some efficiency.
 */
template<typename Stat_, typename Weight_, typename Output_>
void compute_weighted(size_t n, std::vector<Stat_*> in, const Weight_* w, Output_* out, bool skip_nan) {
    if (!in.empty()) {
        bool same = true;
        for (size_t i = 1, end = in.size(); i < end; ++i) {
            if (w[i] != w[0]) {
                same = false;
                break;
            }
        }

        if (same) {
            if (w[0] == 0) {
                std::fill_n(out, n, std::numeric_limits<Output_>::quiet_NaN());
            } else {
                compute(n, std::move(in), out, skip_nan);
            }
            return;
        }
    }

    internal::compute<true>(n, std::move(in), w, out, skip_nan);
    return;
}

/**
 * Overload of `compute_weighted()` that allocates an output vector of averaged values.
 *
 * @tparam Output_ Floating-point output type.
 * @tparam Weight_ Type of the weight, typically floating point.
 * @tparam Stat_ Type of the input statistic, typically floating point.
 *
 * @param n Length of each array.
 * @param[in] in Vector of pointers to input arrays of the same length.
 * @param[in] w Pointer to an array of length equal to `in.size()`, containing the weight to use for each input array.
 * Weights should be non-negative and finite.
 * @param skip_nan Whether to check for NaNs.
 *
 * @return A vector is returned containing with the average of all arrays in `in`.
 */
template<typename Output_ = double, typename Stat_, typename Weight_>
std::vector<Output_> compute_weighted(size_t n, std::vector<Stat_*> in, const Weight_* w, bool skip_nan) {
    std::vector<Output_> out(n);
    compute_weighted(n, std::move(in), w, out.data(), skip_nan);
    return out;
}

}

}

#endif
