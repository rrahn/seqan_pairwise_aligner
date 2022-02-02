// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/rrahn/pairwise_aligner/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides seqan::pairwise_aligner::cfg::score_model_matrix.
 * \author Rene Rahn <rahn AT molgen.mpg.de>
 */

#pragma once

#include <array>
#include <seqan3/std/type_traits>
#include <utility>

#include <pairwise_aligner/configuration/initial.hpp>
#include <pairwise_aligner/configuration/rule_score_model.hpp>
#include <pairwise_aligner/dp_algorithm_template/dp_algorithm_template_standard.hpp>
#include <pairwise_aligner/interface/interface_one_to_one_single.hpp>
#include <pairwise_aligner/matrix/dp_vector_policy.hpp>
#include <pairwise_aligner/matrix/dp_vector_rank_transformation.hpp>
#include <pairwise_aligner/matrix/dp_vector_single.hpp>
#include <pairwise_aligner/score_model/score_model_matrix_local.hpp>
#include <pairwise_aligner/score_model/score_model_matrix.hpp>
#include <pairwise_aligner/tracker/tracker_global_scalar.hpp>
#include <pairwise_aligner/tracker/tracker_local_scalar.hpp>
#include <pairwise_aligner/type_traits.hpp>
#include <pairwise_aligner/utility/type_list.hpp>

namespace seqan::pairwise_aligner {
inline namespace v1
{
namespace cfg
{
namespace _score_model_matrix
{

// ----------------------------------------------------------------------------
// traits
// ----------------------------------------------------------------------------

template <typename matrix_t, typename rank_map_t, size_t dimension_v>
struct traits
{
    static constexpr cfg::detail::rule_category category = cfg::detail::rule_category::score_model;

    matrix_t _substituion_matrix;
    rank_map_t _rank_map;

    using score_type = std::tuple_element_t<0, matrix_t>;

    template <bool is_local>
    using score_model_type = std::conditional_t<is_local,
                                                score_model_matrix_local<matrix_t>,
                                                score_model_matrix<matrix_t>>;

    template <typename dp_vector_t>
    using dp_vector_column_type = dp_vector_t;

    template <typename dp_vector_t>
    using dp_vector_row_type = dp_vector_t;
    template <typename dp_algorithm_t>
    using dp_interface_type = interface_one_to_one_single<dp_algorithm_t>;

    template <typename configuration_t>
    constexpr auto configure_substitution_policy([[maybe_unused]] configuration_t const & configuration) const noexcept
    {
        return score_model_type<configuration_t::is_local>{_substituion_matrix};
    }

    template <typename configuration_t>
    constexpr auto configure_result_factory_policy([[maybe_unused]] configuration_t const & configuration)
        const noexcept
    {
        if constexpr (configuration_t::is_local)
            return tracker::local_scalar::factory<score_type>{};
        else
            return tracker::global_scalar::factory{configuration.trailing_gap_setting()};
    }

    template <typename common_configurations_t>
    constexpr auto configure_dp_vector_policy([[maybe_unused]] common_configurations_t const & configuration)
        const noexcept
    {
        using column_cell_t = typename common_configurations_t::dp_cell_column_type<score_type>;
        using row_cell_t = typename common_configurations_t::dp_cell_row_type<score_type>;

        return dp_vector_policy{
                dp_vector_rank_transformation_factory(dp_vector_single<column_cell_t>{}, _rank_map, dimension_v),
                dp_vector_rank_transformation_factory(dp_vector_single<row_cell_t>{}, _rank_map)
            };
    }

    template <typename configuration_t, typename ...policies_t>
    constexpr auto configure_algorithm(configuration_t const &, policies_t && ...policies) const noexcept
    {
        using algorithm_t = typename configuration_t::algorithm_type<dp_algorithm_template_standard,
                                                                     std::remove_cvref_t<policies_t>...>;

        return interface_one_to_one_single<algorithm_t>{algorithm_t{std::move(policies)...}};
    }
};

// ----------------------------------------------------------------------------
// configurator
// ----------------------------------------------------------------------------

template <typename next_configurator_t, typename traits_t>
struct _configurator
{
    struct type;
};

template <typename next_configurator_t, typename traits_t>
using configurator_t = typename _configurator<next_configurator_t, traits_t>::type;

template <typename next_configurator_t, typename traits_t>
struct _configurator<next_configurator_t, traits_t>::type
{
    next_configurator_t _next_configurator;
    traits_t _traits;

    template <typename ...values_t>
    void set_config(values_t && ... values) noexcept
    {
        std::forward<next_configurator_t>(_next_configurator).set_config(std::forward<values_t>(values)..., _traits);
    }
};

// ----------------------------------------------------------------------------
// rule
// ----------------------------------------------------------------------------

template <typename predecessor_t, typename traits_t>
struct _rule
{
    struct type;
};

template <typename predecessor_t, typename traits_t>
using rule = typename _rule<predecessor_t, traits_t>::type;

template <typename predecessor_t, typename traits_t>
struct _rule<predecessor_t, traits_t>::type : cfg::score_model::rule<predecessor_t>
{
    predecessor_t _predecessor;
    traits_t _traits;

    using traits_type = type_list<traits_t>;

    template <template <typename ...> typename type_list_t>
    using configurator_types = typename concat_type_lists_t<configurator_types_t<std::remove_cvref_t<predecessor_t>,
                                                                                 type_list>,
                                                            traits_type>::template apply<type_list_t>;

    template <typename next_configurator_t>
    auto apply(next_configurator_t && next_configurator) const
    {
        return _predecessor.apply(configurator_t<next_configurator_t, traits_t>{
                    std::forward<next_configurator_t>(next_configurator),
                    _traits
                });
    }
};

// ----------------------------------------------------------------------------
// CPO
// ----------------------------------------------------------------------------

namespace _cpo
{
struct _fn
{
    template <typename predecessor_t, typename alphabet_t, typename score_t, size_t dimension>
    constexpr auto operator()(predecessor_t && predecessor,
                              std::array<std::pair<alphabet_t, std::array<score_t, dimension>>, dimension> const &
                                    substitution_matrix) const
    {
        using substitution_matrix_t = std::array<score_t, dimension * dimension>;
        using symbol_rank_map_t = std::array<uint8_t, 256>;

        substitution_matrix_t linear_matrix{};
        symbol_rank_map_t symbol_to_rank_map{};
        symbol_to_rank_map.fill(255);

        for (uint8_t rank = 0; rank < dimension; ++rank) {
            symbol_to_rank_map[static_cast<uint8_t>(substitution_matrix[rank].first)] = rank;
            std::ranges::copy_n(substitution_matrix[rank].second.data(),
                                dimension,
                                linear_matrix.data() + (rank * dimension));
        };

        using traits_t = traits<substitution_matrix_t, symbol_rank_map_t, dimension>;
        return _score_model_matrix::rule<predecessor_t, traits_t>{{},
                                                                   std::forward<predecessor_t>(predecessor),
                                                                   traits_t{std::move(linear_matrix),
                                                                            std::move(symbol_to_rank_map)}};
    }

    template <typename alphabet_t, typename score_t, size_t dimension>
    constexpr auto operator()(std::array<std::pair<alphabet_t, std::array<score_t, dimension>>, dimension> const &
                                substitution_matrix)
        const
    {
        return this->operator()(cfg::initial, substitution_matrix);
    }
};
} // namespace _cpo
} // namespace _score_model

inline constexpr _score_model_matrix::_cpo::_fn score_model_matrix{};

} // namespace cfg
} // inline namespace v1
}  // namespace seqan::pairwise_aligner
