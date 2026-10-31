# Keep installation tied to the authored recipe allowlist, not arbitrary WAVs
# in a developer's output folder. Validate current recipes at install time too.
function(gpp_install_sfx_library specs generated)
    set(components SfxAssets)
    if(GPP_PACKAGE_RUNTIME)
        list(APPEND components Runtime)
    endif()
    foreach(component IN LISTS components)
        install(CODE "
            execute_process(COMMAND \"$<TARGET_FILE:gpp_sfxgen>\"
                --all \"${specs}\" --check --require-filename-id
                RESULT_VARIABLE sfx_result OUTPUT_QUIET ERROR_VARIABLE sfx_error)
            if(NOT sfx_result STREQUAL \"0\")
                message(FATAL_ERROR \"SFX library validation failed: \${sfx_error}\")
            endif()
        " COMPONENT ${component})
    endforeach()

    file(GLOB_RECURSE recipes CONFIGURE_DEPENDS "${specs}/*.lua")
    foreach(recipe IN LISTS recipes)
        get_filename_component(id "${recipe}" NAME_WE)
        file(RELATIVE_PATH relative "${specs}" "${recipe}")
        get_filename_component(directory "${relative}" DIRECTORY)
        install(FILES "${recipe}" DESTINATION "data/sounds/specs/${directory}" COMPONENT SfxAssets)
        foreach(component IN LISTS components)
            # Missing generated audio is an error: build gpp_sfx_library first.
            install(FILES "${generated}/${id}.wav"
                DESTINATION data/sounds/generated COMPONENT ${component})
        endforeach()
    endforeach()
endfunction()
