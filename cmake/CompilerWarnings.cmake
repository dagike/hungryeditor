# Shared warning flags. Applied to first-party targets only via
# hungryeditor_set_warnings(<target>); vendored third_party code is spared.

function(hungryeditor_set_warnings target)
    if(MSVC)
        set(flags /W4 /permissive-)
        if(HUNGRYEDITOR_WERROR)
            list(APPEND flags /WX)
        endif()
    else()
        set(flags
            -Wall -Wextra -Wpedantic
            -Wshadow -Wnon-virtual-dtor -Wcast-align
            -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion
            -Wnull-dereference -Wdouble-promotion -Wformat=2)
        if(HUNGRYEDITOR_WERROR)
            list(APPEND flags -Werror)
        endif()
    endif()
    target_compile_options(${target} PRIVATE ${flags})
endfunction()
