@echo off

if "%~1"=="" (
    FOR /R "%~dp0sources\" %%A IN (*.*) do (
        python "%~dp0\..\..\..\..\..\Python\gen_shader_header.py" "-o%~dp0/generated" "%%A"
    )
) else (
    python "%~dp0\..\..\..\..\..\Python\gen_shader_header.py" %*
)
