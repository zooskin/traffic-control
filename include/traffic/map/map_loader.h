#pragma once

/// \file
/// Reading and writing map files. docs/03_MAP_GRAPH.md §24 (Map Load, Map Save).
///
/// The format is JSON, which docs/19_TECHNOLOGY_DECISION.md §15 already settles
/// for external data. Map files are hand-edited by site engineers and reviewed
/// like code; a binary format would make a wrong corridor definition impossible
/// to spot in a diff.
///
/// docs/03_MAP_GRAPH.md §22 asks that the physical map and the traffic graph
/// stay separate, so that changing the site does not reach into the traffic
/// algorithms. This file is that boundary: everything above it works on Map,
/// and only this code knows what a map file looks like.
///
/// Loading is strict. A map that parses but is subtly wrong is worse than one
/// that refuses to load, because the error surfaces hours later as a robot that
/// cannot be routed.

#include <filesystem>
#include <string>
#include <string_view>

#include "traffic/core/result.h"
#include "traffic/map/map.h"
#include "traffic/map/map_validator.h"

namespace traffic::map {

/// Why a load failed.
struct LoadError {
    enum class Kind {
        /// The file could not be opened.
        file_not_readable,

        /// The bytes are not JSON.
        malformed_json,

        /// Valid JSON, but not shaped like a map — a missing required field,
        /// or one of the wrong type.
        schema_violation,

        /// A field held a value the domain refuses, such as a negative length.
        invalid_value,

        /// It parsed into a map, and the map did not validate.
        validation_failed,
    };

    Kind kind{Kind::schema_violation};

    /// What went wrong, in a sentence a site engineer can act on.
    std::string message;

    /// Populated when \c kind is \c validation_failed.
    ValidationReport report;

    [[nodiscard]] std::string to_string() const;
};

[[nodiscard]] std::string_view to_string(LoadError::Kind kind) noexcept;

/// Parses a map from JSON text.
[[nodiscard]] core::Result<Map, LoadError> load_map_from_json(std::string_view json);

/// Reads and parses a map file.
[[nodiscard]] core::Result<Map, LoadError> load_map_from_file(const std::filesystem::path& path);

/// Serialises a map back to JSON.
///
/// Round-trips: loading the output produces an equivalent map. That is what
/// lets a map be edited by a tool and still reviewed as text.
[[nodiscard]] std::string save_map_to_json(const Map& map, int indent = 2);

/// Writes a map file.
[[nodiscard]] core::Result<void, LoadError> save_map_to_file(const Map& map,
                                                             const std::filesystem::path& path);

}  // namespace traffic::map
