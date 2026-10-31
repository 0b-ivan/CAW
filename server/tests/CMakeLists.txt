# Keep firmware suites separate because their fixture names overlap.
foreach(suite cardparty shuffle deck_set sessions full_match disconnect_progress sd_pack catalog table history sentence)
  add_executable(test_${suite} game/test_${suite}.cpp)
  target_include_directories(test_${suite} PRIVATE fixtures)
  target_compile_definitions(test_${suite} PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
  target_link_libraries(test_${suite} PRIVATE caw_game caw_warnings doctest::doctest)
  add_test(NAME game.${suite} COMMAND test_${suite})
endforeach()
