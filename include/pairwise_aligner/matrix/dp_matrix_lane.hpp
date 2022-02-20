// -----------------------------------------------------------------------------------------------------
// Copyright (c) 2006-2021, Knut Reinert & Freie Universität Berlin
// Copyright (c) 2016-2021, Knut Reinert & MPI für molekulare Genetik
// This file may be used, modified and/or redistributed under the terms of the 3-clause BSD-License
// shipped with this file and also available at: https://github.com/rrahn/pairwise_aligner/blob/master/LICENSE.md
// -----------------------------------------------------------------------------------------------------

/*!\file
 * \brief Provides seqan::pairwise_aligner::dp_matrix_lane.
 * \author Rene Rahn <rahn AT molgen.mpg.de>
 */

#pragma once

#include <seqan3/std/ranges>

#include <pairwise_aligner/matrix/dp_matrix_data_handle.hpp>

namespace seqan::pairwise_aligner
{
inline namespace v1
{
namespace dp_matrix {

template <size_t width>
using lane_width_t = std::integral_constant<size_t, width>;

template <typename dp_block_t, size_t lane_width, bool is_last_lane>
struct _lane
{
    class type;
};

template <typename dp_block_t, size_t lane_width, bool is_last_lane>
using lane_t = typename _lane<dp_block_t, lane_width, is_last_lane>::type;

template <typename dp_block_t, size_t lane_width, bool is_last_lane>
class _lane<dp_block_t, lane_width, is_last_lane>::type
{
private:

    using base_block_t = std::remove_reference_t<dp_block_t>;
    using dp_row_value_t = typename base_block_t::row_type::value_type;
    using cached_row_t = std::array<dp_row_value_t, lane_width>;

    dp_block_t _dp_block;
    cached_row_t _cached_row;
    size_t _row_offset;

public:

    type() = delete;
    constexpr explicit type(dp_block_t dp_block, size_t const row_offset) noexcept :
        _dp_block{std::forward<dp_block_t>(dp_block)},
        _row_offset{row_offset + 1}
    {
        if constexpr (!is_last_lane) {
            unroll_load(_cached_row, _dp_block.row(), _row_offset, std::make_index_sequence<width()>());
        } else {
            size_t const end_index = _dp_block.row().size() - _row_offset;
            // std::cout << "last_lane end index = " << end_index << "\n";
            for (size_t i = 0; i < end_index; ++i)
                _cached_row[i] = _dp_block.row()[i + _row_offset];
        }
    }

    ~type() noexcept
    {
        if constexpr (!is_last_lane) {
            unroll_store(_dp_block.row(), _cached_row, _row_offset, std::make_index_sequence<width()>());
        } else {
            size_t const end_index = _dp_block.row().size() - _row_offset;
            for (size_t i = 0; i < end_index; ++i)
                _dp_block.row()[i + _row_offset] = _cached_row[i];
        }
    }

    static constexpr size_t width() noexcept
    {
        return lane_width;
    }

    constexpr size_t size() const noexcept
    {
        return _dp_block.column().size();
    }

    constexpr typename base_block_t::column_type & column() noexcept
    {
        return _dp_block.column();
    }

    constexpr typename base_block_t::column_type const & column() const noexcept
    {
        return _dp_block.column();
    }

    constexpr cached_row_t & row() noexcept
    {
        return _cached_row;
    }

    constexpr cached_row_t const & row() const noexcept
    {
        return _cached_row;
    }

private:
    template <typename cache_t, typename row_vector_t, size_t ...idx>
    constexpr void unroll_load(cache_t & bulk_cache,
                               row_vector_t const & row_vector,
                               size_t const offset,
                               [[maybe_unused]] std::index_sequence<idx...> const & indices) const noexcept
    {
        ((bulk_cache[idx] = row_vector[offset + idx]), ...);
    }

    template <typename row_vector_t, typename cache_t, size_t ...idx>
    constexpr void unroll_store(row_vector_t & row_vector,
                                cache_t const & bulk_cache,
                                size_t const offset,
                                [[maybe_unused]] std::index_sequence<idx...> const & indices) const noexcept
    {
        ((row_vector[offset + idx] = bulk_cache[idx]), ...);
    }
};


namespace cpo {

template <bool is_last_lane>
struct _lane_closure
{
    template <typename dp_block_t, size_t lane_width>
    constexpr auto operator()(dp_block_t && dp_block, size_t const row_offset, lane_width_t<lane_width>) const noexcept
    {
        using dp_lane_t = dp_matrix::lane_t<dp_block_t, lane_width, is_last_lane>;

        return dp_lane_t{std::forward<dp_block_t>(dp_block), row_offset};
    }
};

} // namespace cpo
} // namespace dp_matrxix

inline constexpr dp_matrix::cpo::_lane_closure<false> dp_matrix_lane{};
inline constexpr dp_matrix::cpo::_lane_closure<true> dp_matrix_last_lane{};
} // inline namespace v1
}  // namespace seqan::pairwise_aligner
