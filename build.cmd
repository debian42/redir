@echo off
REM Statischer Build fuer Unabhaengigkeit (Senior-Style)
cl /nologo /std:c++20 /MT /O2 /EHsc /permissive- /W4 /WX /sdl /analyze mock_org.cpp /Fe:mock_org.exe /link /subsystem:console /OPT:REF,ICF
cl /nologo /std:c++20 /MT /GL /O2 /EHsc /permissive- /W4 /WX /sdl /analyze redir.cpp /Fe:redir.exe /link /subsystem:console /OPT:REF,ICF
del mock_org.obj
del redir.obj
@REM powershell -ExecutionPolicy Bypass -Command "$env:REDIR_ENABLE_REDIR='1'; & '.\run_test.ps1'"
