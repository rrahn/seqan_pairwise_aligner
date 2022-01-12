// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/rrahn/pairwise_aligner/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides seqan::pairwise_aligner::cfg::configure_aligner_saturated.
 * \author Rene Rahn <rahn AT molgen.mpg.de>
 */

#pragma once

#include <iostream>
#include <seqan3/std/concepts>
#include <seqan3/std/type_traits>

#include <seqan3/utility/type_pack/traits.hpp>

#include <pairwise_aligner/affine/affine_initialisation_strategy.hpp>
#include <pairwise_aligner/configuration/rule_category.hpp>
#include <pairwise_aligner/dp_algorithm_template/dp_algorithm_template_saturated.hpp>
#include <pairwise_aligner/utility/type_list.hpp>
#include <pairwise_aligner/dp_trailing_gaps.hpp>

namespace seqan::pairwise_aligner
{
inline namespace v1
{
namespace cfg
{
namespace _configure_aligner_saturated
{

// ----------------------------------------------------------------------------
// configurator
// ----------------------------------------------------------------------------

template <typename ...configurations_t>
struct configurator
{
    class type;
};

template <typename ...configurations_t>
using configurator_t = typename configurator<configurations_t...>::type;

template <typename ...configurations_t>
class configurator<configurations_t...>::type
{
private:

    template <typename ..._configurations_t>
    struct accessor : _configurations_t...
    {

        template <typename configuration_t, cfg::detail::rule_category target_category>
        using is_configuration = std::conditional_t<configuration_t::category == target_category,
                                                    std::true_type,
                                                    std::false_type>;

        template <typename configuration_t>
        using is_score_configuration = is_configuration<configuration_t, cfg::detail::rule_category::score_model>;

        template <typename configuration_t>
        using is_gap_configuration = is_configuration<configuration_t, cfg::detail::rule_category::gap_model>;

        template <typename configuration_t>
        using is_method_configuration = is_configuration<configuration_t, cfg::detail::rule_category::method>;

        // now we need to iterate over list and find_if type
        using substitution_configuration_t =
            typename seqan3::pack_traits::at<seqan3::pack_traits::find_if<is_score_configuration, _configurations_t...>,
                                             _configurations_t...>;

        using gap_configuration_t =
            typename seqan3::pack_traits::at<seqan3::pack_traits::find_if<is_gap_configuration, _configurations_t...>,
                                             _configurations_t...>;


        // using method_configuration_t =
        //     typename seqan3::pack_traits::at<seqan3::pack_traits::find_if<is_gap_configuration, _configurations_t...>,
        //                                      _configurations_t...>;

        using score_type = typename substitution_configuration_t::score_type;

        template <typename score_t>
        using dp_cell_column_type = typename gap_configuration_t::dp_cell_column_type<score_t>;

        template <typename score_t>
        using dp_cell_row_type = typename gap_configuration_t::dp_cell_row_type<score_t>;

    };

    using accessor_t = accessor<configurations_t...>;
    accessor_t _configurations_accessor;

public:

    void set_config(configurations_t && ...configurations) noexcept
    {
        _configurations_accessor = accessor_t{std::move(configurations)...};
    }

    auto configure() const
    {
        using score_model_config_traits_t = typename accessor_t::substitution_configuration_t;
        using gap_model_config_traits_t = typename accessor_t::gap_configuration_t;

        auto substitution_policy = _configurations_accessor.configure_substitution_policy();
        auto gap_policy = _configurations_accessor.configure_gap_policy();
        auto result_factory_policy = _configurations_accessor.configure_result_factory_policy();
        auto dp_vector_policy = _configurations_accessor.configure_dp_vector_policy(_configurations_accessor);

        // TODO: Handle defaults.
        initialisation_rule leading_gap_policy{};
        trailing_gap_setting trailing_gap_policy{};

        using dp_kernel_t = typename gap_model_config_traits_t::dp_kernel_type<dp_algorithm_template_saturated,
                                                                               decltype(substitution_policy),
                                                                               decltype(result_factory_policy),
                                                                               decltype(dp_vector_policy)>;

        using dp_vector_column_t = decltype(dp_vector_policy.column_vector());
        using dp_vector_row_t = decltype(dp_vector_policy.row_vector());
        using aligner_t = typename score_model_config_traits_t::dp_interface_type<dp_kernel_t,
                                                                                  dp_vector_column_t,
                                                                                  dp_vector_row_t>;

        return aligner_t{std::move(substitution_policy),
                         std::move(result_factory_policy),
                         std::move(dp_vector_policy),
                         std::move(gap_policy),
                         std::move(leading_gap_policy),
                         std::move(trailing_gap_policy)};
    }
};

// ----------------------------------------------------------------------------
// implementation
// ----------------------------------------------------------------------------

template <typename predecessor_t>
inline constexpr auto _impl(predecessor_t && predecessor)
{
    // Define the value types.
    using pure_predecessor_t = std::remove_cvref_t<predecessor_t>;
    using aligner_configurator_t = typename pure_predecessor_t::template configurator_types<configurator_t>;

    // Get configuration traits positions.
    constexpr size_t score_model_position = pure_predecessor_t::configurator_index_of[0];
    constexpr size_t gap_model_position = pure_predecessor_t::configurator_index_of[1];

    static_assert(score_model_position != -1, "The score model category is required!");
    static_assert(gap_model_position != -1, "The gap model category is required!");

    // Initialise the configurator with the respective types.
    aligner_configurator_t aligner_configurator{};
    auto operation = std::forward<predecessor_t>(predecessor).apply(aligner_configurator);
    operation.start();

    return aligner_configurator.configure();
}

// ----------------------------------------------------------------------------
// CPO
// ----------------------------------------------------------------------------

namespace _cpo
{

struct _fn
{
    template <typename predecessor_t>
    auto operator()(predecessor_t && predecessor) const noexcept
    {
        return _configure_aligner_saturated::_impl(std::forward<predecessor_t>(predecessor));
    }
};

} // namespace _cpo
} // namespace _configure_aligner_saturated

inline constexpr _configure_aligner_saturated::_cpo::_fn configure_aligner_saturated{};

} // namespace cfg
} // inline namespace v1
}  // namespace seqan::pairwise_aligner
