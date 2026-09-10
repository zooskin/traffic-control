# Module build order mirrors the dependency rule in
# docs/12_SOFTWARE_ARCHITECTURE.md §16 and CLAUDE.md:
#
#   core
#    ^  domain / map / state / task
#    ^  planning / reservation / priority
#    ^  replanning / deadlock
#    ^  controller
#    ^  adapter
#
# A lower module must never link a higher one. Modules are added here as their
# phase begins; empty directories are intentional placeholders.

add_subdirectory(core)
add_subdirectory(domain)
add_subdirectory(map)
add_subdirectory(state)
add_subdirectory(task)
add_subdirectory(planning)
add_subdirectory(reservation)
add_subdirectory(infrastructure)

# Phase 8   : priority
# Phase 9-10: deadlock
# Phase 11  : controller
# Phase 12  : replanning
# Phase 14  : adapter
